/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
 *   Mike Shaver <shaver@mozilla.org>
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jstracer_h___
#define jstracer_h___

#ifdef JS_TRACER

#include "jstypes.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsdhash.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsvector.h"

namespace js {

#if defined(DEBUG) && !defined(JS_JIT_SPEW)
#define JS_JIT_SPEW
#endif

template <typename T>
class Queue {
    T* _data;
    unsigned _len;
    unsigned _max;
    nanojit::Allocator* alloc;

public:
    void ensure(unsigned size) {
        if (_max > size)
            return;
        if (!_max)
            _max = 8;
        _max = JS_MAX(_max * 2, size);
        if (alloc) {
            T* tmp = new (*alloc) T[_max];
            memcpy(tmp, _data, _len * sizeof(T));
            _data = tmp;
        } else {
            _data = (T*)js_realloc(_data, _max * sizeof(T));
        }
#if defined(DEBUG)
        memset(&_data[_len], 0xcd, _max - _len);
#endif
    }

    Queue(nanojit::Allocator* alloc)
        : alloc(alloc)
    {
        this->_max =
        this->_len = 0;
        this->_data = NULL;
    }

    ~Queue() {
        if (!alloc)
            js_free(_data);
    }

    bool contains(T a) {
        for (unsigned n = 0; n < _len; ++n) {
            if (_data[n] == a)
                return true;
        }
        return false;
    }

    void add(T a) {
        ensure(_len + 1);
        JS_ASSERT(_len <= _max);
        _data[_len++] = a;
    }

    void add(T* chunk, unsigned size) {
        ensure(_len + size);
        JS_ASSERT(_len <= _max);
        memcpy(&_data[_len], chunk, size * sizeof(T));
        _len += size;
    }

    void addUnique(T a) {
        if (!contains(a))
            add(a);
    }

    void setLength(unsigned len) {
        ensure(len + 1);
        _len = len;
    }

    void clear() {
        _len = 0;
    }

    T & get(unsigned i) {
        JS_ASSERT(i < length());
        return _data[i];
    }

    const T & get(unsigned i) const {
        JS_ASSERT(i < length());
        return _data[i];
    }

    T & operator [](unsigned i) {
        return get(i);
    }

    const T & operator [](unsigned i) const {
        return get(i);
    }

    unsigned length() const {
        return _len;
    }

    T* data() const {
        return _data;
    }

    int offsetOf(T slot) {
        T* p = _data;
        unsigned n = 0;
        for (n = 0; n < _len; ++n)
            if (*p++ == slot)
                return n;
        return -1;
    }

};

/*
 * Tracker is used to keep track of values being manipulated by the interpreter
 * during trace recording.  It maps opaque, 4-byte aligned address to LIns pointers.
 * pointers. To do this efficiently, we observe that the addresses of jsvals
 * living in the interpreter tend to be aggregated close to each other -
 * usually on the same page (where a tracker page doesn't have to be the same
 * size as the OS page size, but it's typically similar).  The Tracker
 * consists of a linked-list of structures representing a memory page, which
 * are created on-demand as memory locations are used.
 *
 * For every address, first we split it into two parts: upper bits which
 * represent the "base", and lower bits which represent an offset against the
 * base.  For the offset, we then right-shift it by two because the bottom two
 * bits of a 4-byte aligned address are always zero.  The mapping then
 * becomes:
 *
 *   page = page in pagelist such that Base(address) == page->base,
 *   page->map[Offset(address)]
 */
class Tracker {
    #define TRACKER_PAGE_SZB        4096
    #define TRACKER_PAGE_ENTRIES    (TRACKER_PAGE_SZB >> 2)    // each slot is 4 bytes
    #define TRACKER_PAGE_MASK       jsuword(TRACKER_PAGE_SZB - 1)

    struct TrackerPage {
        struct TrackerPage* next;
        jsuword             base;
        nanojit::LIns*      map[TRACKER_PAGE_ENTRIES];
    };
    struct TrackerPage* pagelist;

    jsuword             getTrackerPageBase(const void* v) const;
    jsuword             getTrackerPageOffset(const void* v) const;
    struct TrackerPage* findTrackerPage(const void* v) const;
    struct TrackerPage* addTrackerPage(const void* v);
public:
    Tracker();
    ~Tracker();

    bool            has(const void* v) const;
    nanojit::LIns*  get(const void* v) const;
    void            set(const void* v, nanojit::LIns* ins);
    void            clear();
};

class VMFragment : public nanojit::Fragment {
public:
    VMFragment(const void* _ip verbose_only(, uint32_t profFragID))
      : Fragment(_ip verbose_only(, profFragID))
    {}

    /*
     * If this is anchored off a TreeFragment, this points to that tree fragment.
     * Otherwise, it is |this|.
     */
    TreeFragment* root;

    TreeFragment* toTreeFragment();
};

#if defined(JS_JIT_SPEW) || defined(NJ_NO_VARIADIC_MACROS)

enum LC_TMBits {
    /*
     * Output control bits for all non-Nanojit code.  Only use bits 16 and
     * above, since Nanojit uses 0 .. 15 itself.
     */
    LC_TMMinimal  = 1<<16,
    LC_TMTracer   = 1<<17,
    LC_TMRecorder = 1<<18,
    LC_TMAbort    = 1<<19,
    LC_TMStats    = 1<<20,
    LC_TMRegexp   = 1<<21,
    LC_TMTreeVis  = 1<<22
};

#endif

#ifdef NJ_NO_VARIADIC_MACROS

#define debug_only_stmt(action)            /* */
static void debug_only_printf(int mask, const char *fmt, ...) {}
#define debug_only_print0(mask, str)       /* */

#elif defined(JS_JIT_SPEW)

// Top level logging controller object.
extern nanojit::LogControl LogController;

// Top level profiling hook, needed to harvest profile info from Fragments
// whose logical lifetime is about to finish
extern void FragProfiling_FragFinalizer(nanojit::Fragment* f, TraceMonitor*);

#define debug_only_stmt(stmt) \
    stmt

#define debug_only_printf(mask, fmt, ...)                                      \
    JS_BEGIN_MACRO                                                             \
        if ((LogController.lcbits & (mask)) > 0) {                             \
            LogController.printf(fmt, __VA_ARGS__);                            \
            fflush(stdout);                                                    \
        }                                                                      \
    JS_END_MACRO

#define debug_only_print0(mask, str)                                           \
    JS_BEGIN_MACRO                                                             \
        if ((LogController.lcbits & (mask)) > 0) {                             \
            LogController.printf("%s", str);                                   \
            fflush(stdout);                                                    \
        }                                                                      \
    JS_END_MACRO

#else

#define debug_only_stmt(action)            /* */
#define debug_only_printf(mask, fmt, ...)  /* */
#define debug_only_print0(mask, str)       /* */

#endif

/*
 * The oracle keeps track of hit counts for program counter locations, as
 * well as slots that should not be demoted to int because we know them to
 * overflow or they result in type-unstable traces. We are using simple
 * hash tables.  Collisions lead to loss of optimization (demotable slots
 * are not demoted, etc.) but have no correctness implications.
 */
#define ORACLE_SIZE 4096

class Oracle {
    avmplus::BitSet _stackDontDemote;
    avmplus::BitSet _globalDontDemote;
    avmplus::BitSet _pcDontDemote;
    avmplus::BitSet _pcSlowZeroTest;
public:
    Oracle();

    JS_REQUIRES_STACK void markGlobalSlotUndemotable(JSContext* cx, unsigned slot);
    JS_REQUIRES_STACK bool isGlobalSlotUndemotable(JSContext* cx, unsigned slot) const;
    JS_REQUIRES_STACK void markStackSlotUndemotable(JSContext* cx, unsigned slot);
    JS_REQUIRES_STACK void markStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc);
    JS_REQUIRES_STACK bool isStackSlotUndemotable(JSContext* cx, unsigned slot) const;
    JS_REQUIRES_STACK bool isStackSlotUndemotable(JSContext* cx, unsigned slot, const void* pc) const;
    void markInstructionUndemotable(jsbytecode* pc);
    bool isInstructionUndemotable(jsbytecode* pc) const;
    void markInstructionSlowZeroTest(jsbytecode* pc);
    bool isInstructionSlowZeroTest(jsbytecode* pc) const;

    void clearDemotability();
    void clear() {
        clearDemotability();
    }
};

typedef Queue<uint16> SlotList;

class TypeMap : public Queue<JSValueType> {
    Oracle *oracle;
public:
    TypeMap(nanojit::Allocator* alloc) : Queue<JSValueType>(alloc) {}
    void set(unsigned stackSlots, unsigned ngslots,
             const JSValueType* stackTypeMap, const JSValueType* globalTypeMap);
    JS_REQUIRES_STACK void captureTypes(JSContext* cx, JSObject* globalObj, SlotList& slots, unsigned callDepth,
                                        bool speculate);
    JS_REQUIRES_STACK void captureMissingGlobalTypes(JSContext* cx, JSObject* globalObj, SlotList& slots,
                                                     unsigned stackSlots, bool speculate);
    bool matches(TypeMap& other) const;
    void fromRaw(JSValueType* other, unsigned numSlots);
};

