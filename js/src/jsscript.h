/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS script descriptor. */

#ifndef jsscript_h
#define jsscript_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include "jsatom.h"
#ifdef JS_THREADSAFE
#include "jslock.h"
#endif
#include "jsobj.h"
#include "jsopcode.h"

#include "gc/Barrier.h"
#include "vm/Shape.h"

namespace js {

namespace jit {
    struct IonScript;
    struct BaselineScript;
    struct IonScriptCounts;
}

# define ION_DISABLED_SCRIPT ((js::jit::IonScript *)0x1)
# define ION_COMPILING_SCRIPT ((js::jit::IonScript *)0x2)

# define BASELINE_DISABLED_SCRIPT ((js::jit::BaselineScript *)0x1)

class BreakpointSite;
class BindingIter;
class RegExpObject;
struct SourceCompressionToken;
class Shape;
class WatchpointMap;

namespace analyze {
    class ScriptAnalysis;
}

namespace frontend {
    class BytecodeEmitter;
}

}

/*
 * Type of try note associated with each catch or finally block, and also with
 * for-in and other kinds of loops. Non-for-in loops do not need these notes
 * for exception unwinding, but storing their boundaries here is helpful for
 * heuristics that need to know whether a given op is inside a loop.
 */
typedef enum JSTryNoteKind {
    JSTRY_CATCH,
    JSTRY_FINALLY,
    JSTRY_ITER,
    JSTRY_LOOP
} JSTryNoteKind;

/*
 * Exception handling record.
 */
struct JSTryNote {
    uint8_t         kind;       /* one of JSTryNoteKind */
    uint8_t         padding;    /* explicit padding on uint16_t boundary */
    uint16_t        stackDepth; /* stack depth upon exception handler entry */
    uint32_t        start;      /* start of the try statement or loop
                                   relative to script->main */
    uint32_t        length;     /* length of the try statement or loop */
};

namespace js {

struct ConstArray {
    js::HeapValue   *vector;    /* array of indexed constant values */
    uint32_t        length;
};

struct ObjectArray {
    js::HeapPtrObject *vector;  /* array of indexed objects */
    uint32_t        length;     /* count of indexed objects */
};

struct TryNoteArray {
    JSTryNote       *vector;    /* array of indexed try notes */
    uint32_t        length;     /* count of indexed try notes */
};

/*
 * A "binding" is a formal, 'var' or 'const' declaration. A function's lexical
 * scope is composed of these three kinds of bindings.
 */

enum BindingKind { ARGUMENT, VARIABLE, CONSTANT };

class Binding
{
    /*
     * One JSScript stores one Binding per formal/variable so we use a
     * packed-word representation.
     */
    uintptr_t bits_;

    static const uintptr_t KIND_MASK = 0x3;
    static const uintptr_t ALIASED_BIT = 0x4;
    static const uintptr_t NAME_MASK = ~(KIND_MASK | ALIASED_BIT);

  public:
    explicit Binding() : bits_(0) {}

    Binding(PropertyName *name, BindingKind kind, bool aliased) {
        JS_STATIC_ASSERT(CONSTANT <= KIND_MASK);
        JS_ASSERT((uintptr_t(name) & ~NAME_MASK) == 0);
        JS_ASSERT((uintptr_t(kind) & ~KIND_MASK) == 0);
        bits_ = uintptr_t(name) | uintptr_t(kind) | (aliased ? ALIASED_BIT : 0);
    }

    PropertyName *name() const {
        return (PropertyName *)(bits_ & NAME_MASK);
    }

    BindingKind kind() const {
        return BindingKind(bits_ & KIND_MASK);
    }

    bool aliased() const {
        return bool(bits_ & ALIASED_BIT);
    }
};

JS_STATIC_ASSERT(sizeof(Binding) == sizeof(uintptr_t));

class Bindings;
typedef InternalHandle<Bindings *> InternalBindingsHandle;

/*
 * Formal parameters and local variables are stored in a shape tree
 * path encapsulated within this class.  This class represents bindings for
 * both function and top-level scripts (the latter is needed to track names in
 * strict mode eval code, to give such code its own lexical environment).
 */
class Bindings
{
    friend class BindingIter;
    friend class AliasedFormalIter;

    HeapPtr<Shape> callObjShape_;
    uintptr_t bindingArrayAndFlag_;
    uint16_t numArgs_;
    uint16_t numVars_;

    /*
     * During parsing, bindings are allocated out of a temporary LifoAlloc.
     * After parsing, a JSScript object is created and the bindings are
     * permanently transferred to it. On error paths, the JSScript object may
     * end up with bindings that still point to the (new released) LifoAlloc
     * memory. To avoid tracing these bindings during GC, we keep track of
     * whether the bindings are temporary or permanent in the low bit of
     * bindingArrayAndFlag_.
     */
    static const uintptr_t TEMPORARY_STORAGE_BIT = 0x1;
    bool bindingArrayUsingTemporaryStorage() const {
        return bindingArrayAndFlag_ & TEMPORARY_STORAGE_BIT;
    }
    Binding *bindingArray() const {
        return reinterpret_cast<Binding *>(bindingArrayAndFlag_ & ~TEMPORARY_STORAGE_BIT);
    }

  public:
    inline Bindings();

    /*
     * Initialize a Bindings with a pointer into temporary storage.
     * bindingArray must have length numArgs+numVars. Before the temporary
     * storage is release, switchToScriptStorage must be called, providing a
     * pointer into the Binding array stored in script->data.
     */
    static bool initWithTemporaryStorage(ExclusiveContext *cx, InternalBindingsHandle self,
                                         unsigned numArgs, unsigned numVars,
                                         Binding *bindingArray);

    uint8_t *switchToScriptStorage(Binding *newStorage);

    /*
     * Clone srcScript's bindings (as part of js::CloneScript). dstScriptData
     * is the pointer to what will eventually be dstScript->data.
     */
    static bool clone(JSContext *cx, InternalBindingsHandle self, uint8_t *dstScriptData,
                      HandleScript srcScript);

    unsigned numArgs() const { return numArgs_; }
    unsigned numVars() const { return numVars_; }
    unsigned count() const { return numArgs() + numVars(); }

    /* Return the initial shape of call objects created for this scope. */
    Shape *callObjShape() const { return callObjShape_; }

    /* Convenience method to get the var index of 'arguments'. */
    static unsigned argumentsVarIndex(ExclusiveContext *cx, InternalBindingsHandle);

    /* Return whether the binding at bindingIndex is aliased. */
    bool bindingIsAliased(unsigned bindingIndex);

