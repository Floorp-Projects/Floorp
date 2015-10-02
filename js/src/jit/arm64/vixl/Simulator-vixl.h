// Copyright 2013, ARM Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef VIXL_A64_SIMULATOR_A64_H_
#define VIXL_A64_SIMULATOR_A64_H_

#include "js-config.h"

#ifdef JS_SIMULATOR_ARM64

#include "mozilla/Vector.h"

#include "jsalloc.h"

#include "jit/arm64/vixl/Assembler-vixl.h"
#include "jit/arm64/vixl/Disasm-vixl.h"
#include "jit/arm64/vixl/Globals-vixl.h"
#include "jit/arm64/vixl/Instructions-vixl.h"
#include "jit/arm64/vixl/Instrument-vixl.h"
#include "jit/arm64/vixl/Utils-vixl.h"
#include "jit/IonTypes.h"
#include "vm/PosixNSPR.h"


#define JS_CHECK_SIMULATOR_RECURSION_WITH_EXTRA(cx, extra, onerror)             \
    JS_BEGIN_MACRO                                                              \
        if (cx->mainThread().simulator()->overRecursedWithExtra(extra)) {       \
            js::ReportOverRecursed(cx);                                         \
            onerror;                                                            \
        }                                                                       \
    JS_END_MACRO

namespace vixl {

// Debug instructions.
//
// VIXL's macro-assembler and simulator support a few pseudo instructions to
// make debugging easier. These pseudo instructions do not exist on real
// hardware.
//
// TODO: Provide controls to prevent the macro assembler from emitting
// pseudo-instructions. This is important for ahead-of-time compilers, where the
// macro assembler is built with USE_SIMULATOR but the code will eventually be
// run on real hardware.
//
// TODO: Also consider allowing these pseudo-instructions to be disabled in the
// simulator, so that users can check that the input is a valid native code.
// (This isn't possible in all cases. Printf won't work, for example.)
//
// Each debug pseudo instruction is represented by a HLT instruction. The HLT
// immediate field is used to identify the type of debug pseudo isntruction.
// Each pseudo instruction uses a custom encoding for additional arguments, as
// described below.

// Unreachable
//
// Instruction which should never be executed. This is used as a guard in parts
// of the code that should not be reachable, such as in data encoded inline in
// the instructions.
const Instr kUnreachableOpcode = 0xdeb0;

// Printf
//  - arg_count: The number of arguments.
//  - arg_pattern: A set of PrintfArgPattern values, packed into two-bit fields.
//
// Simulate a call to printf.
//
// Floating-point and integer arguments are passed in separate sets of registers
// in AAPCS64 (even for varargs functions), so it is not possible to determine
// the type of each argument without some information about the values that were
// passed in. This information could be retrieved from the printf format string,
// but the format string is not trivial to parse so we encode the relevant
// information with the HLT instruction.
//
// Also, the following registers are populated (as if for a native A64 call):
//    x0: The format string
// x1-x7: Optional arguments, if type == CPURegister::kRegister
// d0-d7: Optional arguments, if type == CPURegister::kFPRegister
const Instr kPrintfOpcode = 0xdeb1;
const unsigned kPrintfArgCountOffset = 1 * kInstructionSize;
const unsigned kPrintfArgPatternListOffset = 2 * kInstructionSize;
const unsigned kPrintfLength = 3 * kInstructionSize;

const unsigned kPrintfMaxArgCount = 4;

// The argument pattern is a set of two-bit-fields, each with one of the
// following values:
enum PrintfArgPattern {
  kPrintfArgW = 1,
  kPrintfArgX = 2,
  // There is no kPrintfArgS because floats are always converted to doubles in C
  // varargs calls.
  kPrintfArgD = 3
};
static const unsigned kPrintfArgPatternBits = 2;

// Trace
//  - parameter: TraceParameter stored as a uint32_t
//  - command: TraceCommand stored as a uint32_t
//
// Allow for trace management in the generated code. This enables or disables
// automatic tracing of the specified information for every simulated
// instruction.
const Instr kTraceOpcode = 0xdeb2;
const unsigned kTraceParamsOffset = 1 * kInstructionSize;
const unsigned kTraceCommandOffset = 2 * kInstructionSize;
const unsigned kTraceLength = 3 * kInstructionSize;

// Trace parameters.
enum TraceParameters {
  LOG_DISASM     = 1 << 0,  // Log disassembly.
  LOG_REGS       = 1 << 1,  // Log general purpose registers.
  LOG_FP_REGS    = 1 << 2,  // Log floating-point registers.
  LOG_SYS_REGS   = 1 << 3,  // Log the flags and system registers.
  LOG_WRITE      = 1 << 4,  // Log writes to memory.