#define JS_TM_EXITCODES(_)    \
    /*                                                                          \
     * An exit at a possible branch-point in the trace at which to attach a     \
     * future secondary trace. Therefore the recorder must generate different   \
     * code to handle the other outcome of the branch condition from the        \
     * primary trace's outcome.                                                 \
     */                                                                         \
    _(BRANCH)                                                                   \
    /*                                                                          \
     * Exit at a tableswitch via a numbered case.                               \
     */                                                                         \
    _(CASE)                                                                     \
    /*                                                                          \
     * Exit at a tableswitch via the default case.                              \
     */                                                                         \
    _(DEFAULT)                                                                  \
    _(LOOP)                                                                     \
    _(NESTED)                                                                   \
    /*                                                                          \
     * An exit from a trace because a condition relied upon at recording time   \
     * no longer holds, where the alternate path of execution is so rare or     \
     * difficult to address in native code that it is not traced at all, e.g.   \
     * negative array index accesses, which differ from positive indexes in     \
     * that they require a string-based property lookup rather than a simple    \
     * memory access.                                                           \
     */                                                                         \
    _(MISMATCH)                                                                 \
    /*                                                                          \
     * A specialization of MISMATCH_EXIT to handle allocation failures.         \
     */                                                                         \
    _(OOM)                                                                      \
    _(OVERFLOW)                                                                 \
    _(MUL_ZERO)                                                                 \
    _(UNSTABLE_LOOP)                                                            \
    _(TIMEOUT)                                                                  \
    _(DEEP_BAIL)                                                                \
    _(STATUS)

enum ExitType {
    #define MAKE_EXIT_CODE(x) x##_EXIT,
    JS_TM_EXITCODES(MAKE_EXIT_CODE)
    #undef MAKE_EXIT_CODE
    TOTAL_EXIT_TYPES
};

struct FrameInfo;

struct VMSideExit : public nanojit::SideExit
{
    jsbytecode* pc;
    jsbytecode* imacpc;
    intptr_t sp_adj;
    intptr_t rp_adj;
    int32_t calldepth;
    uint32 numGlobalSlots;
    uint32 numStackSlots;
    uint32 numStackSlotsBelowCurrentFrame;
    ExitType exitType;
    uintN lookupFlags;
    unsigned hitcount;

    inline JSValueType* stackTypeMap() {
        return (JSValueType*)(this + 1);
    }

    inline JSValueType& stackType(unsigned i) {
        JS_ASSERT(i < numStackSlots);
        return stackTypeMap()[i];
    }

    inline JSValueType* globalTypeMap() {
        return (JSValueType*)(this + 1) + this->numStackSlots;
    }

    inline JSValueType* fullTypeMap() {
        return stackTypeMap();
    }

    inline VMFragment* fromFrag() {
        return (VMFragment*)from;
    }

    inline TreeFragment* root() {
        return fromFrag()->root;
    }
};

class VMAllocator : public nanojit::Allocator
{

public:
    VMAllocator() : mOutOfMemory(false), mSize(0)
    {}

    size_t size() {
        return mSize;
    }

    bool outOfMemory() {
        return mOutOfMemory;
    }

    struct Mark
    {
        VMAllocator& vma;
        bool committed;
        nanojit::Allocator::Chunk* saved_chunk;
        char* saved_top;
        char* saved_limit;
        size_t saved_size;

        Mark(VMAllocator& vma) :
            vma(vma),
            committed(false),
            saved_chunk(vma.current_chunk),
            saved_top(vma.current_top),
            saved_limit(vma.current_limit),
            saved_size(vma.mSize)
        {}

        ~Mark()
        {
            if (!committed)
                vma.rewind(*this);
        }

        void commit() { committed = true; }
    };

    void rewind(const Mark& m) {
        while (current_chunk != m.saved_chunk) {
            Chunk *prev = current_chunk->prev;
            freeChunk(current_chunk);
            current_chunk = prev;
        }
        current_top = m.saved_top;
        current_limit = m.saved_limit;
        mSize = m.saved_size;
        memset(current_top, 0, current_limit - current_top);
    }

    bool mOutOfMemory;
    size_t mSize;

    /*
     * FIXME: Area the LIR spills into if we encounter an OOM mid-way
     * through compilation; we must check mOutOfMemory before we run out
     * of mReserve, otherwise we're in undefined territory. This area
     * used to be one page, now 16 to be "safer". This is a temporary
     * and quite unsatisfactory approach to handling OOM in Nanojit.
     */
    uintptr_t mReserve[0x10000];
};

struct REHashKey {
    size_t re_length;
    uint16 re_flags;
    const jschar* re_chars;

    REHashKey(size_t re_length, uint16 re_flags, const jschar *re_chars)
        : re_length(re_length)
        , re_flags(re_flags)
        , re_chars(re_chars)
    {}

    bool operator==(const REHashKey& other) const
    {
        return ((this->re_length == other.re_length) &&
                (this->re_flags == other.re_flags) &&
                !memcmp(this->re_chars, other.re_chars,
                        this->re_length * sizeof(jschar)));
    }
};

struct REHashFn {
    static size_t hash(const REHashKey& k) {
        return
            k.re_length +
            k.re_flags +
            nanojit::murmurhash(k.re_chars, k.re_length * sizeof(jschar));
    }
};

struct FrameInfo {
    JSObject*       block;      // caller block chain head
    jsbytecode*     pc;         // caller fp->regs->pc
    jsbytecode*     imacpc;     // caller fp->imacpc
    uint32          spdist;     // distance from fp->slots to fp->regs->sp at JSOP_CALL

    /*
     * Bit  15 (0x8000) is a flag that is set if constructing (called through new).
     * Bits 0-14 are the actual argument count. This may be less than fun->nargs.
     * NB: This is argc for the callee, not the caller.
     */
    uint32          argc;

    /*
     * Number of stack slots in the caller, not counting slots pushed when
     * invoking the callee. That is, slots after JSOP_CALL completes but
     * without the return value. This is also equal to the number of slots
     * between fp->prev->argv[-2] (calleR fp->callee) and fp->argv[-2]
     * (calleE fp->callee).
     */
    uint32          callerHeight;

    /* argc of the caller */
    uint32          callerArgc;

    // Safer accessors for argc.
    enum { CONSTRUCTING_FLAG = 0x10000 };
    void   set_argc(uint16 argc, bool constructing) {
        this->argc = uint32(argc) | (constructing ? CONSTRUCTING_FLAG: 0);
    }
    uint16 get_argc() const { return uint16(argc & ~CONSTRUCTING_FLAG); }
    bool   is_constructing() const { return (argc & CONSTRUCTING_FLAG) != 0; }

    // The typemap just before the callee is called.
    JSValueType* get_typemap() { return (JSValueType*) (this+1); }
    const JSValueType* get_typemap() const { return (JSValueType*) (this+1); }
};

struct UnstableExit
{
    VMFragment* fragment;
    VMSideExit* exit;
    UnstableExit* next;
};

struct LinkableFragment : public VMFragment
{
    LinkableFragment(const void* _ip, nanojit::Allocator* alloc
                     verbose_only(, uint32_t profFragID))
      : VMFragment(_ip verbose_only(, profFragID)), typeMap(alloc), nStackTypes(0)
    { }

    uint32                  branchCount;
    TypeMap                 typeMap;
    unsigned                nStackTypes;
    unsigned                spOffsetAtEntry;
    SlotList*               globalSlots;
};

/*
 * argc is cx->fp->argc at the trace loop header, i.e., the number of arguments
 * pushed for the innermost JS frame. This is required as part of the fragment
 * key because the fragment will write those arguments back to the interpreter
 * stack when it exits, using its typemap, which implicitly incorporates a
 * given value of argc. Without this feature, a fragment could be called as an
 * inner tree with two different values of argc, and entry type checking or
 * exit frame synthesis could crash.
 */
struct TreeFragment : public LinkableFragment
{
    TreeFragment(const void* _ip, nanojit::Allocator* alloc, JSObject* _globalObj,
                 uint32 _globalShape, uint32 _argc verbose_only(, uint32_t profFragID)):
        LinkableFragment(_ip, alloc verbose_only(, profFragID)),
        first(NULL),
        next(NULL),
        peer(NULL),
        globalObj(_globalObj),
        globalShape(_globalShape),
        argc(_argc),
        dependentTrees(alloc),
        linkedTrees(alloc),
        sideExits(alloc),
        gcthings(alloc),
        shapes(alloc)
    { }

    TreeFragment* first;
    TreeFragment* next;
    TreeFragment* peer;
    JSObject* globalObj;
    uint32 globalShape;
    uint32 argc;
    /* Dependent trees must be trashed if this tree dies, and updated on missing global types */
    Queue<TreeFragment*>    dependentTrees;
    /* Linked trees must be updated on missing global types, but are not dependent */
    Queue<TreeFragment*>    linkedTrees;
#ifdef DEBUG
    const char*             treeFileName;
    uintN                   treeLineNumber;
    uintN                   treePCOffset;
#endif
    JSScript*               script;
    UnstableExit*           unstableExits;
    Queue<VMSideExit*>      sideExits;
    ptrdiff_t               nativeStackBase;
    unsigned                maxCallDepth;
    /* All embedded GC things are registered here so the GC can scan them. */
    Queue<Value>            gcthings;
    Queue<const js::Shape*> shapes;
    unsigned                maxNativeStackSlots;
    uintN                   execs;

