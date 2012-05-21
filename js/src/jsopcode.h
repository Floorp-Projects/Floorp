/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsopcode_h___
#define jsopcode_h___

/*
 * JS bytecode definitions.
 */
#include <stddef.h>
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsutil.h"

JS_BEGIN_EXTERN_C

/*
 * JS operation bytecodes.
 */
typedef enum JSOp {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    op = val,
#include "jsopcode.tbl"
#undef OPDEF
    JSOP_LIMIT,

    /*
     * These pseudo-ops help js_DecompileValueGenerator decompile JSOP_SETPROP,
     * JSOP_SETELEM, and comprehension-tails, respectively.  They are never
     * stored in bytecode, so they don't preempt valid opcodes.
     */
    JSOP_GETPROP2 = JSOP_LIMIT,
    JSOP_GETELEM2 = JSOP_LIMIT + 1,
    JSOP_FORLOCAL = JSOP_LIMIT + 2,
    JSOP_FAKE_LIMIT = JSOP_FORLOCAL
} JSOp;

/*
 * JS bytecode formats.
 */
#define JOF_BYTE          0       /* single bytecode, no immediates */
#define JOF_JUMP          1       /* signed 16-bit jump offset immediate */
#define JOF_ATOM          2       /* unsigned 16-bit constant index */
#define JOF_UINT16        3       /* unsigned 16-bit immediate operand */
#define JOF_TABLESWITCH   4       /* table switch */
#define JOF_LOOKUPSWITCH  5       /* lookup switch */
#define JOF_QARG          6       /* quickened get/set function argument ops */
#define JOF_LOCAL         7       /* var or block-local variable */
#define JOF_DOUBLE        8       /* uint32_t index for double value */
#define JOF_UINT24        12      /* extended unsigned 24-bit literal (index) */
#define JOF_UINT8         13      /* uint8_t immediate, e.g. top 8 bits of 24-bit
                                     atom index */
#define JOF_INT32         14      /* int32_t immediate operand */
#define JOF_OBJECT        15      /* unsigned 16-bit object index */
#define JOF_SLOTOBJECT    16      /* uint16_t slot index + object index */
#define JOF_REGEXP        17      /* unsigned 32-bit regexp index */
#define JOF_INT8          18      /* int8_t immediate operand */
#define JOF_ATOMOBJECT    19      /* uint16_t constant index + object index */
#define JOF_UINT16PAIR    20      /* pair of uint16_t immediates */
#define JOF_SCOPECOORD    21      /* pair of uint16_t immediates followed by atom index */
#define JOF_TYPEMASK      0x001f  /* mask for above immediate types */

#define JOF_NAME          (1U<<5) /* name operation */
#define JOF_PROP          (2U<<5) /* obj.prop operation */
#define JOF_ELEM          (3U<<5) /* obj[index] operation */
#define JOF_XMLNAME       (4U<<5) /* XML name: *, a::b, @a, @a::b, etc. */
#define JOF_MODEMASK      (7U<<5) /* mask for above addressing modes */
#define JOF_SET           (1U<<8) /* set (i.e., assignment) operation */
#define JOF_DEL           (1U<<9) /* delete operation */
#define JOF_DEC          (1U<<10) /* decrement (--, not ++) opcode */
#define JOF_INC          (2U<<10) /* increment (++, not --) opcode */
#define JOF_INCDEC       (3U<<10) /* increment or decrement opcode */
#define JOF_POST         (1U<<12) /* postorder increment or decrement */
#define JOF_ASSIGNING     JOF_SET /* hint for Class.resolve, used for ops
                                     that do simplex assignment */
#define JOF_DETECTING    (1U<<14) /* object detection for JSNewResolveOp */
#define JOF_BACKPATCH    (1U<<15) /* backpatch placeholder during codegen */
#define JOF_LEFTASSOC    (1U<<16) /* left-associative operator */
#define JOF_DECLARING    (1U<<17) /* var, const, or function declaration op */
/* (1U<<18) is unused */
#define JOF_PARENHEAD    (1U<<20) /* opcode consumes value of expression in
                                     parenthesized statement head */