    /* Return whether this scope has any aliased bindings. */
    bool hasAnyAliasedBindings() const { return callObjShape_ && !callObjShape_->isEmptyShape(); }

    void trace(JSTracer *trc);
};

template <>
struct GCMethods<Bindings> {
    static Bindings initial();
    static ThingRootKind kind() { return THING_ROOT_BINDINGS; }
    static bool poisoned(const Bindings &bindings) {
        return IsPoisonedPtr(static_cast<Shape *>(bindings.callObjShape()));
    }
};

class ScriptCounts
{
    friend class ::JSScript;
    friend struct ScriptAndCounts;

    /*
     * This points to a single block that holds an array of PCCounts followed
     * by an array of doubles.  Each element in the PCCounts array has a
     * pointer into the array of doubles.
     */
    PCCounts *pcCountsVector;

    /* Information about any Ion compilations for the script. */
    jit::IonScriptCounts *ionCounts;

 public:
    ScriptCounts() : pcCountsVector(NULL), ionCounts(NULL) { }

    inline void destroy(FreeOp *fop);

    void set(js::ScriptCounts counts) {
        pcCountsVector = counts.pcCountsVector;
        ionCounts = counts.ionCounts;
    }
};

typedef HashMap<JSScript *,
                ScriptCounts,
                DefaultHasher<JSScript *>,
                SystemAllocPolicy> ScriptCountsMap;

class DebugScript
{
    friend class ::JSScript;

    /*
     * When non-zero, compile script in single-step mode. The top bit is set and
     * cleared by setStepMode, as used by JSD. The lower bits are a count,
     * adjusted by changeStepModeCount, used by the Debugger object. Only
     * when the bit is clear and the count is zero may we compile the script
     * without single-step support.
     */
    uint32_t        stepMode;

    /* Number of breakpoint sites at opcodes in the script. */
    uint32_t        numSites;

    /*
     * Array with all breakpoints installed at opcodes in the script, indexed
     * by the offset of the opcode into the script.
     */
    BreakpointSite  *breakpoints[1];
};

typedef HashMap<JSScript *,
                DebugScript *,
                DefaultHasher<JSScript *>,
                SystemAllocPolicy> DebugScriptMap;

class ScriptSource
{
    friend class SourceCompressorThread;
    union {
        // Before setSourceCopy or setSource are successfully called, this union
        // has a NULL pointer. When the script source is ready,
        // compressedLength_ != 0 implies compressed holds the compressed data;
        // otherwise, source holds the uncompressed source. There is a special
        // pointer |emptySource| for source code for length 0.
        //
        // The only function allowed to malloc, realloc, or free the pointers in
        // this union is adjustDataSize(). Don't do it elsewhere.
        jschar *source;
        unsigned char *compressed;
    } data;
    uint32_t refs;
    uint32_t length_;
    uint32_t compressedLength_;
    char *filename_;
    jschar *sourceMap_;
    JSPrincipals *originPrincipals_;

    // True if we can call JSRuntime::sourceHook to load the source on
    // demand. If sourceRetrievable_ and hasSourceData() are false, it is not
    // possible to get source at all.
    bool sourceRetrievable_:1;
    bool argumentsNotIncluded_:1;
    bool ready_:1;

  public:
    ScriptSource(JSPrincipals *originPrincipals)
      : refs(0),
        length_(0),
        compressedLength_(0),
        filename_(NULL),
        sourceMap_(NULL),
        originPrincipals_(originPrincipals),
        sourceRetrievable_(false),
        argumentsNotIncluded_(false),
        ready_(true)
    {
        data.source = NULL;
        if (originPrincipals_)
            JS_HoldPrincipals(originPrincipals_);
    }
    void incref() { refs++; }
    void decref() {
        JS_ASSERT(refs != 0);
        if (--refs == 0)
            destroy();
    }
    bool setSourceCopy(JSContext *cx,
                       const jschar *src,
                       uint32_t length,
                       bool argumentsNotIncluded,
                       SourceCompressionToken *tok);
    void setSource(const jschar *src, uint32_t length);
    bool ready() const { return ready_; }
    void setSourceRetrievable() { sourceRetrievable_ = true; }
    bool sourceRetrievable() const { return sourceRetrievable_; }
    bool hasSourceData() const { return !!data.source || !ready(); }
    uint32_t length() const {
        JS_ASSERT(hasSourceData());
        return length_;
    }
    bool argumentsNotIncluded() const {
        JS_ASSERT(hasSourceData());
        return argumentsNotIncluded_;
    }
    const jschar *chars(JSContext *cx);
    JSStableString *substring(JSContext *cx, uint32_t start, uint32_t stop);
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    // XDR handling
    template <XDRMode mode>
    bool performXDR(XDRState<mode> *xdr);

    bool setFilename(ExclusiveContext *cx, const char *filename);
    const char *filename() const {
        return filename_;
    }

    // Source maps
    bool setSourceMap(ExclusiveContext *cx, jschar *sourceMapURL);
    const jschar *sourceMap();
    bool hasSourceMap() const { return sourceMap_ != NULL; }

    JSPrincipals *originPrincipals() const { return originPrincipals_; }

  private:
    void destroy();
    bool compressed() const { return compressedLength_ != 0; }
    size_t computedSizeOfData() const {
        return compressed() ? compressedLength_ : sizeof(jschar) * length_;
    }
    bool adjustDataSize(size_t nbytes);
};

class ScriptSourceHolder
{
    ScriptSource *ss;
  public:
    explicit ScriptSourceHolder(ScriptSource *ss)
      : ss(ss)
    {
        ss->incref();
    }
    ~ScriptSourceHolder()
    {
        ss->decref();
    }
};

class ScriptSourceObject : public JSObject
{
  public:
    static Class class_;

    static void finalize(FreeOp *fop, JSObject *obj);
    static ScriptSourceObject *create(ExclusiveContext *cx, ScriptSource *source);

    ScriptSource *source() {
        return static_cast<ScriptSource *>(getReservedSlot(SOURCE_SLOT).toPrivate());
    }

    void setSource(ScriptSource *source);

  private:
    static const uint32_t SOURCE_SLOT = 0;
};

enum GeneratorKind { NotGenerator, LegacyGenerator, StarGenerator };

static inline unsigned
GeneratorKindAsBits(GeneratorKind generatorKind) {
    return static_cast<unsigned>(generatorKind);
}

static inline GeneratorKind
GeneratorKindFromBits(unsigned val) {
    JS_ASSERT(val <= StarGenerator);
    return static_cast<GeneratorKind>(val);
}

} /* namespace js */