  LOG_NONE       = 0,
  LOG_STATE      = LOG_REGS | LOG_FP_REGS | LOG_SYS_REGS,
  LOG_ALL        = LOG_DISASM | LOG_STATE | LOG_WRITE
};

// Trace commands.
enum TraceCommand {
  TRACE_ENABLE   = 1,
  TRACE_DISABLE  = 2
};

// Log
//  - parameter: TraceParameter stored as a uint32_t
//
// Print the specified information once. This mechanism is separate from Trace.
// In particular, _all_ of the specified registers are printed, rather than just
// the registers that the instruction writes.
//
// Any combination of the TraceParameters values can be used, except that
// LOG_DISASM is not supported for Log.
const Instr kLogOpcode = 0xdeb3;
const unsigned kLogParamsOffset = 1 * kInstructionSize;
const unsigned kLogLength = 2 * kInstructionSize;

// The proper way to initialize a simulated system register (such as NZCV) is as
// follows:
//  SimSystemRegister nzcv = SimSystemRegister::DefaultValueFor(NZCV);
class SimSystemRegister {
 public:
  // The default constructor represents a register which has no writable bits.
  // It is not possible to set its value to anything other than 0.
  SimSystemRegister() : value_(0), write_ignore_mask_(0xffffffff) { }

  uint32_t RawValue() const {
    return value_;
  }

  void SetRawValue(uint32_t new_value) {
    value_ = (value_ & write_ignore_mask_) | (new_value & ~write_ignore_mask_);
  }

  uint32_t Bits(int msb, int lsb) const {
    return unsigned_bitextract_32(msb, lsb, value_);
  }

  int32_t SignedBits(int msb, int lsb) const {
    return signed_bitextract_32(msb, lsb, value_);
  }

  void SetBits(int msb, int lsb, uint32_t bits);

  // Default system register values.
  static SimSystemRegister DefaultValueFor(SystemRegister id);

#define DEFINE_GETTER(Name, HighBit, LowBit, Func)                            \
  uint32_t Name() const { return Func(HighBit, LowBit); }              \
  void Set##Name(uint32_t bits) { SetBits(HighBit, LowBit, bits); }
#define DEFINE_WRITE_IGNORE_MASK(Name, Mask)                                  \
  static const uint32_t Name##WriteIgnoreMask = ~static_cast<uint32_t>(Mask);

  SYSTEM_REGISTER_FIELDS_LIST(DEFINE_GETTER, DEFINE_WRITE_IGNORE_MASK)

#undef DEFINE_ZERO_BITS
#undef DEFINE_GETTER

 protected:
  // Most system registers only implement a few of the bits in the word. Other
  // bits are "read-as-zero, write-ignored". The write_ignore_mask argument
  // describes the bits which are not modifiable.
  SimSystemRegister(uint32_t value, uint32_t write_ignore_mask)
      : value_(value), write_ignore_mask_(write_ignore_mask) { }

  uint32_t value_;
  uint32_t write_ignore_mask_;
};


// Represent a register (r0-r31, v0-v31).
template<int kSizeInBytes>
class SimRegisterBase {
 public:
  // Write the specified value. The value is zero-extended if necessary.
  template<typename T>
  void Set(T new_value) {
    VIXL_STATIC_ASSERT(sizeof(new_value) <= kSizeInBytes);
    if (sizeof(new_value) < kSizeInBytes) {
      // All AArch64 registers are zero-extending.
      memset(value_ + sizeof(new_value), 0, kSizeInBytes - sizeof(new_value));
    }
    memcpy(value_, &new_value, sizeof(new_value));
  }

  // Read the value as the specified type. The value is truncated if necessary.
  template<typename T>
  T Get() const {
    T result;
    VIXL_STATIC_ASSERT(sizeof(result) <= kSizeInBytes);
    memcpy(&result, value_, sizeof(T));
    return result;
  }