    inline unsigned nGlobalTypes() {
        return typeMap.length() - nStackTypes;
    }
    inline JSValueType* globalTypeMap() {
        return typeMap.data() + nStackTypes;
    }
    inline JSValueType* stackTypeMap() {
        return typeMap.data();
    }

    JS_REQUIRES_STACK void initialize(JSContext* cx, SlotList *globalSlots, bool speculate);
    UnstableExit* removeUnstableExit(VMSideExit* exit);
};

inline TreeFragment*
VMFragment::toTreeFragment()
{
    JS_ASSERT(root == this);
    return static_cast<TreeFragment*>(this);
}

/*
 * BUILTIN_NO_FIXUP_NEEDED indicates that after the initial LeaveTree of a deep
 * bail, the builtin call needs no further fixup when the trace exits and calls
 * LeaveTree the second time.
 */
typedef enum BuiltinStatus {
    BUILTIN_BAILED = 1,
    BUILTIN_ERROR = 2,
    BUILTIN_NO_FIXUP_NEEDED = 4,

    BUILTIN_ERROR_NO_FIXUP_NEEDED = BUILTIN_ERROR | BUILTIN_NO_FIXUP_NEEDED
} BuiltinStatus;

static JS_INLINE void
SetBuiltinError(JSContext *cx, BuiltinStatus status = BUILTIN_ERROR)
{
    cx->tracerState->builtinStatus |= status;
}

#ifdef DEBUG_RECORDING_STATUS_NOT_BOOL
/* #define DEBUG_RECORDING_STATUS_NOT_BOOL to detect misuses of RecordingStatus */
struct RecordingStatus {
    int code;
    bool operator==(RecordingStatus &s) { return this->code == s.code; };
    bool operator!=(RecordingStatus &s) { return this->code != s.code; };
};
enum RecordingStatusCodes {
    RECORD_ERROR_code     = 0,
    RECORD_STOP_code      = 1,

    RECORD_CONTINUE_code  = 3,
    RECORD_IMACRO_code    = 4
};
RecordingStatus RECORD_CONTINUE = { RECORD_CONTINUE_code };
RecordingStatus RECORD_STOP     = { RECORD_STOP_code };
RecordingStatus RECORD_IMACRO   = { RECORD_IMACRO_code };
RecordingStatus RECORD_ERROR    = { RECORD_ERROR_code };

struct AbortableRecordingStatus {
    int code;
    bool operator==(AbortableRecordingStatus &s) { return this->code == s.code; };
    bool operator!=(AbortableRecordingStatus &s) { return this->code != s.code; };
};
enum AbortableRecordingStatusCodes {
    ARECORD_ERROR_code     = 0,
    ARECORD_STOP_code      = 1,
    ARECORD_ABORTED_code   = 2,
    ARECORD_CONTINUE_code  = 3,
    ARECORD_IMACRO_code    = 4,
    ARECORD_IMACRO_ABORTED_code = 5,
    ARECORD_COMPLETED_code = 6
};
AbortableRecordingStatus ARECORD_ERROR    = { ARECORD_ERROR_code };
AbortableRecordingStatus ARECORD_STOP     = { ARECORD_STOP_code };
AbortableRecordingStatus ARECORD_CONTINUE = { ARECORD_CONTINUE_code };
AbortableRecordingStatus ARECORD_IMACRO   = { ARECORD_IMACRO_code };
AbortableRecordingStatus ARECORD_IMACRO_ABORTED   = { ARECORD_IMACRO_ABORTED_code };
AbortableRecordingStatus ARECORD_ABORTED =  { ARECORD_ABORTED_code };
AbortableRecordingStatus ARECORD_COMPLETED =  { ARECORD_COMPLETED_code };

static inline AbortableRecordingStatus
InjectStatus(RecordingStatus rs)
{
    AbortableRecordingStatus ars = { rs.code };
    return ars;
}
static inline AbortableRecordingStatus
InjectStatus(AbortableRecordingStatus ars)
{
    return ars;
}

static inline bool
StatusAbortsRecorderIfActive(AbortableRecordingStatus ars)
{
    return ars == ARECORD_ERROR || ars == ARECORD_STOP;
}
#else

/*
 * Normally, during recording, when the recorder cannot continue, it returns
 * ARECORD_STOP to indicate that recording should be aborted by the top-level
 * recording function. However, if the recorder reenters the interpreter (e.g.,
 * when executing an inner loop), there will be an immediate abort. This
 * condition must be carefully detected and propagated out of all nested
 * recorder calls lest the now-invalid TraceRecorder object be accessed
 * accidentally. This condition is indicated by the ARECORD_ABORTED value.
 *
 * The AbortableRecordingStatus enumeration represents the general set of
 * possible results of calling a recorder function. Functions that cannot
 * possibly return ARECORD_ABORTED may statically guarantee this to the caller
 * using the RecordingStatus enumeration. Ideally, C++ would allow subtyping
 * of enumerations, but it doesn't. To simulate subtype conversion manually,
 * code should call InjectStatus to inject a value of the restricted set into a
 * value of the general set.
 */

enum RecordingStatus {
    RECORD_STOP       = 0,  // Recording should be aborted at the top-level
                            // call to the recorder.
    RECORD_ERROR      = 1,  // Recording should be aborted at the top-level
                            // call to the recorder and the interpreter should
                            // goto error
    RECORD_CONTINUE   = 2,  // Continue recording.
    RECORD_IMACRO     = 3   // Entered imacro; continue recording.
                            // Only JSOP_IS_IMACOP opcodes may return this.
};

enum AbortableRecordingStatus {
    ARECORD_STOP      = 0,  // see RECORD_STOP
    ARECORD_ERROR     = 1,  // Recording may or may not have been aborted.
                            // Recording should be aborted at the top-level
                            // if it has not already been and the interpreter
                            // should goto error
    ARECORD_CONTINUE  = 2,  // see RECORD_CONTINUE
    ARECORD_IMACRO    = 3,  // see RECORD_IMACRO
    ARECORD_IMACRO_ABORTED = 4, // see comment in TR::monitorRecording.
    ARECORD_ABORTED   = 5,  // Recording has already been aborted; the
                            // interpreter should continue executing
    ARECORD_COMPLETED = 6   // Recording completed successfully, the
                            // trace recorder has been deleted
};

static JS_ALWAYS_INLINE AbortableRecordingStatus
InjectStatus(RecordingStatus rs)
{
    return static_cast<AbortableRecordingStatus>(rs);
}

static JS_ALWAYS_INLINE AbortableRecordingStatus
InjectStatus(AbortableRecordingStatus ars)
{
    return ars;
}

/*
 * Return whether the recording status requires the current recording session
 * to be deleted. ERROR means the recording session should be deleted if it
 * hasn't already. ABORTED and COMPLETED indicate the recording session is
 * already deleted, so they return 'false'.
 */
static JS_ALWAYS_INLINE bool
StatusAbortsRecorderIfActive(AbortableRecordingStatus ars)
{
    return ars <= ARECORD_ERROR;
}
#endif

class SlotMap;
class SlurpInfo;

/* Results of trying to compare two typemaps together */
enum TypeConsensus
{
    TypeConsensus_Okay,         /* Two typemaps are compatible */
    TypeConsensus_Undemotes,    /* Not compatible now, but would be with pending undemotes. */
    TypeConsensus_Bad           /* Typemaps are not compatible */
};

enum MonitorResult {
    MONITOR_RECORDING,
    MONITOR_NOT_RECORDING,
    MONITOR_ERROR
};

enum TracePointAction {
    TPA_Nothing,
    TPA_RanStuff,
    TPA_Recorded,
    TPA_Error
};

typedef HashMap<nanojit::LIns*, JSObject*> GuardedShapeTable;

#ifdef DEBUG
# define AbortRecording(cx, reason) AbortRecordingImpl(cx, reason)
#else
# define AbortRecording(cx, reason) AbortRecordingImpl(cx)
#endif

class TraceRecorder
{
    /*************************************************************** Recording session constants */

    /* The context in which recording started. */
    JSContext* const                cx;

    /* Cached value of JS_TRACE_MONITOR(cx). */
    TraceMonitor* const             traceMonitor;

    /* Cached oracle keeps track of hit counts for program counter locations */
    Oracle*                         oracle;

    /* The Fragment being recorded by this recording session. */
    VMFragment* const               fragment;

    /* The root fragment representing the tree. */
    TreeFragment* const             tree;

    /* The global object from the start of recording until now. */
    JSObject* const                 globalObj;

    /* If non-null, the (pc of the) outer loop aborted to start recording this loop. */
    jsbytecode* const               outer;