class JSScript : public js::gc::Cell
{
    static const uint32_t stepFlagMask = 0x80000000U;
    static const uint32_t stepCountMask = 0x7fffffffU;

  public:
    //
    // We order fields according to their size in order to avoid wasting space
    // for alignment.
    //

    // Larger-than-word-sized fields.

  public:
    js::Bindings    bindings;   /* names of top-level variables in this script
                                   (and arguments if this is a function script) */

    // Word-sized fields.

  public:
    jsbytecode      *code;      /* bytecodes and their immediate operands */
    uint8_t         *data;      /* pointer to variable-length data array (see
                                   comment above Create() for details) */

    js::HeapPtrAtom *atoms;     /* maps immediate index to literal struct */

    JSCompartment   *compartment_;

    /* Persistent type information retained across GCs. */
    js::types::TypeScript *types;

  private:
    js::HeapPtrObject sourceObject_; /* source code object */
    js::HeapPtrFunction function_;

    // For callsite clones, which cannot have enclosing scopes, the original
    // function; otherwise the enclosing scope
    js::HeapPtrObject   enclosingScopeOrOriginalFunction_;

    /* Information attached by Baseline/Ion for sequential mode execution. */
    js::jit::IonScript *ion;
    js::jit::BaselineScript *baseline;

    /* Information attached by Ion for parallel mode execution */
    js::jit::IonScript *parallelIon;

    /*
     * Pointer to either baseline->method()->raw() or ion->method()->raw(), or NULL
     * if there's no Baseline or Ion script.
     */
    uint8_t *baselineOrIonRaw;
    uint8_t *baselineOrIonSkipArgCheck;

    // 32-bit fields.

  public:
    uint32_t        length;     /* length of code vector */

    uint32_t        dataSize;   /* size of the used part of the data array */

    uint32_t        lineno;     /* base line number of script */
    uint32_t        column;     /* base column of script, optionally set */

    uint32_t        mainOffset; /* offset of main entry point from code, after
                                   predef'ing prolog */

    uint32_t        natoms;     /* length of atoms array */

    /* Range of characters in scriptSource which contains this script's source. */
    uint32_t        sourceStart;
    uint32_t        sourceEnd;

  private:
    uint32_t        useCount;   /* Number of times the script has been called
                                 * or has had backedges taken. Reset if the
                                 * script's JIT code is forcibly discarded. */

#ifdef DEBUG
    // Unique identifier within the compartment for this script, used for
    // printing analysis information.
    uint32_t        id_;
  private:
    uint32_t        idpad;
#endif

    // 16-bit fields.

  private:
    uint16_t        PADDING16;
    uint16_t        version;    /* JS version under which script was compiled */

  public:
    uint16_t        funLength;  /* ES6 function length */

    uint16_t        nfixed;     /* number of slots besides stack operands in
                                   slot array */

    uint16_t        nTypeSets;  /* number of type sets used in this script for
                                   dynamic type monitoring */

    uint16_t        nslots;     /* vars plus maximum stack depth */
    uint16_t        staticLevel;/* static level for display maintenance */

    // 4-bit fields.

  public:
    // The kinds of the optional arrays.
    enum ArrayKind {
        CONSTS,
        OBJECTS,
        REGEXPS,
        TRYNOTES,
        ARRAY_KIND_BITS
    };

  private:
    // The bits in this field indicate the presence/non-presence of several
    // optional arrays in |data|.  See the comments above Create() for details.
    uint8_t         hasArrayBits:4;

    // The GeneratorKind of the script.
    uint8_t         generatorKindBits_:4;

    // 1-bit fields.

  public:
    bool            noScriptRval:1; /* no need for result value of last
                                       expression statement */
    bool            savedCallerFun:1; /* can call getCallerFunction() */
    bool            strict:1; /* code is in strict mode */
    bool            explicitUseStrict:1; /* code has "use strict"; explicitly */
    bool            compileAndGo:1;   /* see Parser::compileAndGo */
    bool            selfHosted:1;     /* see Parser::selfHostingMode */
    bool            bindingsAccessedDynamically:1; /* see FunctionContextFlags */
    bool            funHasExtensibleScope:1;       /* see FunctionContextFlags */
    bool            funNeedsDeclEnvObject:1;       /* see FunctionContextFlags */
    bool            funHasAnyAliasedFormal:1;      /* true if any formalIsAliased(i) */
    bool            warnedAboutTwoArgumentEval:1; /* have warned about use of
                                                     obsolete eval(s, o) in
                                                     this script */
    bool            warnedAboutUndefinedProp:1; /* have warned about uses of
                                                   undefined properties in this
                                                   script */
    bool            hasSingletons:1;  /* script has singleton objects */
    bool            treatAsRunOnce:1; /* script is a lambda to treat as running once. */
    bool            hasRunOnce:1;     /* if treatAsRunOnce, whether script has executed. */
    bool            hasBeenCloned:1;  /* script has been reused for a clone. */
    bool            isActiveEval:1;   /* script came from eval(), and is still active */
    bool            isCachedEval:1;   /* script came from eval(), and is in eval cache */

    // Set for functions defined at the top level within an 'eval' script.
    bool directlyInsideEval:1;

    // Both 'arguments' and f.apply() are used. This is likely to be a wrapper.
    bool usesArgumentsAndApply:1;

    /* script is attempted to be cloned anew at each callsite. This is
       temporarily needed for ParallelArray selfhosted code until type
       information can be made context sensitive. See discussion in
       bug 826148. */
    bool            shouldCloneAtCallsite:1;
    bool            isCallsiteClone:1; /* is a callsite clone; has a link to the original function */
    bool            shouldInline:1;    /* hint to inline when possible */
#ifdef JS_ION
    bool            failedBoundsCheck:1; /* script has had hoisted bounds checks fail */
    bool            failedShapeGuard:1; /* script has had hoisted shape guard fail */
    bool            hadFrequentBailouts:1;
#else
    bool            failedBoundsCheckPad:1;
    bool            failedShapeGuardPad:1;
    bool            hadFrequentBailoutsPad:1;
#endif
    bool            invalidatedIdempotentCache:1; /* idempotent cache has triggered invalidation */

    // If the generator was created implicitly via a generator expression,
    // isGeneratorExp will be true.
    bool            isGeneratorExp:1;