 protected:
  uint8_t value_[kSizeInBytes];
};
typedef SimRegisterBase<kXRegSizeInBytes> SimRegister;      // r0-r31
typedef SimRegisterBase<kDRegSizeInBytes> SimFPRegister;    // v0-v31


class SimExclusiveLocalMonitor {
 public:
  SimExclusiveLocalMonitor() : kSkipClearProbability(8), seed_(0x87654321) {
    Clear();
  }

  // Clear the exclusive monitor (like clrex).
  void Clear() {
    address_ = 0;
    size_ = 0;
  }

  // Clear the exclusive monitor most of the time.
  void MaybeClear() {
    if ((seed_ % kSkipClearProbability) != 0) {
      Clear();
    }

    // Advance seed_ using a simple linear congruential generator.
    seed_ = (seed_ * 48271) % 2147483647;
  }

  // Mark the address range for exclusive access (like load-exclusive).
  void MarkExclusive(uint64_t address, size_t size) {
    address_ = address;
    size_ = size;
  }

  // Return true if the address range is marked (like store-exclusive).
  // This helper doesn't implicitly clear the monitor.
  bool IsExclusive(uint64_t address, size_t size) {
    VIXL_ASSERT(size > 0);
    // Be pedantic: Require both the address and the size to match.
    return (size == size_) && (address == address_);
  }

 private:
  uint64_t address_;
  size_t size_;

  const int kSkipClearProbability;
  uint32_t seed_;
};


// We can't accurate simulate the global monitor since it depends on external
// influences. Instead, this implementation occasionally causes accesses to
// fail, according to kPassProbability.
class SimExclusiveGlobalMonitor {
 public:
  SimExclusiveGlobalMonitor() : kPassProbability(8), seed_(0x87654321) {}

  bool IsExclusive(uint64_t address, size_t size) {
    USE(address);
    USE(size);

    bool pass = (seed_ % kPassProbability) != 0;
    // Advance seed_ using a simple linear congruential generator.
    seed_ = (seed_ * 48271) % 2147483647;
    return pass;
  }

 private:
  const int kPassProbability;
  uint32_t seed_;
};

class Redirection;

class Simulator : public DecoderVisitor {
  friend class AutoLockSimulatorCache;

 public:
  explicit Simulator(Decoder* decoder, FILE* stream = stdout);
  ~Simulator();

  // Moz changes.
  void init(Decoder* decoder, FILE* stream);
  static Simulator* Current();
  static Simulator* Create();
  static void Destroy(Simulator* sim);
  uintptr_t stackLimit() const;
  uintptr_t* addressOfStackLimit();
  bool overRecursed(uintptr_t newsp = 0) const;
  bool overRecursedWithExtra(uint32_t extra) const;
  int64_t call(uint8_t* entry, int argument_count, ...);
  void setRedirection(Redirection* redirection);
  Redirection* redirection() const;
  static void* RedirectNativeFunction(void* nativeFunction, js::jit::ABIFunctionType type);
  void setGPR32Result(int32_t result);
  void setGPR64Result(int64_t result);
  void setFP32Result(float result);
  void setFP64Result(double result);
  void VisitCallRedirection(const Instruction* instr);

  void ResetState();

  static inline uintptr_t StackLimit() {
    return Simulator::Current()->stackLimit();
  }

  // Run the simulator.
  virtual void Run();
  void RunFrom(const Instruction* first);

  // Simulation helpers.
  const Instruction* pc() const { return pc_; }
  const Instruction* get_pc() const { return pc_; }

  template <typename T>
  T get_pc_as() const { return reinterpret_cast<T>(const_cast<Instruction*>(pc())); }

  void set_pc(const Instruction* new_pc) {
    pc_ = AddressUntag(new_pc);
    pc_modified_ = true;
  }
  void set_resume_pc(void* new_resume_pc);

  void increment_pc() {
    if (!pc_modified_) {
      pc_ = pc_->NextInstruction();
    }

    pc_modified_ = false;
  }

  void ExecuteInstruction();