    /* If |outer|, the argc to use when looking up |outer| in the fragments table. */
    uint32 const                    outerArgc;

    /* If non-null, the side exit from which we are growing. */
    VMSideExit* const               anchor;

    /* The LIR-generation pipeline used to build |fragment|. */
    nanojit::LirWriter* const       lir;
    nanojit::CseFilter* const       cse_filter;

    /* Instructions yielding the corresponding trace-const members of TracerState. */
    nanojit::LIns* const            cx_ins;
    nanojit::LIns* const            eos_ins;
    nanojit::LIns* const            eor_ins;
    nanojit::LIns* const            loopLabel;

    /* Lazy slot import state. */
    unsigned                        importStackSlots;
    unsigned                        importGlobalSlots;
    TypeMap                         importTypeMap;

    /*
     * The LirBuffer used to supply memory to our LirWriter pipeline. Also contains the most recent
     * instruction for {sp, rp, state}. Also contains names for debug JIT spew. Should be split.
     */
    nanojit::LirBuffer* const       lirbuf;

    /*
     * Remembers traceAlloc state before recording started; automatically rewinds when mark is
     * destroyed on a failed compilation.
     */
    VMAllocator::Mark               mark;

    /* Remembers the number of sideExits in treeInfo before recording started. */
    const unsigned                  numSideExitsBefore;

    /*********************************************************** Recording session mutable state */

    /* Maps interpreter stack values to the instruction generating that value. */
    Tracker                         tracker;

    /* Maps interpreter stack values to the instruction writing back to the native stack. */
    Tracker                         nativeFrameTracker;

    /* The start of the global object's slots we assume for the trackers. */
    Value*                          global_slots;

    /* The number of interpreted calls entered (and not yet left) since recording began. */
    unsigned                        callDepth;

    /* The current atom table, mirroring the interpreter loop's variable of the same name. */
    JSAtom**                        atoms;
    Value*                          consts;

    /* An instruction yielding the current script's strict mode code flag.  */
    nanojit::LIns*                  strictModeCode_ins;

    /* FIXME: Dead, but soon to be used for something or other. */
    Queue<jsbytecode*>              cfgMerges;

    /* Indicates whether the current tree should be trashed when the recording session ends. */
    bool                            trashSelf;

    /* A list of trees to trash at the end of the recording session. */
    Queue<TreeFragment*>            whichTreesToTrash;

    /* The set of objects whose shapes already have been guarded. */
    GuardedShapeTable               guardedShapeTable;

    /***************************************** Temporal state hoisted into the recording session */

    /* Carry the return value from a STOP/RETURN to the subsequent record_LeaveFrame. */
    nanojit::LIns*                  rval_ins;

    /* Carry the return value from a native call to the record_NativeCallComplete. */
    nanojit::LIns*                  native_rval_ins;

    /* Carry the return value of js_CreateThis to record_NativeCallComplete. */
    nanojit::LIns*                  newobj_ins;

    /* Carry the JSSpecializedNative used to generate a call to record_NativeCallComplete. */
    JSSpecializedNative*            pendingSpecializedNative;

    /* Carry whether this is a jsval on the native stack from finishGetProp to monitorRecording. */
    Value*                          pendingUnboxSlot;

    /* Carry a guard condition to the beginning of the next monitorRecording. */
    nanojit::LIns*                  pendingGuardCondition;

    /* Carry whether we have an always-exit from emitIf to checkTraceEnd. */
    bool                            pendingLoop;

    /* Temporary JSSpecializedNative used to describe non-specialized fast natives. */
    JSSpecializedNative             generatedSpecializedNative;

    /* Temporary JSValueType array used to construct temporary typemaps. */
    js::Vector<JSValueType, 256>    tempTypeMap;

    /************************************************************* 10 bajillion member functions */

    /* 
     * These can be put around a control-flow diamond if it's important that
     * CSE work across the diamond.  Duplicated expressions within the diamond
     * will be CSE'd, but expressions defined within the diamond won't be
     * added to the tables of CSEable expressions.  Loads are still
     * invalidated if they alias any stores that occur within diamonds.
     */
    void suspendCSE()   { if (cse_filter) cse_filter->suspend(); }
    void resumeCSE()    { if (cse_filter) cse_filter->resume();  }

    nanojit::LIns* insImmVal(const Value& val);
    nanojit::LIns* insImmObj(JSObject* obj);
    nanojit::LIns* insImmFun(JSFunction* fun);
    nanojit::LIns* insImmStr(JSString* str);
    nanojit::LIns* insImmShape(const js::Shape* shape);
    nanojit::LIns* insImmId(jsid id);
    nanojit::LIns* p2i(nanojit::LIns* ins);

    /*
     * Examines current interpreter state to record information suitable for returning to the
     * interpreter through a side exit of the given type.
     */
    JS_REQUIRES_STACK VMSideExit* snapshot(ExitType exitType);

    /*
     * Creates a separate but identical copy of the given side exit, allowing the guards associated
     * with each to be entirely separate even after subsequent patching.
     */
    JS_REQUIRES_STACK VMSideExit* copy(VMSideExit* exit);

    /*
     * Creates an instruction whose payload is a GuardRecord for the given exit.  The instruction
     * is suitable for use as the final argument of a single call to LirBuffer::insGuard; do not
     * reuse the returned value.
     */
    JS_REQUIRES_STACK nanojit::GuardRecord* createGuardRecord(VMSideExit* exit);

    JS_REQUIRES_STACK JS_INLINE void markSlotUndemotable(LinkableFragment* f, unsigned slot);

    JS_REQUIRES_STACK JS_INLINE void markSlotUndemotable(LinkableFragment* f, unsigned slot, const void* pc);

    JS_REQUIRES_STACK unsigned findUndemotesInTypemaps(const TypeMap& typeMap, LinkableFragment* f,
                            Queue<unsigned>& undemotes);

    JS_REQUIRES_STACK void assertDownFrameIsConsistent(VMSideExit* anchor, FrameInfo* fi);

    JS_REQUIRES_STACK void captureStackTypes(unsigned callDepth, JSValueType* typeMap);