#define JOF_INVOKE       (1U<<21) /* JSOP_CALL, JSOP_NEW, JSOP_EVAL */
#define JOF_TMPSLOT      (1U<<22) /* interpreter uses extra temporary slot
                                     to root intermediate objects besides
                                     the slots opcode uses */
#define JOF_TMPSLOT2     (2U<<22) /* interpreter uses extra 2 temporary slot
                                     besides the slots opcode uses */
#define JOF_TMPSLOT3     (3U<<22) /* interpreter uses extra 3 temporary slot
                                     besides the slots opcode uses */
#define JOF_TMPSLOT_SHIFT 22
#define JOF_TMPSLOT_MASK  (JS_BITMASK(2) << JOF_TMPSLOT_SHIFT)

/* (1U<<24) is unused */
#define JOF_GNAME        (1U<<25) /* predicted global name */
#define JOF_TYPESET      (1U<<26) /* has an entry in a script's type sets */
#define JOF_DECOMPOSE    (1U<<27) /* followed by an equivalent decomposed
                                   * version of the opcode */
#define JOF_ARITH        (1U<<28) /* unary or binary arithmetic opcode */

/* Shorthands for type from format and type from opcode. */
#define JOF_TYPE(fmt)   ((fmt) & JOF_TYPEMASK)
#define JOF_OPTYPE(op)  JOF_TYPE(js_CodeSpec[op].format)

/* Shorthands for mode from format and mode from opcode. */
#define JOF_MODE(fmt)   ((fmt) & JOF_MODEMASK)
#define JOF_OPMODE(op)  JOF_MODE(js_CodeSpec[op].format)

#define JOF_TYPE_IS_EXTENDED_JUMP(t) \
    ((unsigned)((t) - JOF_JUMP) <= (unsigned)(JOF_LOOKUPSWITCH - JOF_JUMP))

/*
 * Immediate operand getters, setters, and bounds.
 */

static JS_ALWAYS_INLINE uint8_t
GET_UINT8(jsbytecode *pc)
{
    return (uint8_t) pc[1];
}

static JS_ALWAYS_INLINE void
SET_UINT8(jsbytecode *pc, uint8_t u)
{
    pc[1] = (jsbytecode) u;
}

/* Common uint16_t immediate format helpers. */
#define UINT16_LEN              2
#define UINT16_HI(i)            ((jsbytecode)((i) >> 8))
#define UINT16_LO(i)            ((jsbytecode)(i))
#define GET_UINT16(pc)          ((unsigned)(((pc)[1] << 8) | (pc)[2]))
#define SET_UINT16(pc,i)        ((pc)[1] = UINT16_HI(i), (pc)[2] = UINT16_LO(i))
#define UINT16_LIMIT            ((unsigned)1 << 16)

/* Helpers for accessing the offsets of jump opcodes. */
#define JUMP_OFFSET_LEN         4
#define JUMP_OFFSET_MIN         INT32_MIN
#define JUMP_OFFSET_MAX         INT32_MAX

static JS_ALWAYS_INLINE int32_t
GET_JUMP_OFFSET(jsbytecode *pc)
{
    return (pc[1] << 24) | (pc[2] << 16) | (pc[3] << 8) | pc[4];
}

static JS_ALWAYS_INLINE void
SET_JUMP_OFFSET(jsbytecode *pc, int32_t off)
{
    pc[1] = (jsbytecode)(off >> 24);
    pc[2] = (jsbytecode)(off >> 16);
    pc[3] = (jsbytecode)(off >> 8);
    pc[4] = (jsbytecode)off;
}

#define UINT32_INDEX_LEN        4