  // Declare all Visitor functions.
  #define DECLARE(A) virtual void Visit##A(const Instruction* instr);
  VISITOR_LIST(DECLARE)
  #undef DECLARE

  // Integer register accessors.

  // Basic accessor: Read the register as the specified type.
  template<typename T>
  T reg(unsigned code, Reg31Mode r31mode = Reg31IsZeroRegister) const {
    VIXL_ASSERT(code < kNumberOfRegisters);
    if ((code == 31) && (r31mode == Reg31IsZeroRegister)) {
      T result;
      memset(&result, 0, sizeof(result));
      return result;
    }
    return registers_[code].Get<T>();
  }

  // Common specialized accessors for the reg() template.
  int32_t wreg(unsigned code,
               Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int32_t>(code, r31mode);
  }

  int64_t xreg(unsigned code,
               Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int64_t>(code, r31mode);
  }

  // As above, with parameterized size and return type. The value is
  // either zero-extended or truncated to fit, as required.
  template<typename T>
  T reg(unsigned size, unsigned code,
        Reg31Mode r31mode = Reg31IsZeroRegister) const {
    uint64_t raw;
    switch (size) {
      case kWRegSize: raw = reg<uint32_t>(code, r31mode); break;
      case kXRegSize: raw = reg<uint64_t>(code, r31mode); break;
      default:
        VIXL_UNREACHABLE();
        return 0;
    }

    T result;
    VIXL_STATIC_ASSERT(sizeof(result) <= sizeof(raw));
    // Copy the result and truncate to fit. This assumes a little-endian host.
    memcpy(&result, &raw, sizeof(result));
    return result;
  }

  // Use int64_t by default if T is not specified.
  int64_t reg(unsigned size, unsigned code,
              Reg31Mode r31mode = Reg31IsZeroRegister) const {
    return reg<int64_t>(size, code, r31mode);
  }

  enum RegLogMode {
    LogRegWrites,
    NoRegLog
  };

  // Write 'value' into an integer register. The value is zero-extended. This
  // behaviour matches AArch64 register writes.
  template<typename T>
  void set_reg(unsigned code, T value,
               RegLogMode log_mode = LogRegWrites,
               Reg31Mode r31mode = Reg31IsZeroRegister) {
    VIXL_STATIC_ASSERT((sizeof(T) == kWRegSizeInBytes) ||
                       (sizeof(T) == kXRegSizeInBytes));
    VIXL_ASSERT(code < kNumberOfRegisters);

    if ((code == 31) && (r31mode == Reg31IsZeroRegister)) {
      return;
    }

    registers_[code].Set(value);

    if (log_mode == LogRegWrites) LogRegister(code, r31mode);
  }

  // Common specialized accessors for the set_reg() template.
  void set_wreg(unsigned code, int32_t value,
                RegLogMode log_mode = LogRegWrites,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(code, value, log_mode, r31mode);
  }

  void set_xreg(unsigned code, int64_t value,
               RegLogMode log_mode = LogRegWrites,
                Reg31Mode r31mode = Reg31IsZeroRegister) {
    set_reg(code, value, log_mode, r31mode);
  }

  // As above, with parameterized size and type. The value is either
  // zero-extended or truncated to fit, as required.
  template<typename T>
  void set_reg(unsigned size, unsigned code, T value,
               RegLogMode log_mode = LogRegWrites,
               Reg31Mode r31mode = Reg31IsZeroRegister) {
    // Zero-extend the input.
    uint64_t raw = 0;
    VIXL_STATIC_ASSERT(sizeof(value) <= sizeof(raw));
    memcpy(&raw, &value, sizeof(value));

    // Write (and possibly truncate) the value.
    switch (size) {
      case kWRegSize: set_reg<uint32_t>(code, raw, log_mode, r31mode); break;
      case kXRegSize: set_reg<uint64_t>(code, raw, log_mode, r31mode); break;
      default:
        VIXL_UNREACHABLE();
        return;
    }
  }

  // Common specialized accessors for the set_reg() template.

  // Commonly-used special cases.
  template<typename T>
  void set_lr(T value) {
    set_reg(kLinkRegCode, value);
  }

  template<typename T>
  void set_sp(T value) {
    set_reg(31, value, LogRegWrites, Reg31IsStackPointer);
  }

