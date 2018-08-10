/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SourceNotes_h
#define frontend_SourceNotes_h

#include <stdint.h>

#include "jstypes.h"

typedef uint8_t jssrcnote;

namespace js {

/*
 * Source notes generated along with bytecode for decompiling and debugging.
 * A source note is a uint8_t with 5 bits of type and 3 of offset from the pc
 * of the previous note. If 3 bits of offset aren't enough, extended delta
 * notes (SRC_XDELTA) consisting of 2 set high order bits followed by 6 offset
 * bits are emitted before the next note. Some notes have operand offsets
 * encoded immediately after them, in note bytes or byte-triples.
 *
 *                 Source Note               Extended Delta
 *              +7-6-5-4-3+2-1-0+           +7-6-5+4-3-2-1-0+
 *              |note-type|delta|           |1 1| ext-delta |
 *              +---------+-----+           +---+-----------+
 *
 * At most one "gettable" note (i.e., a note of type other than SRC_NEWLINE,
 * SRC_COLSPAN, SRC_SETLINE, and SRC_XDELTA) applies to a given bytecode.
 *
 * NB: the js_SrcNoteSpec array in BytecodeEmitter.cpp is indexed by this
 * enum, so its initializers need to match the order here.
 */

class SrcNote {
  public:
    // SRC_FOR: Source note for JSOP_NOP at the top of C-style for loop,
    //          which is placed after init expression/declaration ops.
    class For {
      public:
        enum Fields {
            // The offset of the condition expression ops from JSOP_NOP.
            CondOffset,

            // The offset of the update expression ops from JSOP_NOP.
            UpdateOffset,

            // The offset of JSOP_GOTO/JSOP_IFNE at the end of the loop from
            // JSOP_NOP.
            BackJumpOffset,
            Count,
        };
    };
    // SRC_WHILE: Source note for JSOP_GOTO at the top of while loop,
    //            which jumps to JSOP_LOOPENTRY.
    class While {
      public:
        enum Fields {
            // The offset of JSOP_IFNE at the end of the loop from JSOP_GOTO.
            BackJumpOffset,
            Count,
        };
    };
    // SRC_WHILE for do-while: Source note for JSOP_NOP at the top of do-while
    //                         loop
    // FIXME: Add SRC_DO_WHILE (bug 1477896).
    class DoWhile1 {
      public:
        enum Fields {
            // The offset of the condition ops from JSOP_NOP.
            CondOffset,
            Count,
        };
    };
    // SRC_WHILE for do-while: Source note for JSOP_LOOPHEAD at the top of
    //                         do-while loop
    // FIXME: Add SRC_DO_WHILE (bug 1477896).
    class DoWhile2 {
      public:
        enum Fields {
            // The offset of JSOP_IFNE at the end of the loop from
            // JSOP_LOOPHEAD.
            BackJumpOffset,
            Count,
        };
    };
    // SRC_FOR_IN: Source note for JSOP_GOTO at the top of for-in loop,
    //             which jumps to JSOP_LOOPENTRY.
    class ForIn {
      public:
        enum Fields {
            // The offset of JSOP_IFEQ at the end of the loop from JSOP_GOTO.
            BackJumpOffset,
            Count,
        };
    };
    // SRC_FOR_OF: Source note for JSOP_GOTO at the top of for-of loop,
    //             which jumps to JSOP_LOOPENTRY.
    class ForOf {
      public:
        enum Fields {
            // The offset of JSOP_IFEQ at the end of the loop from JSOP_GOTO.
            BackJumpOffset,
            Count,
        };
    };
    // SRC_TABLESWITCH: Source note for JSOP_TABLESWITCH.
    class TableSwitch {
      public:
        enum Fields {
            // The offset of the end of switch (the first non-JumpTarget op
            // after switch) from JSOP_TABLESWITCH.
            EndOffset,
            Count
        };
    };
    // SRC_CONDSWITCH: Source note for JSOP_CONDSWITCH.
    class CondSwitch {
      public:
        enum Fields {
            // The offset of the end of switch (the first non-JumpTarget op
            // after switch) from JSOP_CONDSWITCH.
            EndOffset,