static JS_ALWAYS_INLINE uint32_t
GET_UINT32_INDEX(const jsbytecode *pc)
{
    return (pc[1] << 24) | (pc[2] << 16) | (pc[3] << 8) | pc[4];
}

static JS_ALWAYS_INLINE void
SET_UINT32_INDEX(jsbytecode *pc, uint32_t index)
{
    pc[1] = (jsbytecode)(index >> 24);
    pc[2] = (jsbytecode)(index >> 16);
    pc[3] = (jsbytecode)(index >> 8);
    pc[4] = (jsbytecode)index;
}

#define UINT24_HI(i)            ((jsbytecode)((i) >> 16))
#define UINT24_MID(i)           ((jsbytecode)((i) >> 8))
#define UINT24_LO(i)            ((jsbytecode)(i))
#define GET_UINT24(pc)          ((jsatomid)(((pc)[1] << 16) |                 \
                                            ((pc)[2] << 8) |                  \
                                            (pc)[3]))
#define SET_UINT24(pc,i)        ((pc)[1] = UINT24_HI(i),                      \
                                 (pc)[2] = UINT24_MID(i),                     \
                                 (pc)[3] = UINT24_LO(i))

#define GET_INT8(pc)            (int8_t((pc)[1]))

#define GET_INT32(pc)           (((uint32_t((pc)[1]) << 24) |                 \
                                  (uint32_t((pc)[2]) << 16) |                 \
                                  (uint32_t((pc)[3]) << 8)  |                 \
                                  uint32_t((pc)[4])))
#define SET_INT32(pc,i)         ((pc)[1] = (jsbytecode)(uint32_t(i) >> 24),   \
                                 (pc)[2] = (jsbytecode)(uint32_t(i) >> 16),   \
                                 (pc)[3] = (jsbytecode)(uint32_t(i) >> 8),    \
                                 (pc)[4] = (jsbytecode)uint32_t(i))

/* Index limit is determined by SN_3BYTE_OFFSET_FLAG, see frontend/BytecodeEmitter.h. */
#define INDEX_LIMIT_LOG2        23
#define INDEX_LIMIT             (uint32_t(1) << INDEX_LIMIT_LOG2)

/* Actual argument count operand format helpers. */
#define ARGC_HI(argc)           UINT16_HI(argc)
#define ARGC_LO(argc)           UINT16_LO(argc)
#define GET_ARGC(pc)            GET_UINT16(pc)
#define ARGC_LIMIT              UINT16_LIMIT

/* Synonyms for quick JOF_QARG and JOF_LOCAL bytecodes. */
#define GET_ARGNO(pc)           GET_UINT16(pc)
#define SET_ARGNO(pc,argno)     SET_UINT16(pc,argno)
#define ARGNO_LEN               2
#define ARGNO_LIMIT             UINT16_LIMIT

#define GET_SLOTNO(pc)          GET_UINT16(pc)
#define SET_SLOTNO(pc,varno)    SET_UINT16(pc,varno)
#define SLOTNO_LEN              2
#define SLOTNO_LIMIT            UINT16_LIMIT

struct JSCodeSpec {
    int8_t              length;         /* length including opcode byte */
    int8_t              nuses;          /* arity, -1 if variadic */
    int8_t              ndefs;          /* number of stack results */
    uint8_t             prec;           /* operator precedence */
    uint32_t            format;         /* immediate operand format */

    uint32_t type() const { return JOF_TYPE(format); }
};

extern const JSCodeSpec js_CodeSpec[];
extern unsigned            js_NumCodeSpecs;
extern const char       *js_CodeName[];
extern const char       js_EscapeMap[];

/* Silence unreferenced formal parameter warnings */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100)
#endif

/*
 * Return a GC'ed string containing the chars in str, with any non-printing
 * chars or quotes (' or " as specified by the quote argument) escaped, and
 * with the quote character at the beginning and end of the result string.
 */
extern JSString *
js_QuoteString(JSContext *cx, JSString *str, jschar quote);