  // FP register accessors.
  // These are equivalent to the integer register accessors, but for FP
  // registers.

  // Basic accessor: Read the register as the specified type.
  template<typename T>
  T fpreg(unsigned code) const {
    VIXL_STATIC_ASSERT((sizeof(T) == kSRegSizeInBytes) ||
                       (sizeof(T) == kDRegSizeInBytes));
    VIXL_ASSERT(code < kNumberOfFPRegisters);

    return fpregisters_[code].Get<T>();
  }

  // Common specialized accessors for the fpreg() template.
  float sreg(unsigned code) const {
    return fpreg<float>(code);
  }

  uint32_t sreg_bits(unsigned code) const {
    return fpreg<uint32_t>(code);
  }

  double dreg(unsigned code) const {
    return fpreg<double>(code);
  }

  uint64_t dreg_bits(unsigned code) const {
    return fpreg<uint64_t>(code);
  }

  // As above, with parameterized size and return type. The value is
  // either zero-extended or truncated to fit, as required.
  template<typename T>
  T fpreg(unsigned size, unsigned code) const {
    uint64_t raw;
    switch (size) {
      case kSRegSize: raw = fpreg<uint32_t>(code); break;
      case kDRegSize: raw = fpreg<uint64_t>(code); break;
      default:
        VIXL_UNREACHABLE();
        raw = 0;
        break;
    }

    T result;
    VIXL_STATIC_ASSERT(sizeof(result) <= sizeof(raw));
    // Copy the result and truncate to fit. This assumes a little-endian host.
    memcpy(&result, &raw, sizeof(result));
    return result;
  }

  // Basic accessor: Write the specified value.
  template<typename T>
  void set_fpreg(unsigned code, T value,
                RegLogMode log_mode = LogRegWrites) {
    VIXL_STATIC_ASSERT((sizeof(value) == kSRegSizeInBytes) ||
                       (sizeof(value) == kDRegSizeInBytes));
    VIXL_ASSERT(code < kNumberOfFPRegisters);
    fpregisters_[code].Set(value);

    if (log_mode == LogRegWrites) {
      if (sizeof(value) <= kSRegSizeInBytes) {
        LogFPRegister(code, kPrintSRegValue);
      } else {
        LogFPRegister(code, kPrintDRegValue);
      }
    }
  }

  // Common specialized accessors for the set_fpreg() template.
  void set_sreg(unsigned code, float value,
                RegLogMode log_mode = LogRegWrites) {
    set_fpreg(code, value, log_mode);
  }

  void set_sreg_bits(unsigned code, uint32_t value,
                     RegLogMode log_mode = LogRegWrites) {
    set_fpreg(code, value, log_mode);
  }

  void set_dreg(unsigned code, double value,
                RegLogMode log_mode = LogRegWrites) {
    set_fpreg(code, value, log_mode);
  }

  void set_dreg_bits(unsigned code, uint64_t value,
                     RegLogMode log_mode = LogRegWrites) {
    set_fpreg(code, value, log_mode);
  }

  bool N() const { return nzcv_.N() != 0; }
  bool Z() const { return nzcv_.Z() != 0; }
  bool C() const { return nzcv_.C() != 0; }
  bool V() const { return nzcv_.V() != 0; }
  SimSystemRegister& nzcv() { return nzcv_; }

  // TODO(jbramley): Find a way to make the fpcr_ members return the proper
  // types, so these accessors are not necessary.
  FPRounding RMode() { return static_cast<FPRounding>(fpcr_.RMode()); }
  bool DN() { return fpcr_.DN() != 0; }
  SimSystemRegister& fpcr() { return fpcr_; }

  // Print all registers of the specified types.
  void PrintRegisters();
  void PrintFPRegisters();
  void PrintSystemRegisters();

  // Like Print* (above), but respect trace_parameters().
  void LogSystemRegisters() {
    if (trace_parameters() & LOG_SYS_REGS) PrintSystemRegisters();
  }
  void LogRegisters() {
    if (trace_parameters() & LOG_REGS) PrintRegisters();
  }
  void LogFPRegisters() {
    if (trace_parameters() & LOG_FP_REGS) PrintFPRegisters();
  }