            // The offset of JSOP_CASE for the first case from JSOP_CONDSWITCH.
            FirstCaseOffset,
            Count
        };
    };
    // SRC_NEXTCASE: Source note for JSOP_CASE in a JSOP_CONDSWITCH.
    class NextCase {
      public:
        enum Fields {
            // Offset of the next JSOP_CASE from this JSOP_CASE.  This field is
            // 0 if this is the last JSOP_CASE.
            NextCaseOffset,
            Count
        };
    };
    // SRC_TRY: Source note for JSOP_TRY.
    class Try {
      public:
        enum Fields {
            // The offset of the JSOP_GOTO at the end of the try block from
            // JSOP_TRY.
            EndOfTryJumpOffset,
            Count
        };
    };
    // SRC_COLSPAN: Source note for arbitrary ops.
    class ColSpan {
      public:
        enum Fields {
            // The column span (the diff between the column corresponds to the
            // current op and last known column).
            Span,
            Count
        };
    };
    // SRC_SETLINE: Source note for arbitrary ops.
    class SetLine {
      public:
        enum Fields {
            // The file-absolute source line number of the current op.
            Line,
            Count
        };
    };
};

// FIXME: Add SRC_DO_WHILE (bug 1477896).
static_assert(unsigned(SrcNote::While::Count) == unsigned(SrcNote::DoWhile1::Count),
              "SRC_WHILE can be shared between while and do-while");
static_assert(unsigned(SrcNote::While::Count) == unsigned(SrcNote::DoWhile2::Count),
              "SRC_WHILE can be shared between while and do-while");

#define FOR_EACH_SRC_NOTE_TYPE(M)                                                                  \
    M(SRC_NULL,         "null",        0)  /* Terminates a note vector. */                         \
    M(SRC_IF,           "if",          0)  /* JSOP_IFEQ bytecode is from an if-then. */            \
    M(SRC_IF_ELSE,      "if-else",     0)  /* JSOP_IFEQ bytecode is from an if-then-else. */       \
    M(SRC_COND,         "cond",        0)  /* JSOP_IFEQ is from conditional ?: operator. */        \
    M(SRC_FOR,          "for",         SrcNote::For::Count) \
    M(SRC_WHILE,        "while",       SrcNote::While::Count) \
    M(SRC_FOR_IN,       "for-in",      SrcNote::ForIn::Count) \
    M(SRC_FOR_OF,       "for-of",      SrcNote::ForOf::Count) \
    M(SRC_CONTINUE,     "continue",    0)  /* JSOP_GOTO is a continue. */                          \
    M(SRC_BREAK,        "break",       0)  /* JSOP_GOTO is a break. */                             \
    M(SRC_BREAK2LABEL,  "break2label", 0)  /* JSOP_GOTO for 'break label'. */                      \
    M(SRC_SWITCHBREAK,  "switchbreak", 0)  /* JSOP_GOTO is a break in a switch. */                 \
    M(SRC_TABLESWITCH,  "tableswitch", SrcNote::TableSwitch::Count) \
    M(SRC_CONDSWITCH,   "condswitch",  SrcNote::CondSwitch::Count) \
    M(SRC_NEXTCASE,     "nextcase",    SrcNote::NextCase::Count) \
    M(SRC_ASSIGNOP,     "assignop",    0)  /* += or another assign-op follows. */                  \
    M(SRC_CLASS_SPAN,   "class",       2)  /* The starting and ending offsets for the class, used  \
                                              for toString correctness for default ctors. */       \
    M(SRC_TRY,          "try",         SrcNote::Try::Count) \
    /* All notes above here are "gettable".  See SN_IS_GETTABLE below. */                          \
    M(SRC_COLSPAN,      "colspan",     SrcNote::ColSpan::Count) \
    M(SRC_NEWLINE,      "newline",     0)  /* Bytecode follows a source newline. */                \
    M(SRC_SETLINE,      "setline",     SrcNote::SetLine::Count) \
    M(SRC_UNUSED21,     "unused21",    0)  /* Unused. */                                           \
    M(SRC_UNUSED22,     "unused22",    0)  /* Unused. */                                           \
    M(SRC_UNUSED23,     "unused23",    0)  /* Unused. */                                           \
    M(SRC_XDELTA,       "xdelta",      0)  /* 24-31 are for extended delta notes. */

enum SrcNoteType {
#define DEFINE_SRC_NOTE_TYPE(sym, name, arity) sym,
    FOR_EACH_SRC_NOTE_TYPE(DEFINE_SRC_NOTE_TYPE)
#undef DEFINE_SRC_NOTE_TYPE

    SRC_LAST,
    SRC_LAST_GETTABLE = SRC_TRY
};

static_assert(SRC_XDELTA == 24, "SRC_XDELTA should be 24");

/* A source note array is terminated by an all-zero element. */
inline void
SN_MAKE_TERMINATOR(jssrcnote* sn)
{
    *sn = SRC_NULL;
}

inline bool
SN_IS_TERMINATOR(jssrcnote* sn)
{
    return *sn == SRC_NULL;
}

} // namespace js

#define SN_TYPE_BITS            5
#define SN_DELTA_BITS           3
#define SN_XDELTA_BITS          6
#define SN_TYPE_MASK            (JS_BITMASK(SN_TYPE_BITS) << SN_DELTA_BITS)
#define SN_DELTA_MASK           ((ptrdiff_t)JS_BITMASK(SN_DELTA_BITS))
#define SN_XDELTA_MASK          ((ptrdiff_t)JS_BITMASK(SN_XDELTA_BITS))

#define SN_MAKE_NOTE(sn,t,d)    (*(sn) = (jssrcnote)                          \
                                          (((t) << SN_DELTA_BITS)             \
                                           | ((d) & SN_DELTA_MASK)))
#define SN_MAKE_XDELTA(sn,d)    (*(sn) = (jssrcnote)                          \
                                          ((SRC_XDELTA << SN_DELTA_BITS)      \
                                           | ((d) & SN_XDELTA_MASK)))