    bool            hasScriptCounts:1;/* script has an entry in
                                         JSCompartment::scriptCountsMap */
    bool            hasDebugScript:1; /* script has an entry in
                                         JSCompartment::debugScriptMap */
    bool            hasFreezeConstraints:1; /* freeze constraints for stack
                                             * type sets have been generated */

  private:
    /* See comments below. */
    bool            argsHasVarBinding_:1;
    bool            needsArgsAnalysis_:1;
    bool            needsArgsObj_:1;

    //
    // End of fields.  Start methods.
    //

  public:
    static JSScript *Create(js::ExclusiveContext *cx,
                            js::HandleObject enclosingScope, bool savedCallerFun,
                            const JS::CompileOptions &options, unsigned staticLevel,
                            JS::HandleScriptSource sourceObject, uint32_t sourceStart,
                            uint32_t sourceEnd);

    void initCompartment(js::ExclusiveContext *cx);

    // Three ways ways to initialize a JSScript. Callers of partiallyInit()
    // and fullyInitTrivial() are responsible for notifying the debugger after
    // successfully creating any kind (function or other) of new JSScript.
    // However, callers of fullyInitFromEmitter() do not need to do this.
    static bool partiallyInit(js::ExclusiveContext *cx, JS::Handle<JSScript*> script,
                              uint32_t nobjects, uint32_t nregexps,
                              uint32_t ntrynotes, uint32_t nconsts, uint32_t nTypeSets);
    static bool fullyInitFromEmitter(js::ExclusiveContext *cx, JS::Handle<JSScript*> script,
                                     js::frontend::BytecodeEmitter *bce);
    // Initialize a no-op script.
    static bool fullyInitTrivial(js::ExclusiveContext *cx, JS::Handle<JSScript*> script);

    inline JSPrincipals *principals();

    JSCompartment *compartment() const { return compartment_; }

    void setVersion(JSVersion v) { version = v; }

    /* See ContextFlags::funArgumentsHasLocalBinding comment. */
    bool argumentsHasVarBinding() const { return argsHasVarBinding_; }
    jsbytecode *argumentsBytecode() const { JS_ASSERT(code[0] == JSOP_ARGUMENTS); return code; }
    void setArgumentsHasVarBinding();

    js::GeneratorKind generatorKind() const {
        return js::GeneratorKindFromBits(generatorKindBits_);
    }
    bool isGenerator() const { return generatorKind() != js::NotGenerator; }
    bool isLegacyGenerator() const { return generatorKind() == js::LegacyGenerator; }
    bool isStarGenerator() const { return generatorKind() == js::StarGenerator; }
    void setGeneratorKind(js::GeneratorKind kind) {
        // A script only gets its generator kind set as part of initialization,
        // so it can only transition from not being a generator.
        JS_ASSERT(!isGenerator());
        generatorKindBits_ = GeneratorKindAsBits(kind);
    }

    /*
     * As an optimization, even when argsHasLocalBinding, the function prologue
     * may not need to create an arguments object. This is determined by
     * needsArgsObj which is set by ScriptAnalysis::analyzeSSA before running
     * the script the first time. When !needsArgsObj, the prologue may simply
     * write MagicValue(JS_OPTIMIZED_ARGUMENTS) to 'arguments's slot and any
     * uses of 'arguments' will be guaranteed to handle this magic value.
     * So avoid spurious arguments object creation, we maintain the invariant
     * that needsArgsObj is only called after the script has been analyzed.
     */
    bool analyzedArgsUsage() const { return !needsArgsAnalysis_; }
    bool needsArgsObj() const { JS_ASSERT(analyzedArgsUsage()); return needsArgsObj_; }
    void setNeedsArgsObj(bool needsArgsObj);
    static bool argumentsOptimizationFailed(JSContext *cx, js::HandleScript script);

    /*
     * Arguments access (via JSOP_*ARG* opcodes) must access the canonical
     * location for the argument. If an arguments object exists AND this is a
     * non-strict function (where 'arguments' aliases formals), then all access
     * must go through the arguments object. Otherwise, the local slot is the
     * canonical location for the arguments. Note: if a formal is aliased
     * through the scope chain, then script->formalIsAliased and JSOP_*ARG*
     * opcodes won't be emitted at all.
     */
    bool argsObjAliasesFormals() const {
        return needsArgsObj() && !strict;
    }

    bool hasAnyIonScript() const {
        return hasIonScript() || hasParallelIonScript();
    }

    bool hasIonScript() const {
        return ion && ion != ION_DISABLED_SCRIPT && ion != ION_COMPILING_SCRIPT;
    }
    bool canIonCompile() const {
        return ion != ION_DISABLED_SCRIPT;
    }

    bool isIonCompilingOffThread() const {
        return ion == ION_COMPILING_SCRIPT;
    }

    js::jit::IonScript *ionScript() const {
        JS_ASSERT(hasIonScript());
        return ion;
    }
    js::jit::IonScript *maybeIonScript() const {
        return ion;
    }
    js::jit::IonScript *const *addressOfIonScript() const {
        return &ion;
    }
    inline void setIonScript(js::jit::IonScript *ionScript);

    bool hasBaselineScript() const {
        return baseline && baseline != BASELINE_DISABLED_SCRIPT;
    }
    bool canBaselineCompile() const {
        return baseline != BASELINE_DISABLED_SCRIPT;
    }
    js::jit::BaselineScript *baselineScript() const {
        JS_ASSERT(hasBaselineScript());
        return baseline;
    }
    inline void setBaselineScript(js::jit::BaselineScript *baselineScript);

    void updateBaselineOrIonRaw();

    bool hasParallelIonScript() const {
        return parallelIon && parallelIon != ION_DISABLED_SCRIPT && parallelIon != ION_COMPILING_SCRIPT;
    }

    bool canParallelIonCompile() const {
        return parallelIon != ION_DISABLED_SCRIPT;
    }

    bool isParallelIonCompilingOffThread() const {
        return parallelIon == ION_COMPILING_SCRIPT;
    }

    js::jit::IonScript *parallelIonScript() const {
        JS_ASSERT(hasParallelIonScript());
        return parallelIon;
    }
    js::jit::IonScript *maybeParallelIonScript() const {
        return parallelIon;
    }
    inline void setParallelIonScript(js::jit::IonScript *ionScript);