  // Specify relevant register sizes, for PrintFPRegister.
  //
  // These values are bit masks; they can be combined in case multiple views of
  // a machine register are interesting.
  enum PrintFPRegisterSizes {
    kPrintDRegValue = 1 << kDRegSizeInBytes,
    kPrintSRegValue = 1 << kSRegSizeInBytes,
    kPrintAllFPRegValues = kPrintDRegValue | kPrintSRegValue
  };

  // Print individual register values (after update).
  void PrintRegister(unsigned code, Reg31Mode r31mode = Reg31IsStackPointer);
  void PrintFPRegister(unsigned code,
                       PrintFPRegisterSizes sizes = kPrintAllFPRegValues);
  void PrintSystemRegister(SystemRegister id);

  // Like Print* (above), but respect trace_parameters().
  void LogRegister(unsigned code, Reg31Mode r31mode = Reg31IsStackPointer) {
    if (trace_parameters() & LOG_REGS) PrintRegister(code, r31mode);
  }
  void LogFPRegister(unsigned code,
                     PrintFPRegisterSizes sizes = kPrintAllFPRegValues) {
    if (trace_parameters() & LOG_FP_REGS) PrintFPRegister(code, sizes);
  }
  void LogSystemRegister(SystemRegister id) {
    if (trace_parameters() & LOG_SYS_REGS) PrintSystemRegister(id);
  }

  // Print memory accesses.
  void PrintRead(uintptr_t address, size_t size, unsigned reg_code);
  void PrintReadFP(uintptr_t address, size_t size, unsigned reg_code);
  void PrintWrite(uintptr_t address, size_t size, unsigned reg_code);
  void PrintWriteFP(uintptr_t address, size_t size, unsigned reg_code);

  // Like Print* (above), but respect trace_parameters().
  void LogRead(uintptr_t address, size_t size, unsigned reg_code) {
    if (trace_parameters() & LOG_REGS) PrintRead(address, size, reg_code);
  }
  void LogReadFP(uintptr_t address, size_t size, unsigned reg_code) {
    if (trace_parameters() & LOG_FP_REGS) PrintReadFP(address, size, reg_code);
  }
  void LogWrite(uintptr_t address, size_t size, unsigned reg_code) {
    if (trace_parameters() & LOG_WRITE) PrintWrite(address, size, reg_code);
  }
  void LogWriteFP(uintptr_t address, size_t size, unsigned reg_code) {
    if (trace_parameters() & LOG_WRITE) PrintWriteFP(address, size, reg_code);
  }

  void DoUnreachable(const Instruction* instr);
  void DoTrace(const Instruction* instr);
  void DoLog(const Instruction* instr);

  static const char* WRegNameForCode(unsigned code,
                                     Reg31Mode mode = Reg31IsZeroRegister);
  static const char* XRegNameForCode(unsigned code,
                                     Reg31Mode mode = Reg31IsZeroRegister);
  static const char* SRegNameForCode(unsigned code);
  static const char* DRegNameForCode(unsigned code);
  static const char* VRegNameForCode(unsigned code);

  bool coloured_trace() const { return coloured_trace_; }
  void set_coloured_trace(bool value);

  int trace_parameters() const { return trace_parameters_; }
  void set_trace_parameters(int parameters);

  void set_instruction_stats(bool value);

  // Clear the simulated local monitor to force the next store-exclusive
  // instruction to fail.
  void ClearLocalMonitor() {
    local_monitor_.Clear();
  }

  void SilenceExclusiveAccessWarning() {
    print_exclusive_access_warning_ = false;
  }

 protected:
  const char* clr_normal;
  const char* clr_flag_name;
  const char* clr_flag_value;
  const char* clr_reg_name;
  const char* clr_reg_value;
  const char* clr_fpreg_name;
  const char* clr_fpreg_value;
  const char* clr_memory_address;
  const char* clr_warning;
  const char* clr_warning_message;
  const char* clr_printf;

  // Simulation helpers ------------------------------------
  bool ConditionPassed(Condition cond) {
    switch (cond) {
      case eq:
        return Z();
      case ne:
        return !Z();
      case hs:
        return C();
      case lo:
        return !C();
      case mi:
        return N();
      case pl:
        return !N();
      case vs:
        return V();
      case vc:
        return !V();
      case hi:
        return C() && !Z();
      case ls:
        return !(C() && !Z());
      case ge:
        return N() == V();
      case lt:
        return N() != V();
      case gt:
        return !Z() && (N() == V());
      case le:
        return !(!Z() && (N() == V()));
      case nv:  // Fall through.
      case al:
        return true;
      default:
        VIXL_UNREACHABLE();
        return false;
    }
  }

