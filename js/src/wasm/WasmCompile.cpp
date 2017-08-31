/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmCompile.h"

#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"

#include "jsprf.h"

#include "jit/ProcessExecutableMemory.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBinaryIterator.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

#define WASM_POLICY_SPEW

#ifdef WASM_POLICY_SPEW
static void
PolicySpew(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#else
# define PolicySpew(...) (void)0
#endif

static bool
DecodeFunctionBody(Decoder& d, ModuleGenerator& mg, uint32_t funcIndex)
{
    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return d.fail("expected number of function body bytes");

    const size_t offsetInModule = d.currentOffset();

    // Skip over the function body; it will be validated by the compilation thread.
    const uint8_t* bodyBegin;
    if (!d.readBytes(bodySize, &bodyBegin))
        return d.fail("function body length too big");

    return mg.compileFuncDef(funcIndex, offsetInModule, bodyBegin, bodyBegin + bodySize);
}

static bool
DecodeCodeSection(Decoder& d, ModuleGenerator& mg, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Code, env, &sectionStart, &sectionSize, "code"))
        return false;

    if (!mg.startFuncDefs())
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (env->numFuncDefs() != 0)
            return d.fail("expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return d.fail("expected function body count");

    if (numFuncDefs != env->numFuncDefs())
        return d.fail("function body count does not match function signature count");

    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
        if (!DecodeFunctionBody(d, mg, env->numFuncImports() + funcDefIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return mg.finishFuncDefs();
}

bool
CompileArgs::initFromContext(JSContext* cx, ScriptedCaller&& scriptedCaller)
{
    baselineEnabled = cx->options().wasmBaseline();

    // For sanity's sake, just use Ion if both compilers are disabled.
    ionEnabled = cx->options().wasmIon() || !cx->options().wasmBaseline();

    // Debug information such as source view or debug traps will require
    // additional memory and permanently stay in baseline code, so we try to
    // only enable it when a developer actually cares: when the debugger tab
    // is open.
    debugEnabled = cx->compartment()->debuggerObservesAsmJS();

    this->scriptedCaller = Move(scriptedCaller);
    return assumptions.initBuildIdFromContext(cx);
}

namespace js {
    extern JS::SystemInformation gSysinfo;
}

enum class SystemClass
{
    DesktopX86,
    DesktopX64,
    DesktopUnknown32,
    DesktopUnknown64,
    MobileX86,
    MobileArm32,
    MobileArm64,
    MobileUnknown32,
    MobileUnknown64
};

// Classify the current system as one among the known classes.  This really
// needs to get our tier-1 systems right.  In cases where there's doubt about
// desktop vs mobile we could also try to use physical memory, core
// configuration, OS details, CPU speed, CPU class, or CPU manufacturer to
// disambiguate.  This would factor into an additional dimension such as
// {low,medium,high}-end.

static SystemClass
Classify()
{
    bool isDesktop;

#if defined(ANDROID) || defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
    isDesktop = false;
#else
    isDesktop = true;
#endif

    if (isDesktop) {
#if defined(JS_CODEGEN_X64)
        return SystemClass::DesktopX64;
#elif defined(JS_CODEGEN_X86)
        return SystemClass::DesktopX86;
#elif defined(JS_64BIT)
        return SystemClass::DesktopUnknown64;
#else
        return SystemClass::DesktopUnknown32;
#endif
    } else {
#if defined(JS_CODEGEN_X86)
        return SystemClass::MobileX86;
#elif defined(JS_CODEGEN_ARM)
        return SystemClass::MobileArm32;
#elif defined(JS_CODEGEN_ARM64)
        return SystemClass::MobileArm64;
#elif defined(JS_64BIT)
        return SystemClass::MobileUnknown64;
#else
        return SystemClass::MobileUnknown32;
#endif
    }
}

static bool
IsDesktop(SystemClass cls)
{
    return cls <= SystemClass::DesktopUnknown64;
}

// An estimate of the local performance relative to the reference system.
// "Performance" should be some measure of clock speed and CPI for a realistic
// instruction mix.
//
// On desktop the reference system is a 16GB late-2013 MacBook Pro; on mobile a
// 2GB late-2014 Nvidia Jetson TK1 Tegra board.  Both remain high-end as of
// late-2017.

static double
RelativePerformance(SystemClass cls)
{
    // Note, cpu max clock frequency is an extremely crude approximation of
    // speed.  The reference MacBook Pro has a significantly lower max frequency
    // than my older AMD tower, but is still a much faster system; the MBP has a
    // slightly lower max frequency than my new high-end Xeon too, but the Xeon
    // can barely keep up, even when tuned to avoid NUMA effects.

    // These values were recorded by running on the respective systems.

    const double referenceDesktopMaxMHz = 2600; // Haswell i7-4850HQ
    const double referenceMobileMaxMHz = 2300;  // Tegra Cortex-A15

    if (gSysinfo.maxCpuClockMHz == 0)
        return 1;

    // Since the reference systems are both quite fast, and we only have CPU
    // clock speed to guide us right now, we will assume no systems are faster
    // than the reference systems, regardless of clock speed.

    double referenceMaxMHz = IsDesktop(cls) ? referenceDesktopMaxMHz : referenceMobileMaxMHz;
    return Min(1.0, gSysinfo.maxCpuClockMHz / referenceMaxMHz);
}

// Various factors for the reference systems.  These are based on empirical
// measurements except where marked "Guess".  Relationships have been found to
// be roughly linear across program sizes, so there's no need to make this
// complicated.
//
// For the "Unknown" classifications we just have to pick a plausible proxy.

// Compilation rates in bytecodes per millisecond.
static const double x64DesktopBytecodesPerMs = 2100;
static const double x86DesktopBytecodesPerMs = x64DesktopBytecodesPerMs / 1.4;
static const double arm32MobileBytecodesPerMs = x64DesktopBytecodesPerMs / 3.3;
static const double x86MobileBytecodesPerMs = x86DesktopBytecodesPerMs / 2.0; // Guess
static const double arm64MobileBytecodesPerMs = x86MobileBytecodesPerMs; // Guess

static double
BytecodesPerMs(SystemClass cls)
{
    switch (cls) {
      case SystemClass::DesktopX86:
      case SystemClass::DesktopUnknown32:
        return x86DesktopBytecodesPerMs;
      case SystemClass::DesktopX64:
      case SystemClass::DesktopUnknown64:
        return x64DesktopBytecodesPerMs;
      case SystemClass::MobileX86:
        return x86MobileBytecodesPerMs;
      case SystemClass::MobileArm32:
      case SystemClass::MobileUnknown32:
        return arm32MobileBytecodesPerMs;
      case SystemClass::MobileArm64:
      case SystemClass::MobileUnknown64:
        return arm64MobileBytecodesPerMs;
      default:
        MOZ_CRASH();
    }
};

#ifndef JS_64BIT

// Code sizes in machine code bytes per bytecode byte, again empirical except
// where marked as "Guess".

static const double x64Tox86Inflation = 1.25;

static const double x64IonBytesPerBytecode = 2.45;
static const double x86IonBytesPerBytecode = x64IonBytesPerBytecode * x64Tox86Inflation;
static const double arm32IonBytesPerBytecode = 3.3;
static const double arm64IonBytesPerBytecode = 3.0; // Guess

static const double x64BaselineBytesPerBytecode = x64IonBytesPerBytecode * 1.43;
static const double x86BaselineBytesPerBytecode = x64BaselineBytesPerBytecode * x64Tox86Inflation;
static const double arm32BaselineBytesPerBytecode = arm32IonBytesPerBytecode * 1.39;
static const double arm64BaselineBytesPerBytecode = arm64IonBytesPerBytecode * 1.39; // Guess

static double
IonBytesPerBytecode(SystemClass cls)
{
    switch (cls) {
      case SystemClass::DesktopX86:
      case SystemClass::MobileX86:
      case SystemClass::DesktopUnknown32:
        return x86IonBytesPerBytecode;
      case SystemClass::DesktopX64:
      case SystemClass::DesktopUnknown64:
        return x64IonBytesPerBytecode;
      case SystemClass::MobileArm32:
      case SystemClass::MobileUnknown32:
        return arm32IonBytesPerBytecode;
      case SystemClass::MobileArm64:
      case SystemClass::MobileUnknown64:
        return arm64IonBytesPerBytecode;
      default:
        MOZ_CRASH();
    }
}

static double
BaselineBytesPerBytecode(SystemClass cls)
{
    switch (cls) {
      case SystemClass::DesktopX86:
      case SystemClass::MobileX86:
      case SystemClass::DesktopUnknown32:
        return x86BaselineBytesPerBytecode;
      case SystemClass::DesktopX64:
      case SystemClass::DesktopUnknown64:
        return x64BaselineBytesPerBytecode;
      case SystemClass::MobileArm32:
      case SystemClass::MobileUnknown32:
        return arm32BaselineBytesPerBytecode;
      case SystemClass::MobileArm64:
      case SystemClass::MobileUnknown64:
        return arm64BaselineBytesPerBytecode;
      default:
        MOZ_CRASH();
    }
}
#endif  // !JS_64BIT

static uint32_t
MillisToIonCompileOnThisSystem(SystemClass cls, uint32_t codeSize)
{
    return Max(1U, uint32_t(codeSize / BytecodesPerMs(cls) / RelativePerformance(cls)));
}

static double
EffectiveCores(SystemClass cls, uint32_t cores)
{
    // cores^0.667 is crude but more realistic than cores^1, and probably not
    // far off for the systems that run browsers (most have 2-4 hyperthreaded
    // cores).
    //
    // To do better, we could examine the actual architecture: hyperthreads vs
    // real cores, sizes of caches, architecture class.

    return pow(cores, 2.0/3.0);
}

// Figure out whether we should use tiered compilation or not.
static bool
GetTieringEnabled(uint32_t codeSize)
{
    // If we can reasonably compile in less than 250ms, don't enable tiering.
    const uint32_t timeCutoffMs = 250;

#ifndef JS_64BIT
    // If tiering will fill code memory to more than 90%, don't enable it.
    const double spaceCutoffPct = 0.9;
#endif

    if (!CanUseExtraThreads()) {
        PolicySpew("Tiering disabled : background-work-disabled\n");
        return false;
    }

    uint32_t cpuCount = HelperThreadState().cpuCount;
    MOZ_ASSERT(cpuCount > 0);

    // TODO: Investigate the multicore-required limitation.  "It's always been
    // this way" is insufficient justification.
    //
    // It's mostly sensible not to background compile when there's only one
    // hardware thread as we want foreground computation to have access to that,
    // but if wasm background compilation helper threads can be given lower
    // priority then background compilation on single-core systems still makes
    // some kind of sense.
    //
    // On desktop this is really a non-problem though, as of September 2017
    // 1-core was down to 3.5% of our population and falling.

    if (cpuCount == 1) {
        PolicySpew("Tiering disabled : single-core\n");
        return false;
    }

    DebugOnly<uint32_t> threadCount = HelperThreadState().threadCount;
    MOZ_ASSERT(threadCount >= cpuCount);

    // Compute the max number of threads available to do actual background
    // compilation work.
    //
    // TODO: Here we can play around with the threadCount we /need/ rather than
    // the threadCount we /have/, and feed the number we need into the
    // scheduler.  That requires a more sophisticated scheduler, though.
    //
    // TODO: Here we might also worry about the threadCount that is currently
    // available, given what is going on elsewhere in the pipeline.  In
    // principle maxWasmCompilationThreads can encapsulate that, but it doesn't.

    uint32_t workers = HelperThreadState().maxWasmCompilationThreads();

    // The number of cores we will use is bounded both by the CPU count and the
    // worker count.

    uint32_t cores = Min(cpuCount, workers);

    SystemClass cls = Classify();

    PolicySpew("Tiering system-info : class %d code-size %u bytecodes-per-ms %g relative-speed %g clock-mhz %u\n",
               uint32_t(cls), codeSize, BytecodesPerMs(cls), RelativePerformance(cls),
               gSysinfo.maxCpuClockMHz);

    // Ion compilation on available cores must take long enough to be worth the
    // bother.  Keep it simple and divide estimated ion time by the number of
    // cores, don't worry about sequential sections or parallel overhead.

    uint32_t ms = MillisToIonCompileOnThisSystem(cls, codeSize);

    // Sadly, linear speedup is not a reality, so try to get a grip on what
    // speedup is realistic on this system.

    double effective_cores = EffectiveCores(cls, cores);

    PolicySpew("Tiering estimation : single-core-ion-est %u num-cores %u effective-cores %g parallel-est %g\n",
               ms, cores, effective_cores, ms / effective_cores);

    if ((ms / effective_cores) < timeCutoffMs) {
        PolicySpew("Tiering disabled : ion-fast-enough\n");
        return false;
    }

    // Do not implement a size cutoff for 64-bit systems since the code size
    // budget for 64 bit is so large that it will hardly ever be an issue.
    // (Also the cutoff percentage might be different on 64-bit.)

#ifndef JS_64BIT
    // If the amount of executable code for baseline compilation jeopardizes the
    // availability of executable memory for ion code then do not tier, for now.
    //
    // TODO: For now we consider this module in isolation.  We should really
    // worry about what else is going on in this process and might be filling up
    // the code memory.

    double ionRatio = IonBytesPerBytecode(cls);
    double baselineRatio = BaselineBytesPerBytecode(cls);
    uint64_t needMemory = codeSize * (ionRatio + baselineRatio);
    uint64_t availMemory = LikelyAvailableExecutableMemory();
    double cutoff = spaceCutoffPct * MaxCodeBytesPerProcess;

    PolicySpew("Tiering memory-info : code-bytes %u ion-blowup %g baseline-blowup %g need %" PRIu64
               " max %" PRIu64 " avail %" PRIu64 " cutoff %" PRIu64 "\n",
               codeSize, ionRatio, baselineRatio, needMemory, uint64_t(MaxCodeBytesPerProcess),
               availMemory, uint64_t(cutoff));

    // If the sum of baseline and ion code makes us exceeds some set percentage
    // of the executable memory then disable tiering.

    if ((MaxCodeBytesPerProcess - availMemory) + needMemory > cutoff) {
        PolicySpew("Tiering disabled : memory-budget\n");
        return false;
    }
#endif

    PolicySpew("Tiering enabled : all-tests-pass\n");
    return true;
}

SharedModule
wasm::CompileInitialTier(const ShareableBytes& bytecode, const CompileArgs& args, UniqueChars* error)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    bool baselineEnabled = BaselineCanCompile() && args.baselineEnabled;
    bool debugEnabled = BaselineCanCompile() && args.debugEnabled;
    bool ionEnabled = args.ionEnabled || !baselineEnabled;

    DebugEnabled debug = debugEnabled ? DebugEnabled::True : DebugEnabled::False;

    ModuleEnvironment env(ModuleEnvironment::UnknownMode, ModuleEnvironment::UnknownTier, debug);

    Decoder d(bytecode.bytes, error);
    if (!DecodeModuleEnvironment(d, &env))
        return nullptr;

    uint32_t codeSize;
    if (!d.peekSectionSize(SectionId::Code, &env, "code", &codeSize))
        codeSize = 0;

    CompileMode mode;
    Tier tier;
    if (baselineEnabled && ionEnabled && !debugEnabled && GetTieringEnabled(codeSize)) {
        mode = CompileMode::Tier1;
        tier = Tier::Baseline;
    } else {
        mode = CompileMode::Once;
        tier = debugEnabled || !ionEnabled ? Tier::Baseline : Tier::Ion;
    }

    env.setModeAndTier(mode, tier);

    ModuleGenerator mg(args, &env, nullptr, error);
    if (!mg.init())
        return nullptr;

#ifdef WASM_POLICY_SPEW
    uint64_t before = PRMJ_Now();
#endif

    if (!DecodeCodeSection(d, mg, &env))
        return nullptr;

#ifdef WASM_POLICY_SPEW
    uint64_t after = PRMJ_Now();
    if (mode == CompileMode::Tier1) {
        PolicySpew("Tiering tier-1 : baseline-parallel-time %" PRIu64 "\n", (after - before) / PRMJ_USEC_PER_MSEC);
    } else {
        PolicySpew("Tiering once : %s-parallel-time %" PRIu64 "\n",
                   tier == Tier::Baseline ? "baseline" : "ion",
                   (after - before) / PRMJ_USEC_PER_MSEC);
    }
#endif

    if (!DecodeModuleTail(d, &env))
        return nullptr;

    return mg.finishModule(bytecode);
}

bool
wasm::CompileTier2(Module& module, const CompileArgs& args, Atomic<bool>* cancelled)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    UniqueChars error;
    Decoder d(module.bytecode().bytes, &error);

    ModuleEnvironment env(CompileMode::Tier2, Tier::Ion, DebugEnabled::False);
    if (!DecodeModuleEnvironment(d, &env))
        return false;

    ModuleGenerator mg(args, &env, cancelled, &error);
    if (!mg.init())
        return false;

#ifdef WASM_POLICY_SPEW
    uint64_t before = PRMJ_Now();
#endif

    if (!DecodeCodeSection(d, mg, &env))
        return false;

#ifdef WASM_POLICY_SPEW
    uint64_t after = PRMJ_Now();
    PolicySpew("Tiering tier-2 : ion-background-time %" PRIu64 "\n", (after - before) / PRMJ_USEC_PER_MSEC);
#endif

    if (!DecodeModuleTail(d, &env))
        return false;

    return mg.finishTier2(module);
}