    static size_t offsetOfBaselineScript() {
        return offsetof(JSScript, baseline);
    }
    static size_t offsetOfIonScript() {
        return offsetof(JSScript, ion);
    }
    static size_t offsetOfParallelIonScript() {
        return offsetof(JSScript, parallelIon);
    }
    static size_t offsetOfBaselineOrIonRaw() {
        return offsetof(JSScript, baselineOrIonRaw);
    }
    static size_t offsetOfBaselineOrIonSkipArgCheck() {
        return offsetof(JSScript, baselineOrIonSkipArgCheck);
    }

    /*
     * Original compiled function for the script, if it has a function.
     * NULL for global and eval scripts.
     */
    JSFunction *function() const { return function_; }
    inline void setFunction(JSFunction *fun);

    JSFunction *originalFunction() const;
    void setOriginalFunctionObject(JSObject *fun);

    JSFlatString *sourceData(JSContext *cx);

    static bool loadSource(JSContext *cx, js::HandleScript scr, bool *worked);

    void setSourceObject(js::ScriptSourceObject *object);
    js::ScriptSourceObject *sourceObject() const;
    js::ScriptSource *scriptSource() const { return sourceObject()->source(); }
    JSPrincipals *originPrincipals() const { return scriptSource()->originPrincipals(); }
    const char *filename() const { return scriptSource()->filename(); }

  public:

    /* Return whether this script was compiled for 'eval' */
    bool isForEval() { return isCachedEval || isActiveEval; }

#ifdef DEBUG
    unsigned id();
#else
    unsigned id() { return 0; }
#endif

    /* Ensure the script has a TypeScript. */
    inline bool ensureHasTypes(JSContext *cx);

    /* Ensure the script has a TypeScript and map for computing BytecodeTypes. */
    inline bool ensureHasBytecodeTypeMap(JSContext *cx);

    /*
     * Ensure the script has bytecode analysis information. Performed when the
     * script first runs, or first runs after a TypeScript GC purge.
     */
    inline bool ensureRanAnalysis(JSContext *cx);

    /* Ensure the script has type inference analysis information. */
    inline bool ensureRanInference(JSContext *cx);

    inline bool hasAnalysis();
    inline void clearAnalysis();
    inline js::analyze::ScriptAnalysis *analysis();

    inline void clearPropertyReadTypes();

    inline js::GlobalObject &global() const;
    js::GlobalObject &uninlinedGlobal() const;

    /* See StaticScopeIter comment. */
    JSObject *enclosingStaticScope() const {
        if (isCallsiteClone)
            return NULL;
        return enclosingScopeOrOriginalFunction_;
    }

    /*
     * If a compile error occurs in an enclosing function after parsing a
     * nested function, the enclosing function's JSFunction, which appears on
     * the nested function's enclosingScope chain, will be invalid. Normal VM
     * operation only sees scripts where all enclosing scripts have been
     * successfully compiled. Any path that may look at scripts left over from
     * unsuccessful compilation (e.g., by iterating over all scripts in the
     * compartment) should check this predicate before doing any operation that
     * uses enclosingScope (e.g., ScopeCoordinateName).
     */
    bool enclosingScriptsCompiledSuccessfully() const;

  private:
    bool makeTypes(JSContext *cx);
    bool makeBytecodeTypeMap(JSContext *cx);
    bool makeAnalysis(JSContext *cx);

  public:
    uint32_t getUseCount() const  { return useCount; }
    uint32_t incUseCount(uint32_t amount = 1) { return useCount += amount; }
    uint32_t *addressOfUseCount() { return &useCount; }
    static size_t offsetOfUseCount() { return offsetof(JSScript, useCount); }
    void resetUseCount() { useCount = 0; }

  public:
    bool initScriptCounts(JSContext *cx);
    js::PCCounts getPCCounts(jsbytecode *pc);
    void addIonCounts(js::jit::IonScriptCounts *ionCounts);
    js::jit::IonScriptCounts *getIonCounts();
    js::ScriptCounts releaseScriptCounts();
    void destroyScriptCounts(js::FreeOp *fop);

    jsbytecode *main() {
        return code + mainOffset;
    }

    /*
     * computedSizeOfData() is the in-use size of all the data sections.
     * sizeOfData() is the size of the block allocated to hold all the data sections
     * (which can be larger than the in-use size).
     */
    size_t computedSizeOfData();
    size_t sizeOfData(mozilla::MallocSizeOf mallocSizeOf);

    uint32_t numNotes();  /* Number of srcnote slots in the srcnotes section */

    /* Script notes are allocated right after the code. */
    jssrcnote *notes() { return (jssrcnote *)(code + length); }

    bool hasArray(ArrayKind kind)           { return (hasArrayBits & (1 << kind)); }
    void setHasArray(ArrayKind kind)        { hasArrayBits |= (1 << kind); }
    void cloneHasArray(JSScript *script) { hasArrayBits = script->hasArrayBits; }

    bool hasConsts()        { return hasArray(CONSTS);      }
    bool hasObjects()       { return hasArray(OBJECTS);     }
    bool hasRegexps()       { return hasArray(REGEXPS);     }
    bool hasTrynotes()      { return hasArray(TRYNOTES);    }

    #define OFF(fooOff, hasFoo, t)   (fooOff() + (hasFoo() ? sizeof(t) : 0))

    size_t constsOffset()     { return 0; }
    size_t objectsOffset()    { return OFF(constsOffset,     hasConsts,     js::ConstArray);      }
    size_t regexpsOffset()    { return OFF(objectsOffset,    hasObjects,    js::ObjectArray);     }
    size_t trynotesOffset()   { return OFF(regexpsOffset,    hasRegexps,    js::ObjectArray);     }

    js::ConstArray *consts() {
        JS_ASSERT(hasConsts());
        return reinterpret_cast<js::ConstArray *>(data + constsOffset());
    }

    js::ObjectArray *objects() {
        JS_ASSERT(hasObjects());
        return reinterpret_cast<js::ObjectArray *>(data + objectsOffset());
    }

    js::ObjectArray *regexps() {
        JS_ASSERT(hasRegexps());
        return reinterpret_cast<js::ObjectArray *>(data + regexpsOffset());
    }

    js::TryNoteArray *trynotes() {
        JS_ASSERT(hasTrynotes());
        return reinterpret_cast<js::TryNoteArray *>(data + trynotesOffset());
    }

    js::HeapPtrAtom &getAtom(size_t index) const {
        JS_ASSERT(index < natoms);
        return atoms[index];
    }

    js::HeapPtrAtom &getAtom(jsbytecode *pc) const {
        JS_ASSERT(pc >= code && pc + sizeof(uint32_t) < code + length);
        return getAtom(GET_UINT32_INDEX(pc));
    }