  bool ConditionPassed(Instr cond) {
    return ConditionPassed(static_cast<Condition>(cond));
  }

  bool ConditionFailed(Condition cond) {
    return !ConditionPassed(cond);
  }

  void AddSubHelper(const Instruction* instr, int64_t op2);
  int64_t AddWithCarry(unsigned reg_size,
                       bool set_flags,
                       int64_t src1,
                       int64_t src2,
                       int64_t carry_in = 0);
  void LogicalHelper(const Instruction* instr, int64_t op2);
  void ConditionalCompareHelper(const Instruction* instr, int64_t op2);
  void LoadStoreHelper(const Instruction* instr,
                       int64_t offset,
                       AddrMode addrmode);
  void LoadStorePairHelper(const Instruction* instr, AddrMode addrmode);
  uintptr_t AddressModeHelper(unsigned addr_reg,
                              int64_t offset,
                              AddrMode addrmode);

  uint64_t AddressUntag(uint64_t address) {
    return address & ~kAddressTagMask;
  }

  template <typename T>
  T* AddressUntag(T* address) {
    uintptr_t address_raw = reinterpret_cast<uintptr_t>(address);
    return reinterpret_cast<T*>(AddressUntag(address_raw));
  }

  template <typename T, typename A>
  T MemoryRead(A address) {
    T value;
    address = AddressUntag(address);
    VIXL_ASSERT((sizeof(value) == 1) || (sizeof(value) == 2) ||
                (sizeof(value) == 4) || (sizeof(value) == 8));
    memcpy(&value, reinterpret_cast<const char *>(address), sizeof(value));
    return value;
  }

  template <typename T, typename A>
  void MemoryWrite(A address, T value) {
    address = AddressUntag(address);
    VIXL_ASSERT((sizeof(value) == 1) || (sizeof(value) == 2) ||
                (sizeof(value) == 4) || (sizeof(value) == 8));
    memcpy(reinterpret_cast<char *>(address), &value, sizeof(value));
  }

  int64_t ShiftOperand(unsigned reg_size,
                       int64_t value,
                       Shift shift_type,
                       unsigned amount);
  int64_t Rotate(unsigned reg_width,
                 int64_t value,
                 Shift shift_type,
                 unsigned amount);
  int64_t ExtendValue(unsigned reg_width,
                      int64_t value,
                      Extend extend_type,
                      unsigned left_shift = 0);

  enum ReverseByteMode {
    Reverse16 = 0,
    Reverse32 = 1,
    Reverse64 = 2
  };
  uint64_t ReverseBytes(uint64_t value, ReverseByteMode mode);
  uint64_t ReverseBits(uint64_t value, unsigned num_bits);

  template <typename T>
  T FPDefaultNaN() const;

  void FPCompare(double val0, double val1);
  double FPRoundInt(double value, FPRounding round_mode);
  double FPToDouble(float value);
  float FPToFloat(double value, FPRounding round_mode);
  double FixedToDouble(int64_t src, int fbits, FPRounding round_mode);
  double UFixedToDouble(uint64_t src, int fbits, FPRounding round_mode);
  float FixedToFloat(int64_t src, int fbits, FPRounding round_mode);
  float UFixedToFloat(uint64_t src, int fbits, FPRounding round_mode);
  int32_t FPToInt32(double value, FPRounding rmode);
  int64_t FPToInt64(double value, FPRounding rmode);
  uint32_t FPToUInt32(double value, FPRounding rmode);
  uint64_t FPToUInt64(double value, FPRounding rmode);

  template <typename T>
  T FPAdd(T op1, T op2);

  template <typename T>
  T FPDiv(T op1, T op2);

  template <typename T>
  T FPMax(T a, T b);

  template <typename T>
  T FPMaxNM(T a, T b);

  template <typename T>
  T FPMin(T a, T b);

  template <typename T>
  T FPMinNM(T a, T b);