/*
 * JSPrinter operations, for printf style message formatting.  The return
 * value from js_GetPrinterOutput() is the printer's cumulative output, in
 * a GC'ed string.
 *
 * strict is true if the context in which the output will appear has
 * already been marked as strict, thus indicating that nested
 * functions need not be re-marked with a strict directive.  It should
 * be false in the outermost printer.
 */

extern JSPrinter *
js_NewPrinter(JSContext *cx, const char *name, JSFunction *fun,
              unsigned indent, JSBool pretty, JSBool grouped, JSBool strict);

extern void
js_DestroyPrinter(JSPrinter *jp);

extern JSString *
js_GetPrinterOutput(JSPrinter *jp);

extern int
js_printf(JSPrinter *jp, const char *format, ...);

extern JSBool
js_puts(JSPrinter *jp, const char *s);

#define GET_ATOM_FROM_BYTECODE(script, pc, pcoff, atom)                       \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(js_CodeSpec[*(pc)].format & JOF_ATOM);                      \
        (atom) = (script)->getAtom(GET_UINT32_INDEX((pc) + (pcoff)));         \
    JS_END_MACRO

#define GET_NAME_FROM_BYTECODE(script, pc, pcoff, name)                       \
    JS_BEGIN_MACRO                                                            \
        JSAtom *atom_;                                                        \
        GET_ATOM_FROM_BYTECODE(script, pc, pcoff, atom_);                     \
        JS_ASSERT(js_CodeSpec[*(pc)].format & (JOF_NAME | JOF_PROP));         \
        (name) = atom_->asPropertyName();                                     \
    JS_END_MACRO

namespace js {

extern unsigned
StackUses(JSScript *script, jsbytecode *pc);

extern unsigned
StackDefs(JSScript *script, jsbytecode *pc);

}  /* namespace js */

/*
 * Decompilers, for script, function, and expression pretty-printing.
 */
extern JSBool
js_DecompileScript(JSPrinter *jp, JSScript *script);

extern JSBool
js_DecompileFunctionBody(JSPrinter *jp);

extern JSBool
js_DecompileFunction(JSPrinter *jp);

/*
 * Some C++ compilers treat the language linkage (extern "C" vs.
 * extern "C++") as part of function (and thus pointer-to-function)
 * types. The use of this typedef (defined in "C") ensures that
 * js_DecompileToString's definition (in "C++") gets matched up with
 * this declaration.
 */
typedef JSBool (* JSDecompilerPtr)(JSPrinter *);

extern JSString *
js_DecompileToString(JSContext *cx, const char *name, JSFunction *fun,
                     unsigned indent, JSBool pretty, JSBool grouped, JSBool strict,
                     JSDecompilerPtr decompiler);

/*
 * Find the source expression that resulted in v, and return a newly allocated
 * C-string containing it.  Fall back on v's string conversion (fallback) if we
 * can't find the bytecode that generated and pushed v on the operand stack.
 *
 * Search the current stack frame if spindex is JSDVG_SEARCH_STACK.  Don't
 * look for v on the stack if spindex is JSDVG_IGNORE_STACK.  Otherwise,
 * spindex is the negative index of v, measured from cx->fp->sp, or from a
 * lower frame's sp if cx->fp is native.
 *
 * The caller must call JS_free on the result after a succsesful call.
 */
extern char *
js_DecompileValueGenerator(JSContext *cx, int spindex, jsval v,
                           JSString *fallback);

/*
 * Given bytecode address pc in script's main program code, return the operand
 * stack depth just before (JSOp) *pc executes.
 */
extern unsigned
js_ReconstructStackDepth(JSContext *cx, JSScript *script, jsbytecode *pc);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

JS_END_EXTERN_C

#define JSDVG_IGNORE_STACK      0
#define JSDVG_SEARCH_STACK      1

/*
 * Get the length of variable-length bytecode like JSOP_TABLESWITCH.
 */