    js::PropertyName *getName(size_t index) {
        return getAtom(index)->asPropertyName();
    }

    js::PropertyName *getName(jsbytecode *pc) const {
        JS_ASSERT(pc >= code && pc + sizeof(uint32_t) < code + length);
        return getAtom(GET_UINT32_INDEX(pc))->asPropertyName();
    }

    JSObject *getObject(size_t index) {
        js::ObjectArray *arr = objects();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    size_t innerObjectsStart() {
        // The first object contains the caller if savedCallerFun is used.
        return savedCallerFun ? 1 : 0;
    }

    JSObject *getObject(jsbytecode *pc) {
        JS_ASSERT(pc >= code && pc + sizeof(uint32_t) < code + length);
        return getObject(GET_UINT32_INDEX(pc));
    }

    JSVersion getVersion() const {
        return JSVersion(version);
    }

    inline JSFunction *getFunction(size_t index);
    inline JSFunction *getCallerFunction();
    inline JSFunction *functionOrCallerFunction();

    inline js::RegExpObject *getRegExp(size_t index);

    const js::Value &getConst(size_t index) {
        js::ConstArray *arr = consts();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    /*
     * The isEmpty method tells whether this script has code that computes any
     * result (not return value, result AKA normal completion value) other than
     * JSVAL_VOID, or any other effects.
     */
    bool isEmpty() const {
        if (length > 3)
            return false;

        jsbytecode *pc = code;
        if (noScriptRval && JSOp(*pc) == JSOP_FALSE)
            ++pc;
        return JSOp(*pc) == JSOP_STOP;
    }

    bool varIsAliased(unsigned varSlot);
    bool formalIsAliased(unsigned argSlot);
    bool formalLivesInArgumentsObject(unsigned argSlot);

  private:
    /*
     * Recompile with or without single-stepping support, as directed
     * by stepModeEnabled().
     */
    void recompileForStepMode(js::FreeOp *fop);

    /* Attempt to change this->stepMode to |newValue|. */
    bool tryNewStepMode(JSContext *cx, uint32_t newValue);

    bool ensureHasDebugScript(JSContext *cx);
    js::DebugScript *debugScript();
    js::DebugScript *releaseDebugScript();
    void destroyDebugScript(js::FreeOp *fop);

  public:
    bool hasBreakpointsAt(jsbytecode *pc);
    bool hasAnyBreakpointsOrStepMode() { return hasDebugScript; }

    js::BreakpointSite *getBreakpointSite(jsbytecode *pc)
    {
        JS_ASSERT(size_t(pc - code) < length);
        return hasDebugScript ? debugScript()->breakpoints[pc - code] : NULL;
    }

    js::BreakpointSite *getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc);

    void destroyBreakpointSite(js::FreeOp *fop, jsbytecode *pc);

    void clearBreakpointsIn(js::FreeOp *fop, js::Debugger *dbg, JSObject *handler);
    void clearTraps(js::FreeOp *fop);

    void markTrapClosures(JSTracer *trc);

    /*
     * Set or clear the single-step flag. If the flag is set or the count
     * (adjusted by changeStepModeCount) is non-zero, then the script is in
     * single-step mode. (JSD uses an on/off-style interface; Debugger uses a
     * count-style interface.)
     */
    bool setStepModeFlag(JSContext *cx, bool step);

    /*
     * Increment or decrement the single-step count. If the count is non-zero or
     * the flag (set by setStepModeFlag) is set, then the script is in
     * single-step mode. (JSD uses an on/off-style interface; Debugger uses a
     * count-style interface.)
     */
    bool changeStepModeCount(JSContext *cx, int delta);

    bool stepModeEnabled() { return hasDebugScript && !!debugScript()->stepMode; }

#ifdef DEBUG
    uint32_t stepModeCount() { return hasDebugScript ? (debugScript()->stepMode & stepCountMask) : 0; }
#endif

    void finalize(js::FreeOp *fop);

    JS::Zone *zone() const { return tenuredZone(); }

    static inline void writeBarrierPre(JSScript *script);
    static void writeBarrierPost(JSScript *script, void *addr) {}

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_SCRIPT; }

    void markChildren(JSTracer *trc);
};

/* The array kind flags are stored in a 4-bit field; make sure they fit. */
JS_STATIC_ASSERT(JSScript::ARRAY_KIND_BITS <= 4);

/* If this fails, add/remove padding within JSScript. */
JS_STATIC_ASSERT(sizeof(JSScript) % js::gc::CellSize == 0);

namespace js {

/*
 * Iterator over a script's bindings (formals and variables).
 * The order of iteration is:
 *  - first, formal arguments, from index 0 to numArgs
 *  - next, variables, from index 0 to numVars
 */
class BindingIter
{
    const InternalBindingsHandle bindings_;
    unsigned i_;

    friend class Bindings;

  public:
    explicit BindingIter(const InternalBindingsHandle &bindings) : bindings_(bindings), i_(0) {}
    explicit BindingIter(const HandleScript &script) : bindings_(script, &script->bindings), i_(0) {}

    bool done() const { return i_ == bindings_->count(); }
    operator bool() const { return !done(); }
    void operator++(int) { JS_ASSERT(!done()); i_++; }
    BindingIter &operator++() { (*this)++; return *this; }

    unsigned frameIndex() const {
        JS_ASSERT(!done());
        return i_ < bindings_->numArgs() ? i_ : i_ - bindings_->numArgs();
    }

    const Binding &operator*() const { JS_ASSERT(!done()); return bindings_->bindingArray()[i_]; }
    const Binding *operator->() const { JS_ASSERT(!done()); return &bindings_->bindingArray()[i_]; }
};

/*
 * This helper function fills the given BindingVector with the sequential
 * values of BindingIter.
 */

typedef Vector<Binding, 32> BindingVector;

extern bool
FillBindingVector(HandleScript fromScript, BindingVector *vec);

/*
 * Iterator over the aliased formal bindings in ascending index order. This can
 * be veiwed as a filtering of BindingIter with predicate
 *   bi->aliased() && bi->kind() == ARGUMENT
 */
class AliasedFormalIter
{
    const Binding *begin_, *p_, *end_;
    unsigned slot_;

    void settle() {
        while (p_ != end_ && !p_->aliased())
            p_++;
    }

  public:
    explicit inline AliasedFormalIter(JSScript *script);

    bool done() const { return p_ == end_; }
    operator bool() const { return !done(); }
    void operator++(int) { JS_ASSERT(!done()); p_++; slot_++; settle(); }