  template <typename T>
  T FPMul(T op1, T op2);

  template <typename T>
  T FPMulAdd(T a, T op1, T op2);

  template <typename T>
  T FPSqrt(T op);

  template <typename T>
  T FPSub(T op1, T op2);

  // This doesn't do anything at the moment. We'll need it if we want support
  // for cumulative exception bits or floating-point exceptions.
  void FPProcessException() { }

  // Standard NaN processing.
  template <typename T>
  T FPProcessNaN(T op);

  bool FPProcessNaNs(const Instruction* instr);

  template <typename T>
  T FPProcessNaNs(T op1, T op2);

  template <typename T>
  T FPProcessNaNs3(T op1, T op2, T op3);

  // Pseudo Printf instruction
  void DoPrintf(const Instruction* instr);

  // Processor state ---------------------------------------

  // Simulated monitors for exclusive access instructions.
  SimExclusiveLocalMonitor local_monitor_;
  SimExclusiveGlobalMonitor global_monitor_;

  // Output stream.
  FILE* stream_;
  PrintDisassembler* print_disasm_;

  // Instruction statistics instrumentation.
  Instrument* instrumentation_;

  // General purpose registers. Register 31 is the stack pointer.
  SimRegister registers_[kNumberOfRegisters];

  // Floating point registers
  SimFPRegister fpregisters_[kNumberOfFPRegisters];

  // Program Status Register.
  // bits[31, 27]: Condition flags N, Z, C, and V.
  //               (Negative, Zero, Carry, Overflow)
  SimSystemRegister nzcv_;

  // Floating-Point Control Register
  SimSystemRegister fpcr_;

  // Only a subset of FPCR features are supported by the simulator. This helper
  // checks that the FPCR settings are supported.
  //
  // This is checked when floating-point instructions are executed, not when
  // FPCR is set. This allows generated code to modify FPCR for external
  // functions, or to save and restore it when entering and leaving generated
  // code.
  void AssertSupportedFPCR() {
    VIXL_ASSERT(fpcr().FZ() == 0);             // No flush-to-zero support.
    VIXL_ASSERT(fpcr().RMode() == FPTieEven);  // Ties-to-even rounding only.

    // The simulator does not support half-precision operations so fpcr().AHP()
    // is irrelevant, and is not checked here.
  }

  static int CalcNFlag(uint64_t result, unsigned reg_size) {
    return (result >> (reg_size - 1)) & 1;
  }

  static int CalcZFlag(uint64_t result) {
    return result == 0;
  }

  static const uint32_t kConditionFlagsMask = 0xf0000000;

  // Stack
  byte* stack_;
  static const int stack_protection_size_ = 128 * KBytes;
  static const int stack_size_ = (2 * MBytes) + (2 * stack_protection_size_);
  byte* stack_limit_;

  Decoder* decoder_;
  // Indicates if the pc has been modified by the instruction and should not be
  // automatically incremented.
  bool pc_modified_;
  const Instruction* pc_;
  const Instruction* resume_pc_;

  static const char* xreg_names[];
  static const char* wreg_names[];
  static const char* sreg_names[];
  static const char* dreg_names[];
  static const char* vreg_names[];

  static const Instruction* kEndOfSimAddress;

 private:
  bool coloured_trace_;

  // A set of TraceParameters flags.
  int trace_parameters_;

  // Indicates whether the instruction instrumentation is active.
  bool instruction_stats_;

  // Indicates whether the exclusive-access warning has been printed.
  bool print_exclusive_access_warning_;
  void PrintExclusiveAccessWarning();

  // Indicates that the simulator ran out of memory at some point.
  // Data structures may not be fully allocated.
  bool oom_;

 public:
  // True if the simulator ran out of memory during or after construction.
  bool oom() const { return oom_; }

 protected:
  // Moz: Synchronizes access between main thread and compilation threads.
  PRLock* lock_;
#ifdef DEBUG
  PRThread* lockOwner_;
#endif
  Redirection* redirection_;
  mozilla::Vector<int64_t, 0, js::SystemAllocPolicy> spStack_;
};
}  // namespace vixl

#endif  // JS_SIMULATOR_ARM64
#endif  // VIXL_A64_SIMULATOR_A64_H_