extern size_t
js_GetVariableBytecodeLength(jsbytecode *pc);

namespace js {

static inline char *
DecompileValueGenerator(JSContext *cx, int spindex, const Value &v,
                        JSString *fallback)
{
    return js_DecompileValueGenerator(cx, spindex, v, fallback);
}

/*
 * Sprintf, but with unlimited and automatically allocated buffering.
 */
class Sprinter
{
  public:
    struct InvariantChecker
    {
        const Sprinter *parent;

        explicit InvariantChecker(const Sprinter *p) : parent(p) {
            parent->checkInvariants();
        }

        ~InvariantChecker() {
            parent->checkInvariants();
        }
    };

    JSContext               *context;       /* context executing the decompiler */

  private:
    static const size_t     DefaultSize;
#ifdef DEBUG
    bool                    initialized;    /* true if this is initialized, use for debug builds */
#endif
    char                    *base;          /* malloc'd buffer address */
    size_t                  size;           /* size of buffer allocated at base */
    ptrdiff_t               offset;         /* offset of next free char in buffer */

    bool realloc_(size_t newSize);

  public:
    explicit Sprinter(JSContext *cx);
    ~Sprinter();

    /* Initialize this sprinter, returns false on error */
    bool init();

    void checkInvariants() const;

    const char *string() const;
    const char *stringEnd() const;
    /* Returns the string at offset |off| */
    char *stringAt(ptrdiff_t off) const;
    /* Returns the char at offset |off| */
    char &operator[](size_t off);
    /* Test if this Sprinter is empty */
    bool empty() const;

    /*
     * Attempt to reserve len + 1 space (for a trailing NULL byte). If the
     * attempt succeeds, return a pointer to the start of that space and adjust the
     * internal content. The caller *must* completely fill this space on success.
     */
    char *reserve(size_t len);
    /* Like reserve, but memory is initialized to 0 */
    char *reserveAndClear(size_t len);

    /*
     * Puts |len| characters from |s| at the current position and return an offset to
     * the beginning of this new data
     */
    ptrdiff_t put(const char *s, size_t len);
    ptrdiff_t put(const char *s);
    ptrdiff_t putString(JSString *str);

    /* Prints a formatted string into the buffer */
    int printf(const char *fmt, ...);

    /* Change the offset */
    void setOffset(const char *end);
    void setOffset(ptrdiff_t off);

    /* Get the offset */
    ptrdiff_t getOffset() const;
    ptrdiff_t getOffsetOf(const char *string) const;
};

extern ptrdiff_t
Sprint(Sprinter *sp, const char *format, ...);

extern bool
CallResultEscapes(jsbytecode *pc);

static inline unsigned
GetDecomposeLength(jsbytecode *pc, size_t len)
{
    /*
     * The last byte of a DECOMPOSE op stores the decomposed length.  This is a
     * constant: perhaps we should just hardcode values instead?
     */
    JS_ASSERT(size_t(js_CodeSpec[*pc].length) == len);
    return (unsigned) pc[len - 1];
}

static inline unsigned
GetBytecodeLength(jsbytecode *pc)
{
    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);

    if (js_CodeSpec[op].length != -1)
        return js_CodeSpec[op].length;
    return js_GetVariableBytecodeLength(pc);
}

extern bool
IsValidBytecodeOffset(JSContext *cx, JSScript *script, size_t offset);

inline bool
FlowsIntoNext(JSOp op)
{
    /* JSOP_YIELD is considered to flow into the next instruction, like JSOP_CALL. */
    return op != JSOP_STOP && op != JSOP_RETURN && op != JSOP_RETRVAL && op != JSOP_THROW &&
           op != JSOP_GOTO && op != JSOP_RETSUB;
}

/*
 * Counts accumulated for a single opcode in a script. The counts tracked vary
 * between opcodes, and this structure ensures that counts are accessed in a
 * coherent fashion.
 */