    const Binding &operator*() const { JS_ASSERT(!done()); return *p_; }
    const Binding *operator->() const { JS_ASSERT(!done()); return p_; }
    unsigned frameIndex() const { JS_ASSERT(!done()); return p_ - begin_; }
    unsigned scopeSlot() const { JS_ASSERT(!done()); return slot_; }
};

struct SourceCompressionToken;

// Information about a script which may be (or has been) lazily compiled to
// bytecode from its source.
class LazyScript : public js::gc::Cell
{
    // If non-NULL, the script has been compiled and this is a forwarding
    // pointer to the result.
    HeapPtrScript script_;

    // Original function with which the lazy script is associated.
    HeapPtrFunction function_;

    // Function or block chain in which the script is nested, or NULL.
    HeapPtrObject enclosingScope_;

    // Source code object, or NULL if the script in which this is nested has
    // not been compiled yet.
    HeapPtrObject sourceObject_;

    // Heap allocated table with any free variables or inner functions.
    void *table_;

#if JS_BITS_PER_WORD == 32
    uint32_t padding;
#endif

    // Assorted bits that should really be in ScriptSourceObject.
    uint32_t version_ : 8;

    uint32_t numFreeVariables_ : 24;
    uint32_t numInnerFunctions_ : 24;

    uint32_t generatorKindBits_:2;

    // N.B. These are booleans but need to be uint32_t to pack correctly on MSVC.
    uint32_t strict_ : 1;
    uint32_t bindingsAccessedDynamically_ : 1;
    uint32_t hasDebuggerStatement_ : 1;
    uint32_t directlyInsideEval_:1;
    uint32_t usesArgumentsAndApply_:1;
    uint32_t hasBeenCloned_:1;

    // Source location for the script.
    uint32_t begin_;
    uint32_t end_;
    uint32_t lineno_;
    uint32_t column_;

    LazyScript(JSFunction *fun, void *table,
               uint32_t numFreeVariables, uint32_t numInnerFunctions, JSVersion version,
               uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column);

  public:
    static LazyScript *Create(ExclusiveContext *cx, HandleFunction fun,
                              uint32_t numFreeVariables, uint32_t numInnerFunctions,
                              JSVersion version, uint32_t begin, uint32_t end,
                              uint32_t lineno, uint32_t column);

    JSFunction *function() const {
        return function_;
    }

    void initScript(JSScript *script);
    JSScript *maybeScript() {
        return script_;
    }

    JSObject *enclosingScope() const {
        return enclosingScope_;
    }
    ScriptSourceObject *sourceObject() const;
    ScriptSource *scriptSource() const {
        return sourceObject()->source();
    }
    JSPrincipals *originPrincipals() const {
        return scriptSource()->originPrincipals();
    }
    JSVersion version() const {
        JS_STATIC_ASSERT(JSVERSION_UNKNOWN == -1);
        return (version_ == JS_BIT(8) - 1) ? JSVERSION_UNKNOWN : JSVersion(version_);
    }

    void setParent(JSObject *enclosingScope, ScriptSourceObject *sourceObject);

    uint32_t numFreeVariables() const {
        return numFreeVariables_;
    }
    HeapPtrAtom *freeVariables() {
        return (HeapPtrAtom *)table_;
    }

    uint32_t numInnerFunctions() const {
        return numInnerFunctions_;
    }
    HeapPtrFunction *innerFunctions() {
        return (HeapPtrFunction *)&freeVariables()[numFreeVariables()];
    }

    GeneratorKind generatorKind() const { return GeneratorKindFromBits(generatorKindBits_); }

    bool isGenerator() const { return generatorKind() != NotGenerator; }

    bool isLegacyGenerator() const { return generatorKind() == LegacyGenerator; }

    bool isStarGenerator() const { return generatorKind() == StarGenerator; }

    void setGeneratorKind(GeneratorKind kind) {
        // A script only gets its generator kind set as part of initialization,
        // so it can only transition from NotGenerator.
        JS_ASSERT(!isGenerator());
        // Legacy generators cannot currently be lazy.
        JS_ASSERT(kind != LegacyGenerator);
        generatorKindBits_ = GeneratorKindAsBits(kind);
    }

    bool strict() const {
        return strict_;
    }
    void setStrict() {
        strict_ = true;
    }

    bool bindingsAccessedDynamically() const {
        return bindingsAccessedDynamically_;
    }
    void setBindingsAccessedDynamically() {
        bindingsAccessedDynamically_ = true;
    }

    bool hasDebuggerStatement() const {
        return hasDebuggerStatement_;
    }
    void setHasDebuggerStatement() {
        hasDebuggerStatement_ = true;
    }

    bool directlyInsideEval() const {
        return directlyInsideEval_;
    }
    void setDirectlyInsideEval() {
        directlyInsideEval_ = true;
    }

    bool usesArgumentsAndApply() const {
        return usesArgumentsAndApply_;
    }
    void setUsesArgumentsAndApply() {
        usesArgumentsAndApply_ = true;
    }

    bool hasBeenCloned() const {
        return hasBeenCloned_;
    }
    void setHasBeenCloned() {
        hasBeenCloned_ = true;
    }

    ScriptSource *source() const {
        return sourceObject()->source();
    }
    uint32_t begin() const {
        return begin_;
    }
    uint32_t end() const {
        return end_;
    }
    uint32_t lineno() const {
        return lineno_;
    }
    uint32_t column() const {
        return column_;
    }

    uint32_t staticLevel(JSContext *cx) const;

    Zone *zone() const {
        return Cell::tenuredZone();
    }

    void markChildren(JSTracer *trc);
    void finalize(js::FreeOp *fop);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    {
        return mallocSizeOf(table_);
    }

    static inline void writeBarrierPre(LazyScript *lazy);
};

/* If this fails, add/remove padding within LazyScript. */
JS_STATIC_ASSERT(sizeof(LazyScript) % js::gc::CellSize == 0);

#ifdef JS_THREADSAFE
/*
 * Background thread to compress JS source code. This happens only while parsing
 * and bytecode generation is happening in the main thread. If needed, the
 * compiler waits for compression to complete before returning.
 *
 * To use it, you have to have a SourceCompressionToken, tok, with tok.ss and
 * tok.chars set to the proper values. When the SourceCompressionToken is
 * destroyed, it makes sure the compression is complete. If you are about to
 * successfully exit the scope of tok, you should call and check the return
 * value of SourceCompressionToken::complete(). It returns false if allocation
 * errors occurred in the thread.
 */
class SourceCompressorThread
{
  private:
    enum {
        // The compression thread is in the process of compression some source.
        COMPRESSING,
        // The compression thread is not doing anything and available to
        // compress source.
        IDLE,
        // Set by finish() to tell the compression thread to exit.
        SHUTDOWN
    } state;
    SourceCompressionToken *tok;
    PRThread *thread;
    // Protects |state| and |tok| when it's non-NULL.
    PRLock *lock;
    // When it's idling, the compression thread blocks on this. The main thread
    // uses it to notify the compression thread when it has source to be
    // compressed.
    PRCondVar *wakeup;
    // The main thread can block on this to wait for compression to finish.
    PRCondVar *done;
    // Flag which can be set by the main thread to ask compression to abort.
    volatile bool stop;