#define SN_IS_XDELTA(sn)        ((*(sn) >> SN_DELTA_BITS) >= SRC_XDELTA)
#define SN_TYPE(sn)             ((js::SrcNoteType)(SN_IS_XDELTA(sn)           \
                                                   ? SRC_XDELTA               \
                                                   : *(sn) >> SN_DELTA_BITS))
#define SN_SET_TYPE(sn,type)    SN_MAKE_NOTE(sn, type, SN_DELTA(sn))
#define SN_IS_GETTABLE(sn)      (SN_TYPE(sn) <= SRC_LAST_GETTABLE)

#define SN_DELTA(sn)            ((ptrdiff_t)(SN_IS_XDELTA(sn)                 \
                                             ? *(sn) & SN_XDELTA_MASK         \
                                             : *(sn) & SN_DELTA_MASK))
#define SN_SET_DELTA(sn,delta)  (SN_IS_XDELTA(sn)                             \
                                 ? SN_MAKE_XDELTA(sn, delta)                  \
                                 : SN_MAKE_NOTE(sn, SN_TYPE(sn), delta))

#define SN_DELTA_LIMIT          ((ptrdiff_t)JS_BIT(SN_DELTA_BITS))
#define SN_XDELTA_LIMIT         ((ptrdiff_t)JS_BIT(SN_XDELTA_BITS))

/*
 * Offset fields follow certain notes and are frequency-encoded: an offset in
 * [0,0x7f] consumes one byte, an offset in [0x80,0x7fffffff] takes four, and
 * the high bit of the first byte is set.
 */
#define SN_4BYTE_OFFSET_FLAG    0x80
#define SN_4BYTE_OFFSET_MASK    0x7f

#define SN_OFFSET_BITS          31
#define SN_MAX_OFFSET (((size_t) 1 << SN_OFFSET_BITS) - 1)

inline bool
SN_REPRESENTABLE_OFFSET(ptrdiff_t offset)
{
    return 0 <= offset && size_t(offset) <= SN_MAX_OFFSET;
}

/*
 * SRC_COLSPAN values represent changes to the column number. Colspans are
 * signed: negative changes arise in describing constructs like for(;;) loops,
 * that generate code in non-source order. (Negative colspans also have a
 * history of indicating bugs in updating ParseNodes' source locations.)
 *
 * We store colspans using the same variable-length encoding as offsets,
 * described above. However, unlike offsets, colspans are signed, so we truncate
 * colspans (SN_COLSPAN_TO_OFFSET) for storage as offsets, and sign-extend
 * offsets into colspans when we read them (SN_OFFSET_TO_COLSPAN).
 */
#define SN_COLSPAN_SIGN_BIT (1 << (SN_OFFSET_BITS - 1))
#define SN_MIN_COLSPAN (-SN_COLSPAN_SIGN_BIT)
#define SN_MAX_COLSPAN (SN_COLSPAN_SIGN_BIT - 1)

inline bool
SN_REPRESENTABLE_COLSPAN(ptrdiff_t colspan)
{
    return SN_MIN_COLSPAN <= colspan && colspan <= SN_MAX_COLSPAN;
}

inline ptrdiff_t
SN_OFFSET_TO_COLSPAN(ptrdiff_t offset) {
    // There should be no bits set outside the field we're going to sign-extend.
    MOZ_ASSERT(!(offset & ~((1U << SN_OFFSET_BITS) - 1)));
    // Sign-extend the least significant SN_OFFSET_BITS bits.
    return (offset ^ SN_COLSPAN_SIGN_BIT) - SN_COLSPAN_SIGN_BIT;
}

inline ptrdiff_t
SN_COLSPAN_TO_OFFSET(ptrdiff_t colspan) {
    // Truncate the two's complement colspan, for storage as an offset.
    ptrdiff_t offset = colspan & ((1U << SN_OFFSET_BITS) - 1);
    // When we read this back, we'd better get the value we stored.
    MOZ_ASSERT(SN_OFFSET_TO_COLSPAN(offset) == colspan);
    return offset;
}

#define SN_LENGTH(sn)           ((js_SrcNoteSpec[SN_TYPE(sn)].arity == 0) ? 1 \
                                 : js::SrcNoteLength(sn))
#define SN_NEXT(sn)             ((sn) + SN_LENGTH(sn))

struct JSSrcNoteSpec {
    const char*     name;      /* name for disassembly/debugging output */
    int8_t          arity;      /* number of offset operands */
};

extern JS_FRIEND_DATA(const JSSrcNoteSpec) js_SrcNoteSpec[];

namespace js {

extern JS_FRIEND_API(unsigned)
SrcNoteLength(jssrcnote* sn);

/*
 * Get and set the offset operand identified by which (0 for the first, etc.).
 */
extern JS_FRIEND_API(ptrdiff_t)
GetSrcNoteOffset(jssrcnote* sn, unsigned which);

} // namespace js

#endif /* frontend_SourceNotes_h */