    bool isVoidPtrGlobal(const void* p) const;
    bool isGlobal(const Value* p) const;
    ptrdiff_t nativeGlobalSlot(const Value *p) const;
    ptrdiff_t nativeGlobalOffset(const Value* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativeStackOffsetImpl(const void* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativeStackOffset(const Value* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativeStackSlotImpl(const void* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativeStackSlot(const Value* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativespOffsetImpl(const void* p) const;
    JS_REQUIRES_STACK ptrdiff_t nativespOffset(const Value* p) const;
    JS_REQUIRES_STACK void importImpl(nanojit::LIns* base, ptrdiff_t offset, nanojit::AccSet accSet,
                                      const void* p, JSValueType t,
                                      const char *prefix, uintN index, JSStackFrame *fp);
    JS_REQUIRES_STACK void import(nanojit::LIns* base, ptrdiff_t offset, nanojit::AccSet accSet,
                                  const Value* p, JSValueType t,
                                  const char *prefix, uintN index, JSStackFrame *fp);
    JS_REQUIRES_STACK void import(TreeFragment* tree, nanojit::LIns* sp, unsigned stackSlots,
                                  unsigned callDepth, unsigned ngslots, JSValueType* typeMap);
    void trackNativeStackUse(unsigned slots);

    JS_REQUIRES_STACK bool isValidSlot(JSObject *obj, const js::Shape* shape);
    JS_REQUIRES_STACK bool lazilyImportGlobalSlot(unsigned slot);
    JS_REQUIRES_STACK void importGlobalSlot(unsigned slot);

    JS_REQUIRES_STACK RecordingStatus guard(bool expected, nanojit::LIns* cond, ExitType exitType,
                                            bool abortIfAlwaysExits = false);
    JS_REQUIRES_STACK RecordingStatus guard(bool expected, nanojit::LIns* cond, VMSideExit* exit,
                                            bool abortIfAlwaysExits = false);
    JS_REQUIRES_STACK nanojit::LIns* guard_xov(nanojit::LOpcode op, nanojit::LIns* d0,
                                               nanojit::LIns* d1, VMSideExit* exit);

    nanojit::LIns* addName(nanojit::LIns* ins, const char* name);

    nanojit::LIns* writeBack(nanojit::LIns* i, nanojit::LIns* base, ptrdiff_t offset,
                             bool demote);

#ifdef DEBUG
    bool isValidFrameObjPtr(void *obj);
#endif

    JS_REQUIRES_STACK void setImpl(void* p, nanojit::LIns* l, bool demote = true);
    JS_REQUIRES_STACK void set(Value* p, nanojit::LIns* l, bool demote = true);
    JS_REQUIRES_STACK void setFrameObjPtr(void* p, nanojit::LIns* l, bool demote = true);
    nanojit::LIns* getFromTrackerImpl(const void *p);
    nanojit::LIns* getFromTracker(const Value* p);
    JS_REQUIRES_STACK nanojit::LIns* getImpl(const void* p);
    JS_REQUIRES_STACK nanojit::LIns* get(const Value* p);
    JS_REQUIRES_STACK nanojit::LIns* getFrameObjPtr(void* p);
    JS_REQUIRES_STACK nanojit::LIns* attemptImport(const Value* p);
    JS_REQUIRES_STACK nanojit::LIns* addr(Value* p);

    JS_REQUIRES_STACK bool knownImpl(const void* p);
    JS_REQUIRES_STACK bool known(const Value* p);
    JS_REQUIRES_STACK bool known(JSObject** p);
    /*
     * The slots of the global object are sometimes reallocated by the
     * interpreter.  This function checks for that condition and re-maps the
     * entries of the tracker accordingly.
     */
    JS_REQUIRES_STACK void checkForGlobalObjectReallocation() {
        if (global_slots != globalObj->getSlots())
            checkForGlobalObjectReallocationHelper();
    }
    JS_REQUIRES_STACK void checkForGlobalObjectReallocationHelper();

    JS_REQUIRES_STACK TypeConsensus selfTypeStability(SlotMap& smap);
    JS_REQUIRES_STACK TypeConsensus peerTypeStability(SlotMap& smap, const void* ip,
                                                      TreeFragment** peer);

    JS_REQUIRES_STACK Value& argval(unsigned n) const;
    JS_REQUIRES_STACK Value& varval(unsigned n) const;
    JS_REQUIRES_STACK Value& stackval(int n) const;

    JS_REQUIRES_STACK void updateAtoms();
    JS_REQUIRES_STACK void updateAtoms(JSScript *script);

    struct NameResult {
        // |tracked| is true iff the result of the name lookup is a variable that
        // is already in the tracker. The rest of the fields are set only if
        // |tracked| is false.
        bool             tracked;
        Value            v;              // current property value
        JSObject         *obj;           // Call object where name was found
        nanojit::LIns    *obj_ins;       // LIR value for obj
        js::Shape        *shape;         // shape name was resolved to
    };

    JS_REQUIRES_STACK nanojit::LIns* scopeChain();
    JS_REQUIRES_STACK nanojit::LIns* entryScopeChain() const;
    JS_REQUIRES_STACK nanojit::LIns* entryFrameIns() const;
    JS_REQUIRES_STACK JSStackFrame* frameIfInRange(JSObject* obj, unsigned* depthp = NULL) const;
    JS_REQUIRES_STACK RecordingStatus traverseScopeChain(JSObject *obj, nanojit::LIns *obj_ins, JSObject *obj2, nanojit::LIns *&obj2_ins);
    JS_REQUIRES_STACK AbortableRecordingStatus scopeChainProp(JSObject* obj, Value*& vp, nanojit::LIns*& ins, NameResult& nr);
    JS_REQUIRES_STACK RecordingStatus callProp(JSObject* obj, JSProperty* shape, jsid id, Value*& vp, nanojit::LIns*& ins, NameResult& nr);

    JS_REQUIRES_STACK nanojit::LIns* arg(unsigned n);
    JS_REQUIRES_STACK void arg(unsigned n, nanojit::LIns* i);
    JS_REQUIRES_STACK nanojit::LIns* var(unsigned n);
    JS_REQUIRES_STACK void var(unsigned n, nanojit::LIns* i);
    JS_REQUIRES_STACK nanojit::LIns* upvar(JSScript* script, JSUpvarArray* uva, uintN index, Value& v);
    nanojit::LIns* stackLoad(nanojit::LIns* addr, nanojit::AccSet accSet, uint8 type);
    JS_REQUIRES_STACK nanojit::LIns* stack(int n);
    JS_REQUIRES_STACK void stack(int n, nanojit::LIns* i);

    JS_REQUIRES_STACK void guardNonNeg(nanojit::LIns* d0, nanojit::LIns* d1, VMSideExit* exit);
    JS_REQUIRES_STACK nanojit::LIns* alu(nanojit::LOpcode op, jsdouble v0, jsdouble v1,
                                         nanojit::LIns* s0, nanojit::LIns* s1);

    bool condBranch(nanojit::LOpcode op, nanojit::LIns* cond, nanojit::LIns** brOut);
    nanojit::LIns* unoptimizableCondBranch(nanojit::LOpcode op, nanojit::LIns* cond);
    void labelForBranch(nanojit::LIns* br);
    void labelForBranches(nanojit::LIns* br1, nanojit::LIns* br2);

    nanojit::LIns* i2d(nanojit::LIns* i);
    nanojit::LIns* d2i(nanojit::LIns* f, bool resultCanBeImpreciseIfFractional = false);
    nanojit::LIns* f2u(nanojit::LIns* f);
    JS_REQUIRES_STACK RecordingStatus makeNumberInt32(nanojit::LIns* d, nanojit::LIns** num_ins);
    JS_REQUIRES_STACK nanojit::LIns* stringify(const Value& v);

    JS_REQUIRES_STACK nanojit::LIns* newArguments(nanojit::LIns* callee_ins, bool strict);

    JS_REQUIRES_STACK bool canCallImacro() const;
    JS_REQUIRES_STACK RecordingStatus callImacro(jsbytecode* imacro);
    JS_REQUIRES_STACK RecordingStatus callImacroInfallibly(jsbytecode* imacro);

    JS_REQUIRES_STACK AbortableRecordingStatus ifop();
    JS_REQUIRES_STACK RecordingStatus switchop();
#ifdef NANOJIT_IA32
    JS_REQUIRES_STACK AbortableRecordingStatus tableswitch();
#endif
    JS_REQUIRES_STACK RecordingStatus inc(Value& v, jsint incr, bool pre = true);
    JS_REQUIRES_STACK RecordingStatus inc(const Value &v, nanojit::LIns*& v_ins, jsint incr,
                                            bool pre = true);
    JS_REQUIRES_STACK RecordingStatus incHelper(const Value &v, nanojit::LIns* v_ins,
                                                  nanojit::LIns*& v_after, jsint incr);
    JS_REQUIRES_STACK AbortableRecordingStatus incProp(jsint incr, bool pre = true);
    JS_REQUIRES_STACK RecordingStatus incElem(jsint incr, bool pre = true);
    JS_REQUIRES_STACK AbortableRecordingStatus incName(jsint incr, bool pre = true);

    JS_REQUIRES_STACK void strictEquality(bool equal, bool cmpCase);
    JS_REQUIRES_STACK AbortableRecordingStatus equality(bool negate, bool tryBranchAfterCond);
    JS_REQUIRES_STACK AbortableRecordingStatus equalityHelper(Value& l, Value& r,
                                                                nanojit::LIns* l_ins, nanojit::LIns* r_ins,
                                                                bool negate, bool tryBranchAfterCond,
                                                                Value& rval);
    JS_REQUIRES_STACK AbortableRecordingStatus relational(nanojit::LOpcode op, bool tryBranchAfterCond);

    JS_REQUIRES_STACK RecordingStatus unary(nanojit::LOpcode op);
    JS_REQUIRES_STACK RecordingStatus binary(nanojit::LOpcode op);

    JS_REQUIRES_STACK RecordingStatus guardShape(nanojit::LIns* obj_ins, JSObject* obj,
                                                 uint32 shape, const char* name, VMSideExit* exit);

#if defined DEBUG_notme && defined XP_UNIX
    void dumpGuardedShapes(const char* prefix);
#endif

    void forgetGuardedShapes();

    inline nanojit::LIns* shape_ins(nanojit::LIns *obj_ins);
    inline nanojit::LIns* slots(nanojit::LIns *obj_ins);
    JS_REQUIRES_STACK AbortableRecordingStatus test_property_cache(JSObject* obj, nanojit::LIns* obj_ins,
                                                                     JSObject*& obj2, PCVal& pcval);
    JS_REQUIRES_STACK RecordingStatus guardPropertyCacheHit(nanojit::LIns* obj_ins,
                                                            JSObject* aobj,
                                                            JSObject* obj2,
                                                            PropertyCacheEntry* entry,
                                                            PCVal& pcval);

    void stobj_set_fslot(nanojit::LIns *obj_ins, unsigned slot, const Value &v,
                         nanojit::LIns* v_ins);
    void stobj_set_dslot(nanojit::LIns *obj_ins, unsigned slot,
                         nanojit::LIns*& slots_ins, const Value &v, nanojit::LIns* v_ins);
    void stobj_set_slot(JSObject *obj, nanojit::LIns* obj_ins, unsigned slot,
                        nanojit::LIns*& slots_ins, const Value &v, nanojit::LIns* v_ins);

    nanojit::LIns* stobj_get_slot_uint32(nanojit::LIns* obj_ins, unsigned slot);
    nanojit::LIns* unbox_slot(JSObject *obj, nanojit::LIns *obj_ins, uint32 slot,
                              VMSideExit *exit);
    nanojit::LIns* stobj_get_parent(nanojit::LIns* obj_ins);
    nanojit::LIns* stobj_get_private(nanojit::LIns* obj_ins);
    nanojit::LIns* stobj_get_private_uint32(nanojit::LIns* obj_ins);
    nanojit::LIns* stobj_get_proto(nanojit::LIns* obj_ins);

    /* For slots holding private pointers. */
    nanojit::LIns* stobj_get_const_private_ptr(nanojit::LIns *obj_ins, unsigned slot);

    JS_REQUIRES_STACK AbortableRecordingStatus name(Value*& vp, nanojit::LIns*& ins, NameResult& nr);
    JS_REQUIRES_STACK AbortableRecordingStatus prop(JSObject* obj, nanojit::LIns* obj_ins,
                                                    uint32 *slotp, nanojit::LIns** v_insp,
                                                    Value* outp);
    JS_REQUIRES_STACK RecordingStatus propTail(JSObject* obj, nanojit::LIns* obj_ins,
                                               JSObject* obj2, PCVal pcval,
                                               uint32 *slotp, nanojit::LIns** v_insp,
                                               Value* outp);
    JS_REQUIRES_STACK RecordingStatus denseArrayElement(Value& oval, Value& idx, Value*& vp,
                                                        nanojit::LIns*& v_ins,
                                                        nanojit::LIns*& addr_ins,
                                                        VMSideExit* exit);
    JS_REQUIRES_STACK nanojit::LIns *canonicalizeNaNs(nanojit::LIns *dval_ins);
    JS_REQUIRES_STACK AbortableRecordingStatus typedArrayElement(Value& oval, Value& idx, Value*& vp,
                                                                 nanojit::LIns*& v_ins,
                                                                 nanojit::LIns*& addr_ins);
    JS_REQUIRES_STACK AbortableRecordingStatus getProp(JSObject* obj, nanojit::LIns* obj_ins);
    JS_REQUIRES_STACK AbortableRecordingStatus getProp(Value& v);
    JS_REQUIRES_STACK RecordingStatus getThis(nanojit::LIns*& this_ins);

    JS_REQUIRES_STACK void storeMagic(JSWhyMagic why, nanojit::LIns *addr_ins, ptrdiff_t offset,
                                      nanojit::AccSet accSet);
    JS_REQUIRES_STACK AbortableRecordingStatus unboxNextValue(nanojit::LIns* &v_ins);

    JS_REQUIRES_STACK VMSideExit* enterDeepBailCall();
    JS_REQUIRES_STACK void leaveDeepBailCall();

    JS_REQUIRES_STACK RecordingStatus primitiveToStringInPlace(Value* vp);
    JS_REQUIRES_STACK void finishGetProp(nanojit::LIns* obj_ins, nanojit::LIns* vp_ins,
                                         nanojit::LIns* ok_ins, Value* outp);
    JS_REQUIRES_STACK RecordingStatus getPropertyByName(nanojit::LIns* obj_ins, Value* idvalp,
                                                        Value* outp);
    JS_REQUIRES_STACK RecordingStatus getPropertyByIndex(nanojit::LIns* obj_ins,
                                                         nanojit::LIns* index_ins, Value* outp);
    JS_REQUIRES_STACK RecordingStatus getPropertyById(nanojit::LIns* obj_ins, Value* outp);
    JS_REQUIRES_STACK RecordingStatus getPropertyWithNativeGetter(nanojit::LIns* obj_ins,
                                                                  const js::Shape* shape,
                                                                  Value* outp);
    JS_REQUIRES_STACK RecordingStatus getPropertyWithScriptGetter(JSObject *obj,
                                                                  nanojit::LIns* obj_ins,
                                                                  const js::Shape* shape);

    JS_REQUIRES_STACK nanojit::LIns* getStringLengthAndFlags(nanojit::LIns* str_ins);
    JS_REQUIRES_STACK nanojit::LIns* getStringLength(nanojit::LIns* str_ins);
    JS_REQUIRES_STACK nanojit::LIns* getStringChars(nanojit::LIns* str_ins);
    JS_REQUIRES_STACK RecordingStatus getCharCodeAt(JSString *str,
                                                    nanojit::LIns* str_ins, nanojit::LIns* idx_ins,
                                                    nanojit::LIns** out_ins);
    JS_REQUIRES_STACK nanojit::LIns* getUnitString(nanojit::LIns* str_ins, nanojit::LIns* idx_ins);
    JS_REQUIRES_STACK RecordingStatus getCharAt(JSString *str,
                                                nanojit::LIns* str_ins, nanojit::LIns* idx_ins,
                                                JSOp mode, nanojit::LIns** out_ins);

    JS_REQUIRES_STACK RecordingStatus nativeSet(JSObject* obj, nanojit::LIns* obj_ins,
                                                const js::Shape* shape,
                                                const Value &v, nanojit::LIns* v_ins);
    JS_REQUIRES_STACK RecordingStatus setProp(Value &l, PropertyCacheEntry* entry,
                                              const js::Shape* shape,
                                              Value &v, nanojit::LIns*& v_ins,
                                              bool isDefinitelyAtom);
    JS_REQUIRES_STACK RecordingStatus setCallProp(JSObject *callobj, nanojit::LIns *callobj_ins,
                                                  const js::Shape *shape, nanojit::LIns *v_ins,
                                                  const Value &v);
    JS_REQUIRES_STACK RecordingStatus initOrSetPropertyByName(nanojit::LIns* obj_ins,
                                                              Value* idvalp, Value* rvalp,
                                                              bool init);
    JS_REQUIRES_STACK RecordingStatus initOrSetPropertyByIndex(nanojit::LIns* obj_ins,
                                                               nanojit::LIns* index_ins,
                                                               Value* rvalp, bool init);
    JS_REQUIRES_STACK AbortableRecordingStatus setElem(int lval_spindex, int idx_spindex,
                                                       int v_spindex);

    void box_undefined_into(nanojit::LIns *dstaddr_ins, ptrdiff_t offset, nanojit::AccSet accSet);
#if JS_BITS_PER_WORD == 32
    void box_null_into(nanojit::LIns *dstaddr_ins, ptrdiff_t offset, nanojit::AccSet accSet);
    nanojit::LIns* unbox_number_as_double(nanojit::LIns* vaddr_ins, ptrdiff_t offset,
                                          nanojit::LIns* tag_ins, VMSideExit* exit,
                                          nanojit::AccSet accSet);
    nanojit::LIns* unbox_object(nanojit::LIns* vaddr_ins, ptrdiff_t offset,
                                nanojit::LIns* tag_ins, JSValueType type, VMSideExit* exit,
                                nanojit::AccSet accSet);
    nanojit::LIns* unbox_non_double_object(nanojit::LIns* vaddr_ins, ptrdiff_t offset,
                                           nanojit::LIns* tag_ins, JSValueType type,
                                           VMSideExit* exit, nanojit::AccSet accSet);
#elif JS_BITS_PER_WORD == 64
    nanojit::LIns* non_double_object_value_has_type(nanojit::LIns* v_ins, JSValueType type);
    nanojit::LIns* unpack_ptr(nanojit::LIns* v_ins);
    nanojit::LIns* unbox_number_as_double(nanojit::LIns* v_ins, VMSideExit* exit);
    nanojit::LIns* unbox_object(nanojit::LIns* v_ins, JSValueType type, VMSideExit* exit);
    nanojit::LIns* unbox_non_double_object(nanojit::LIns* v_ins, JSValueType type, VMSideExit* exit);
#endif

    nanojit::LIns* unbox_value(const Value& v, nanojit::LIns* vaddr_ins,
                               ptrdiff_t offset, nanojit::AccSet accSet, VMSideExit* exit,
                               bool force_double=false);
    void unbox_any_object(nanojit::LIns* vaddr_ins, nanojit::LIns** obj_ins,
                          nanojit::LIns** is_obj_ins, nanojit::AccSet accSet);
    nanojit::LIns* is_boxed_true(nanojit::LIns* vaddr_ins, nanojit::AccSet accSet);
    nanojit::LIns* is_boxed_magic(nanojit::LIns* vaddr_ins, JSWhyMagic why, nanojit::AccSet accSet);

    nanojit::LIns* is_string_id(nanojit::LIns* id_ins);
    nanojit::LIns* unbox_string_id(nanojit::LIns* id_ins);
    nanojit::LIns* unbox_int_id(nanojit::LIns* id_ins);

    /* Box a slot on trace into the given address at the given offset. */
    void box_value_into(const Value& v, nanojit::LIns* v_ins,
                        nanojit::LIns* dstaddr_ins, ptrdiff_t offset,
                        nanojit::AccSet accSet);

    /*
     * Box a slot so that it may be passed with value semantics to a native. On
     * 32-bit, this currently means boxing the value into insAlloc'd memory and
     * returning the address which is passed as a Value*. On 64-bit, this
     * currently means returning the boxed value which is passed as a jsval.
     */
    nanojit::LIns* box_value_for_native_call(const Value& v, nanojit::LIns* v_ins);

    /* Box a slot into insAlloc'd memory. */
    nanojit::LIns* box_value_into_alloc(const Value& v, nanojit::LIns* v_ins);

    JS_REQUIRES_STACK void guardClassHelper(bool cond, nanojit::LIns* obj_ins, Class* clasp,
                                            VMSideExit* exit, nanojit::LoadQual loadQual);
    JS_REQUIRES_STACK void guardClass(nanojit::LIns* obj_ins, Class* clasp,
                                      VMSideExit* exit, nanojit::LoadQual loadQual);
    JS_REQUIRES_STACK void guardNotClass(nanojit::LIns* obj_ins, Class* clasp,
                                         VMSideExit* exit, nanojit::LoadQual loadQual);
    JS_REQUIRES_STACK void guardDenseArray(nanojit::LIns* obj_ins, ExitType exitType);
    JS_REQUIRES_STACK void guardDenseArray(nanojit::LIns* obj_ins, VMSideExit* exit);
    JS_REQUIRES_STACK bool guardHasPrototype(JSObject* obj, nanojit::LIns* obj_ins,
                                             JSObject** pobj, nanojit::LIns** pobj_ins,
                                             VMSideExit* exit);
    JS_REQUIRES_STACK RecordingStatus guardPrototypeHasNoIndexedProperties(JSObject* obj,
                                                                           nanojit::LIns* obj_ins,
                                                                           VMSideExit *exit);
    JS_REQUIRES_STACK RecordingStatus guardNativeConversion(Value& v);
    JS_REQUIRES_STACK void clearCurrentFrameSlotsFromTracker(Tracker& which);
    JS_REQUIRES_STACK void putActivationObjects();
    JS_REQUIRES_STACK RecordingStatus guardCallee(Value& callee);
    JS_REQUIRES_STACK JSStackFrame      *guardArguments(JSObject *obj, nanojit::LIns* obj_ins,
                                                        unsigned *depthp);
    JS_REQUIRES_STACK nanojit::LIns* guardArgsLengthNotAssigned(nanojit::LIns* argsobj_ins);
    JS_REQUIRES_STACK void guardNotHole(nanojit::LIns *argsobj_ins, nanojit::LIns *ids_ins);
    JS_REQUIRES_STACK RecordingStatus getClassPrototype(JSObject* ctor,
                                                          nanojit::LIns*& proto_ins);
    JS_REQUIRES_STACK RecordingStatus getClassPrototype(JSProtoKey key,
                                                          nanojit::LIns*& proto_ins);
    JS_REQUIRES_STACK RecordingStatus newArray(JSObject* ctor, uint32 argc, Value* argv,
                                                 Value* rval);
    JS_REQUIRES_STACK RecordingStatus newString(JSObject* ctor, uint32 argc, Value* argv,
                                                  Value* rval);
    JS_REQUIRES_STACK RecordingStatus interpretedFunctionCall(Value& fval, JSFunction* fun,
                                                                uintN argc, bool constructing);
    JS_REQUIRES_STACK void propagateFailureToBuiltinStatus(nanojit::LIns *ok_ins,
                                                           nanojit::LIns *&status_ins);
    JS_REQUIRES_STACK RecordingStatus emitNativeCall(JSSpecializedNative* sn, uintN argc,
                                                       nanojit::LIns* args[], bool rooted);
    JS_REQUIRES_STACK void emitNativePropertyOp(const js::Shape* shape,
                                                nanojit::LIns* obj_ins,
                                                bool setflag,
                                                nanojit::LIns* addr_boxed_val_ins);
    JS_REQUIRES_STACK RecordingStatus callSpecializedNative(JSNativeTraceInfo* trcinfo, uintN argc,
                                                              bool constructing);
    JS_REQUIRES_STACK RecordingStatus callNative(uintN argc, JSOp mode);
    JS_REQUIRES_STACK RecordingStatus callFloatReturningInt(uintN argc,
                                                            const nanojit::CallInfo *ci);
    JS_REQUIRES_STACK RecordingStatus functionCall(uintN argc, JSOp mode);

    JS_REQUIRES_STACK void trackCfgMerges(jsbytecode* pc);
    JS_REQUIRES_STACK void emitIf(jsbytecode* pc, bool cond, nanojit::LIns* x);
    JS_REQUIRES_STACK void fuseIf(jsbytecode* pc, bool cond, nanojit::LIns* x);
    JS_REQUIRES_STACK AbortableRecordingStatus checkTraceEnd(jsbytecode* pc);

    AbortableRecordingStatus hasMethod(JSObject* obj, jsid id, bool& found);
    JS_REQUIRES_STACK AbortableRecordingStatus hasIteratorMethod(JSObject* obj, bool& found);

    JS_REQUIRES_STACK jsatomid getFullIndex(ptrdiff_t pcoff = 0);

    JS_REQUIRES_STACK JSValueType determineSlotType(Value* vp);

    JS_REQUIRES_STACK RecordingStatus setUpwardTrackedVar(Value* stackVp, const Value& v,
                                                          nanojit::LIns* v_ins);

    JS_REQUIRES_STACK AbortableRecordingStatus compile();
    JS_REQUIRES_STACK AbortableRecordingStatus closeLoop();
    JS_REQUIRES_STACK AbortableRecordingStatus closeLoop(VMSideExit* exit);
    JS_REQUIRES_STACK AbortableRecordingStatus closeLoop(SlotMap& slotMap, VMSideExit* exit);
    JS_REQUIRES_STACK AbortableRecordingStatus endLoop();
    JS_REQUIRES_STACK AbortableRecordingStatus endLoop(VMSideExit* exit);
    JS_REQUIRES_STACK void joinEdgesToEntry(TreeFragment* peer_root);
    JS_REQUIRES_STACK void adjustCallerTypes(TreeFragment* f);
    JS_REQUIRES_STACK void prepareTreeCall(TreeFragment* inner);
    JS_REQUIRES_STACK void emitTreeCall(TreeFragment* inner, VMSideExit* exit);
    JS_REQUIRES_STACK void determineGlobalTypes(JSValueType* typeMap);
    JS_REQUIRES_STACK VMSideExit* downSnapshot(FrameInfo* downFrame);
    JS_REQUIRES_STACK TreeFragment* findNestedCompatiblePeer(TreeFragment* f);
    JS_REQUIRES_STACK AbortableRecordingStatus attemptTreeCall(TreeFragment* inner,
                                                               uintN& inlineCallCount);

    static JS_REQUIRES_STACK MonitorResult recordLoopEdge(JSContext* cx, TraceRecorder* r,
                                                          uintN& inlineCallCount);

    /* Allocators associated with this recording session. */
    VMAllocator& tempAlloc() const { return *traceMonitor->tempAlloc; }
    VMAllocator& traceAlloc() const { return *traceMonitor->traceAlloc; }
    VMAllocator& dataAlloc() const { return *traceMonitor->dataAlloc; }

    /* Member declarations for each opcode, to be called before interpreting the opcode. */
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    JS_REQUIRES_STACK AbortableRecordingStatus record_##op();
# include "jsopcode.tbl"
#undef OPDEF

    inline void* operator new(size_t size) { return js_calloc(size); }
    inline void operator delete(void *p) { js_free(p); }

    JS_REQUIRES_STACK
    TraceRecorder(JSContext* cx, VMSideExit*, VMFragment*,
                  unsigned stackSlots, unsigned ngslots, JSValueType* typeMap,
                  VMSideExit* expectedInnerExit, jsbytecode* outerTree,
                  uint32 outerArgc, bool speculate);

    /* The destructor should only be called through finish*, not directly. */
    ~TraceRecorder();
    JS_REQUIRES_STACK AbortableRecordingStatus finishSuccessfully();

    enum AbortResult { NORMAL_ABORT, JIT_RESET };
    JS_REQUIRES_STACK AbortResult finishAbort(const char* reason);

    friend class ImportBoxedStackSlotVisitor;
    friend class ImportUnboxedStackSlotVisitor;
    friend class ImportGlobalSlotVisitor;
    friend class AdjustCallerGlobalTypesVisitor;
    friend class AdjustCallerStackTypesVisitor;
    friend class TypeCompatibilityVisitor;
    friend class ImportFrameSlotsVisitor;
    friend class SlotMap;
    friend class DefaultSlotMap;
    friend class DetermineTypesVisitor;
    friend class RecursiveSlotMap;
    friend class UpRecursiveSlotMap;
    friend MonitorResult MonitorLoopEdge(JSContext*, uintN&);
    friend TracePointAction MonitorTracePoint(JSContext*, uintN &inlineCallCount,
                                              bool &blacklist);
    friend AbortResult AbortRecording(JSContext*, const char*);
    friend class BoxArg;
    friend void TraceMonitor::sweep();

  public:
    static bool JS_REQUIRES_STACK
    startRecorder(JSContext*, VMSideExit*, VMFragment*,
                  unsigned stackSlots, unsigned ngslots, JSValueType* typeMap,
                  VMSideExit* expectedInnerExit, jsbytecode* outerTree,
                  uint32 outerArgc, bool speculate);

    /* Accessors. */
    VMFragment*         getFragment() const { return fragment; }
    TreeFragment*       getTree() const { return tree; }
    bool                outOfMemory() const { return traceMonitor->outOfMemory(); }
    Oracle*             getOracle() const { return oracle; }

    /* Entry points / callbacks from the interpreter. */
    JS_REQUIRES_STACK AbortableRecordingStatus monitorRecording(JSOp op);
    JS_REQUIRES_STACK AbortableRecordingStatus record_EnterFrame();
    JS_REQUIRES_STACK AbortableRecordingStatus record_LeaveFrame();
    JS_REQUIRES_STACK AbortableRecordingStatus record_SetPropHit(PropertyCacheEntry* entry,
                                                                 const js::Shape* shape);
    JS_REQUIRES_STACK AbortableRecordingStatus record_DefLocalFunSetSlot(uint32 slot,
                                                                         JSObject* obj);
    JS_REQUIRES_STACK AbortableRecordingStatus record_NativeCallComplete();
    void forgetGuardedShapesForObject(JSObject* obj);

#ifdef DEBUG
    /* Debug printing functionality to emit printf() on trace. */
    JS_REQUIRES_STACK void tprint(const char *format, int count, nanojit::LIns *insa[]);
    JS_REQUIRES_STACK void tprint(const char *format);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins1,
                                  nanojit::LIns *ins2);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins1,
                                  nanojit::LIns *ins2, nanojit::LIns *ins3);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins1,
                                  nanojit::LIns *ins2, nanojit::LIns *ins3,
                                  nanojit::LIns *ins4);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins1,
                                  nanojit::LIns *ins2, nanojit::LIns *ins3,
                                  nanojit::LIns *ins4, nanojit::LIns *ins5);
    JS_REQUIRES_STACK void tprint(const char *format, nanojit::LIns *ins1,
                                  nanojit::LIns *ins2, nanojit::LIns *ins3,
                                  nanojit::LIns *ins4, nanojit::LIns *ins5,
                                  nanojit::LIns *ins6);
#endif
};

#define TRACING_ENABLED(cx)       ((cx)->traceJitEnabled)
#define REGEX_JIT_ENABLED(cx)     ((cx)->traceJitEnabled || (cx)->methodJitEnabled)
#define TRACE_RECORDER(cx)        (JS_TRACE_MONITOR(cx).recorder)
#define SET_TRACE_RECORDER(cx,tr) (JS_TRACE_MONITOR(cx).recorder = (tr))

#define JSOP_IN_RANGE(op,lo,hi)   (uintN((op) - (lo)) <= uintN((hi) - (lo)))
#define JSOP_IS_BINARY(op)        JSOP_IN_RANGE(op, JSOP_BITOR, JSOP_MOD)
#define JSOP_IS_UNARY(op)         JSOP_IN_RANGE(op, JSOP_NEG, JSOP_POS)
#define JSOP_IS_EQUALITY(op)      JSOP_IN_RANGE(op, JSOP_EQ, JSOP_NE)

#define TRACE_ARGS_(x,args)                                                   \
    JS_BEGIN_MACRO                                                            \
        if (TraceRecorder* tr_ = TRACE_RECORDER(cx)) {                        \
            AbortableRecordingStatus status = tr_->record_##x args;           \
            if (StatusAbortsRecorderIfActive(status)) {                       \
                if (TRACE_RECORDER(cx)) {                                     \
                    JS_ASSERT(TRACE_RECORDER(cx) == tr_);                     \
                    AbortRecording(cx, #x);                                   \
                }                                                             \
                if (status == ARECORD_ERROR)                                  \
                    goto error;                                               \
            }                                                                 \
            JS_ASSERT(status != ARECORD_IMACRO);                              \
        }                                                                     \
    JS_END_MACRO

#define TRACE_ARGS(x,args)      TRACE_ARGS_(x, args)
#define TRACE_0(x)              TRACE_ARGS(x, ())
#define TRACE_1(x,a)            TRACE_ARGS(x, (a))
#define TRACE_2(x,a,b)          TRACE_ARGS(x, (a, b))

extern JS_REQUIRES_STACK MonitorResult
MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount);

extern JS_REQUIRES_STACK TracePointAction
MonitorTracePoint(JSContext*, uintN& inlineCallCount, bool& blacklist);

extern JS_REQUIRES_STACK TraceRecorder::AbortResult
AbortRecording(JSContext* cx, const char* reason);

extern void
InitJIT(TraceMonitor *tm);

extern void
FinishJIT(TraceMonitor *tm);

extern void
PurgeScriptFragments(JSContext* cx, JSScript* script);

extern bool
OverfullJITCache(TraceMonitor* tm);

extern void
FlushJITCache(JSContext* cx);

extern JSObject *
GetBuiltinFunction(JSContext *cx, uintN index);

extern void
SetMaxCodeCacheBytes(JSContext* cx, uint32 bytes);

extern void
ExternNativeToValue(JSContext* cx, Value& v, JSValueType type, double* slot);

#ifdef MOZ_TRACEVIS

extern JS_FRIEND_API(bool)
StartTraceVis(const char* filename);

extern JS_FRIEND_API(JSBool)
StartTraceVisNative(JSContext *cx, uintN argc, jsval *vp);

extern JS_FRIEND_API(bool)
StopTraceVis();

extern JS_FRIEND_API(JSBool)
StopTraceVisNative(JSContext *cx, uintN argc, jsval *vp);

/* Must contain no more than 16 items. */
enum TraceVisState {
    // Special: means we returned from current activity to last
    S_EXITLAST,
    // Activities
    S_INTERP,
    S_MONITOR,
    S_RECORD,
    S_COMPILE,
    S_EXECUTE,
    S_NATIVE,
    // Events: these all have (bit 3) == 1.
    S_RESET = 8
};

/* Reason for an exit to the interpreter. */
enum TraceVisExitReason {
    R_NONE,
    R_ABORT,
    /* Reasons in MonitorLoopEdge */
    R_INNER_SIDE_EXIT,
    R_DOUBLES,
    R_CALLBACK_PENDING,
    R_OOM_GETANCHOR,
    R_BACKED_OFF,
    R_COLD,
    R_FAIL_RECORD_TREE,
    R_MAX_PEERS,
    R_FAIL_EXECUTE_TREE,
    R_FAIL_STABILIZE,
    R_FAIL_EXTEND_FLUSH,
    R_FAIL_EXTEND_MAX_BRANCHES,
    R_FAIL_EXTEND_START,
    R_FAIL_EXTEND_COLD,
    R_FAIL_SCOPE_CHAIN_CHECK,
    R_NO_EXTEND_OUTER,
    R_MISMATCH_EXIT,
    R_OOM_EXIT,
    R_TIMEOUT_EXIT,
    R_DEEP_BAIL_EXIT,
    R_STATUS_EXIT,
    R_OTHER_EXIT
};

enum TraceVisFlushReason {
    FR_DEEP_BAIL,
    FR_OOM,
    FR_GLOBAL_SHAPE_MISMATCH,
    FR_GLOBALS_FULL
};

const unsigned long long MS64_MASK = 0xfull << 60;
const unsigned long long MR64_MASK = 0x1full << 55;
const unsigned long long MT64_MASK = ~(MS64_MASK | MR64_MASK);

extern FILE* traceVisLogFile;
extern JSHashTable *traceVisScriptTable;

extern JS_FRIEND_API(void)
StoreTraceVisState(JSContext *cx, TraceVisState s, TraceVisExitReason r);

static inline void
LogTraceVisState(JSContext *cx, TraceVisState s, TraceVisExitReason r)
{
    if (traceVisLogFile) {
        unsigned long long sllu = s;
        unsigned long long rllu = r;
        unsigned long long d = (sllu << 60) | (rllu << 55) | (rdtsc() & MT64_MASK);
        fwrite(&d, sizeof(d), 1, traceVisLogFile);
    }
    if (traceVisScriptTable) {
        StoreTraceVisState(cx, s, r);
    }
}

/*
 * Although this runs the same code as LogTraceVisState, it is a separate
 * function because the meaning of the log entry is different. Also, the entry
 * formats may diverge someday.
 */
static inline void
LogTraceVisEvent(JSContext *cx, TraceVisState s, TraceVisFlushReason r)
{
    LogTraceVisState(cx, s, (TraceVisExitReason) r);
}

static inline void
EnterTraceVisState(JSContext *cx, TraceVisState s, TraceVisExitReason r)
{
    LogTraceVisState(cx, s, r);
}

static inline void
ExitTraceVisState(JSContext *cx, TraceVisExitReason r)
{
    LogTraceVisState(cx, S_EXITLAST, r);
}

struct TraceVisStateObj {
    TraceVisExitReason r;
    JSContext *mCx;

    inline TraceVisStateObj(JSContext *cx, TraceVisState s) : r(R_NONE)
    {
        EnterTraceVisState(cx, s, R_NONE);
        mCx = cx;
    }
    inline ~TraceVisStateObj()
    {
        ExitTraceVisState(mCx, r);
    }
};

#endif /* MOZ_TRACEVIS */

}      /* namespace js */

#else  /* !JS_TRACER */

#define TRACE_0(x)              ((void)0)
#define TRACE_1(x,a)            ((void)0)
#define TRACE_2(x,a,b)          ((void)0)

#endif /* !JS_TRACER */

#endif /* jstracer_h___ */