    bool internalCompress();
    void threadLoop();
    static void compressorThread(void *arg);

  public:
    explicit SourceCompressorThread()
    : state(IDLE),
      tok(NULL),
      thread(NULL),
      lock(NULL),
      wakeup(NULL),
      done(NULL) {}
    void finish();
    bool init();
    void compress(SourceCompressionToken *tok);
    void waitOnCompression(SourceCompressionToken *userTok);
    void abort(SourceCompressionToken *userTok);
    const jschar *currentChars() const;
};
#endif

struct SourceCompressionToken
{
    friend class ScriptSource;
    friend class SourceCompressorThread;
  private:
    JSContext *cx;
    ScriptSource *ss;
    const jschar *chars;
    bool oom;
  public:
    explicit SourceCompressionToken(JSContext *cx)
       : cx(cx), ss(NULL), chars(NULL), oom(false) {}
    ~SourceCompressionToken()
    {
        complete();
    }

    bool complete();
    void abort();
    bool active() const { return !!ss; }
};

/*
 * New-script-hook calling is factored from JSScript::fullyInitFromEmitter() so
 * that it and callers of XDRScript() can share this code.  In the case of
 * callers of XDRScript(), the hook should be invoked only after successful
 * decode of any owning function (the fun parameter) or script object (null
 * fun).
 */
extern void
CallNewScriptHook(JSContext *cx, JS::HandleScript script, JS::HandleFunction fun);

extern void
CallDestroyScriptHook(FreeOp *fop, JSScript *script);

struct SharedScriptData
{
    uint32_t length;
    uint32_t natoms;
    bool marked;
    jsbytecode data[1];

    static SharedScriptData *new_(ExclusiveContext *cx, uint32_t codeLength,
                                  uint32_t srcnotesLength, uint32_t natoms);

    HeapPtrAtom *atoms() {
        if (!natoms)
            return NULL;
        return reinterpret_cast<HeapPtrAtom *>(data + length - sizeof(JSAtom *) * natoms);
    }

    static SharedScriptData *fromBytecode(const jsbytecode *bytecode) {
        return (SharedScriptData *)(bytecode - offsetof(SharedScriptData, data));
    }

  private:
    SharedScriptData() MOZ_DELETE;
    SharedScriptData(const SharedScriptData&) MOZ_DELETE;
};

struct ScriptBytecodeHasher
{
    struct Lookup
    {
        jsbytecode          *code;
        uint32_t            length;

        Lookup(SharedScriptData *ssd) : code(ssd->data), length(ssd->length) {}
    };
    static HashNumber hash(const Lookup &l) { return mozilla::HashBytes(l.code, l.length); }
    static bool match(SharedScriptData *entry, const Lookup &lookup) {
        if (entry->length != lookup.length)
            return false;
        return mozilla::PodEqual<jsbytecode>(entry->data, lookup.code, lookup.length);
    }
};

typedef HashSet<SharedScriptData*,
                ScriptBytecodeHasher,
                SystemAllocPolicy> ScriptDataTable;

extern void
SweepScriptData(JSRuntime *rt);

extern void
FreeScriptData(JSRuntime *rt);

struct ScriptAndCounts
{
    /* This structure is stored and marked from the JSRuntime. */
    JSScript *script;
    ScriptCounts scriptCounts;

    PCCounts &getPCCounts(jsbytecode *pc) const {
        JS_ASSERT(unsigned(pc - script->code) < script->length);
        return scriptCounts.pcCountsVector[pc - script->code];
    }

    jit::IonScriptCounts *getIonCounts() const {
        return scriptCounts.ionCounts;
    }
};

} /* namespace js */

extern jssrcnote *
js_GetSrcNote(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, unsigned lineno);

extern JS_FRIEND_API(unsigned)
js_GetScriptLineExtent(JSScript *script);

namespace js {

extern unsigned
PCToLineNumber(JSScript *script, jsbytecode *pc, unsigned *columnp = NULL);

extern unsigned
PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc,
               unsigned *columnp = NULL);

/*
 * This function returns the file and line number of the script currently
 * executing on cx. If there is no current script executing on cx (e.g., a
 * native called directly through JSAPI (e.g., by setTimeout)), NULL and 0 are
 * returned as the file and line. Additionally, this function avoids the full
 * linear scan to compute line number when the caller guarnatees that the
 * script compilation occurs at a JSOP_EVAL.
 */

enum LineOption {
    CALLED_FROM_JSOP_EVAL,
    NOT_CALLED_FROM_JSOP_EVAL
};

extern void
CurrentScriptFileLineOrigin(JSContext *cx, const char **file, unsigned *linenop,
                            JSPrincipals **origin, LineOption opt = NOT_CALLED_FROM_JSOP_EVAL);

extern JSScript *
CloneScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun, HandleScript script,
            NewObjectKind newKind = GenericObject);

bool
CloneFunctionScript(JSContext *cx, HandleFunction original, HandleFunction clone,
                    NewObjectKind newKind = GenericObject);

/*
 * JSAPI clients are allowed to leave CompileOptions.originPrincipals NULL in
 * which case the JS engine sets options.originPrincipals = origin.principals.
 * This normalization step must occur before the originPrincipals get stored in
 * the JSScript/ScriptSource.
 */

static inline JSPrincipals *
NormalizeOriginPrincipals(JSPrincipals *principals, JSPrincipals *originPrincipals)
{
    return originPrincipals ? originPrincipals : principals;
}

/*
 * NB: after a successful XDR_DECODE, XDRScript callers must do any required
 * subsequent set-up of owning function or script object and then call
 * js_CallNewScriptHook.
 */
template<XDRMode mode>
bool
XDRScript(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
          HandleFunction fun, MutableHandleScript scriptp);

} /* namespace js */

#endif /* jsscript_h */