class PCCounts
{
    friend struct ::JSScript;
    double *counts;
#ifdef DEBUG
    size_t capacity;
#endif

 public:

    enum BaseCounts {
        BASE_INTERP = 0,
        BASE_METHODJIT,

        BASE_METHODJIT_STUBS,
        BASE_METHODJIT_CODE,
        BASE_METHODJIT_PICS,

        BASE_LIMIT
    };

    enum AccessCounts {
        ACCESS_MONOMORPHIC = BASE_LIMIT,
        ACCESS_DIMORPHIC,
        ACCESS_POLYMORPHIC,

        ACCESS_BARRIER,
        ACCESS_NOBARRIER,

        ACCESS_UNDEFINED,
        ACCESS_NULL,
        ACCESS_BOOLEAN,
        ACCESS_INT32,
        ACCESS_DOUBLE,
        ACCESS_STRING,
        ACCESS_OBJECT,

        ACCESS_LIMIT
    };

    static bool accessOp(JSOp op) {
        /*
         * Access ops include all name, element and property reads, as well as
         * SETELEM and SETPROP (for ElementCounts/PropertyCounts alignment).
         */
        if (op == JSOP_SETELEM || op == JSOP_SETPROP)
            return true;
        int format = js_CodeSpec[op].format;
        return !!(format & (JOF_NAME | JOF_GNAME | JOF_ELEM | JOF_PROP))
            && !(format & (JOF_SET | JOF_INCDEC));
    }

    enum ElementCounts {
        ELEM_ID_INT = ACCESS_LIMIT,
        ELEM_ID_DOUBLE,
        ELEM_ID_OTHER,
        ELEM_ID_UNKNOWN,

        ELEM_OBJECT_TYPED,
        ELEM_OBJECT_PACKED,
        ELEM_OBJECT_DENSE,
        ELEM_OBJECT_OTHER,

        ELEM_LIMIT
    };

    static bool elementOp(JSOp op) {
        return accessOp(op) && (JOF_MODE(js_CodeSpec[op].format) == JOF_ELEM);
    }

    enum PropertyCounts {
        PROP_STATIC = ACCESS_LIMIT,
        PROP_DEFINITE,
        PROP_OTHER,

        PROP_LIMIT
    };

    static bool propertyOp(JSOp op) {
        return accessOp(op) && (JOF_MODE(js_CodeSpec[op].format) == JOF_PROP);
    }

    enum ArithCounts {
        ARITH_INT = BASE_LIMIT,
        ARITH_DOUBLE,
        ARITH_OTHER,
        ARITH_UNKNOWN,

        ARITH_LIMIT
    };

    static bool arithOp(JSOp op) {
        return !!(js_CodeSpec[op].format & (JOF_INCDEC | JOF_ARITH));
    }

    static size_t numCounts(JSOp op)
    {
        if (accessOp(op)) {
            if (elementOp(op))
                return ELEM_LIMIT;
            if (propertyOp(op))
                return PROP_LIMIT;
            return ACCESS_LIMIT;
        }
        if (arithOp(op))
            return ARITH_LIMIT;
        return BASE_LIMIT;
    }

    static const char *countName(JSOp op, size_t which);

    double *rawCounts() { return counts; }

    double& get(size_t which) {
        JS_ASSERT(which < capacity);
        return counts[which];
    }

    /* Boolean conversion, for 'if (counters) ...' */
    operator void*() const {
        return counts;
    }
};

} /* namespace js */

#if defined(DEBUG)
/*
 * Disassemblers, for debugging only.
 */
extern JS_FRIEND_API(JSBool)
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, js::Sprinter *sp);

extern JS_FRIEND_API(unsigned)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc, unsigned loc,
                JSBool lines, js::Sprinter *sp);

extern JS_FRIEND_API(void)
js_DumpPCCounts(JSContext *cx, JSScript *script, js::Sprinter *sp);
#endif

#endif /* jsopcode_h___ */
