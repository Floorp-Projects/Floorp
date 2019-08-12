/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RegExpObject.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#ifdef DEBUG
#  include "jsutil.h"
#endif

#include "builtin/RegExp.h"
#include "builtin/SelfHostingDefines.h"  // REGEXP_*_FLAG
#include "frontend/TokenStream.h"
#include "gc/HashUtil.h"
#ifdef DEBUG
#  include "irregexp/RegExpBytecode.h"
#endif
#include "irregexp/RegExpParser.h"
#include "jit/VMFunctions.h"
#include "js/RegExp.h"
#include "js/RegExpFlags.h"  // JS::RegExpFlags
#include "js/StableStringChars.h"
#include "util/StringBuffer.h"
#include "vm/MatchPairs.h"
#include "vm/RegExpStatics.h"
#include "vm/StringType.h"
#include "vm/TraceLogging.h"
#ifdef DEBUG
#  include "util/Unicode.h"
#endif
#include "vm/Xdr.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using namespace js;

using JS::AutoStableStringChars;
using JS::CompileOptions;
using JS::RegExpFlag;
using JS::RegExpFlags;
using js::frontend::TokenStream;
using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::PodCopy;

using JS::AutoCheckCannotGC;

static_assert(RegExpFlag::Global == REGEXP_GLOBAL_FLAG,
              "self-hosted JS and /g flag bits must agree");
static_assert(RegExpFlag::IgnoreCase == REGEXP_IGNORECASE_FLAG,
              "self-hosted JS and /i flag bits must agree");
static_assert(RegExpFlag::Multiline == REGEXP_MULTILINE_FLAG,
              "self-hosted JS and /m flag bits must agree");
static_assert(RegExpFlag::Unicode == REGEXP_UNICODE_FLAG,
              "self-hosted JS and /u flag bits must agree");
static_assert(RegExpFlag::Sticky == REGEXP_STICKY_FLAG,
              "self-hosted JS and /y flag bits must agree");

RegExpObject* js::RegExpAlloc(JSContext* cx, NewObjectKind newKind,
                              HandleObject proto /* = nullptr */) {
  Rooted<RegExpObject*> regexp(
      cx, NewObjectWithClassProto<RegExpObject>(cx, proto, newKind));
  if (!regexp) {
    return nullptr;
  }

  regexp->initPrivate(nullptr);

  if (!EmptyShape::ensureInitialCustomShape<RegExpObject>(cx, regexp)) {
    return nullptr;
  }

  MOZ_ASSERT(regexp->lookupPure(cx->names().lastIndex)->slot() ==
             RegExpObject::lastIndexSlot());

  return regexp;
}

/* MatchPairs */

bool VectorMatchPairs::initArrayFrom(VectorMatchPairs& copyFrom) {
  MOZ_ASSERT(copyFrom.pairCount() > 0);

  if (!allocOrExpandArray(copyFrom.pairCount())) {
    return false;
  }

  PodCopy(pairs_, copyFrom.pairs_, pairCount_);

  return true;
}

bool VectorMatchPairs::allocOrExpandArray(size_t pairCount) {
  if (!vec_.resizeUninitialized(pairCount)) {
    return false;
  }

  pairs_ = &vec_[0];
  pairCount_ = pairCount;
  return true;
}

/* RegExpObject */

/* static */
RegExpShared* RegExpObject::getShared(JSContext* cx,
                                      Handle<RegExpObject*> regexp) {
  if (regexp->hasShared()) {
    return regexp->sharedRef();
  }

  return createShared(cx, regexp);
}

/* static */
bool RegExpObject::isOriginalFlagGetter(JSNative native, RegExpFlags* mask) {
  if (native == regexp_global) {
    *mask = RegExpFlag::Global;
    return true;
  }
  if (native == regexp_ignoreCase) {
    *mask = RegExpFlag::IgnoreCase;
    return true;
  }
  if (native == regexp_multiline) {
    *mask = RegExpFlag::Multiline;
    return true;
  }
  if (native == regexp_sticky) {
    *mask = RegExpFlag::Sticky;
    return true;
  }
  if (native == regexp_unicode) {
    *mask = RegExpFlag::Unicode;
    return true;
  }

  return false;
}

/* static */
void RegExpObject::trace(JSTracer* trc, JSObject* obj) {
  obj->as<RegExpObject>().trace(trc);
}

static inline bool IsMarkingTrace(JSTracer* trc) {
  // Determine whether tracing is happening during normal marking.  We need to
  // test all the following conditions, since:
  //
  //   1. During TraceRuntime, RuntimeHeapIsBusy() is true, but the
  //      tracer might not be a marking tracer.
  //   2. When a write barrier executes, isMarkingTracer is true, but
  //      RuntimeHeapIsBusy() will be false.

  return JS::RuntimeHeapIsCollecting() && trc->isMarkingTracer();
}

void RegExpObject::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &sharedRef(), "RegExpObject shared");
}

static const ClassOps RegExpObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    nullptr, /* finalize */
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    RegExpObject::trace,
};

static const ClassSpec RegExpObjectClassSpec = {
    GenericCreateConstructor<js::regexp_construct, 2, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<RegExpObject>,
    nullptr,
    js::regexp_static_props,
    js::regexp_methods,
    js::regexp_properties};

const Class RegExpObject::class_ = {
    js_RegExp_str,
    JSCLASS_HAS_PRIVATE |
        JSCLASS_HAS_RESERVED_SLOTS(RegExpObject::RESERVED_SLOTS) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp),
    &RegExpObjectClassOps, &RegExpObjectClassSpec};

const Class RegExpObject::protoClass_ = {
    js_Object_str, JSCLASS_HAS_CACHED_PROTO(JSProto_RegExp), JS_NULL_CLASS_OPS,
    &RegExpObjectClassSpec};

template <typename CharT>
RegExpObject* RegExpObject::create(JSContext* cx, const CharT* chars,
                                   size_t length, RegExpFlags flags,
                                   frontend::TokenStreamAnyChars& tokenStream,
                                   NewObjectKind newKind) {
  static_assert(mozilla::IsSame<CharT, char16_t>::value,
                "this code may need updating if/when CharT encodes UTF-8");

  RootedAtom source(cx, AtomizeChars(cx, chars, length));
  if (!source) {
    return nullptr;
  }

  return create(cx, source, flags, tokenStream, newKind);
}

template RegExpObject* RegExpObject::create(
    JSContext* cx, const char16_t* chars, size_t length, RegExpFlags flags,
    frontend::TokenStreamAnyChars& tokenStream, NewObjectKind newKind);

template <typename CharT>
RegExpObject* RegExpObject::create(JSContext* cx, const CharT* chars,
                                   size_t length, RegExpFlags flags,
                                   NewObjectKind newKind) {
  static_assert(mozilla::IsSame<CharT, char16_t>::value,
                "this code may need updating if/when CharT encodes UTF-8");

  RootedAtom source(cx, AtomizeChars(cx, chars, length));
  if (!source) {
    return nullptr;
  }

  return create(cx, source, flags, newKind);
}

template RegExpObject* RegExpObject::create(JSContext* cx,
                                            const char16_t* chars,
                                            size_t length, RegExpFlags flags,
                                            NewObjectKind newKind);

RegExpObject* RegExpObject::create(JSContext* cx, HandleAtom source,
                                   RegExpFlags flags,
                                   frontend::TokenStreamAnyChars& tokenStream,
                                   NewObjectKind newKind) {
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  if (!irregexp::ParsePatternSyntax(tokenStream, allocScope.alloc(), source,
                                    flags.unicode())) {
    return nullptr;
  }

  Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, newKind));
  if (!regexp) {
    return nullptr;
  }

  regexp->initAndZeroLastIndex(source, flags, cx);

  return regexp;
}

RegExpObject* RegExpObject::create(JSContext* cx, HandleAtom source,
                                   RegExpFlags flags, NewObjectKind newKind) {
  CompileOptions dummyOptions(cx);
  TokenStream dummyTokenStream(cx, dummyOptions, (const char16_t*)nullptr, 0,
                               nullptr);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  if (!irregexp::ParsePatternSyntax(dummyTokenStream, allocScope.alloc(),
                                    source, flags.unicode())) {
    return nullptr;
  }

  Rooted<RegExpObject*> regexp(cx, RegExpAlloc(cx, newKind));
  if (!regexp) {
    return nullptr;
  }

  regexp->initAndZeroLastIndex(source, flags, cx);

  return regexp;
}

/* static */
RegExpShared* RegExpObject::createShared(JSContext* cx,
                                         Handle<RegExpObject*> regexp) {
  MOZ_ASSERT(!regexp->hasShared());
  RootedAtom source(cx, regexp->getSource());
  RegExpShared* shared =
      cx->zone()->regExps().get(cx, source, regexp->getFlags());
  if (!shared) {
    return nullptr;
  }

  regexp->setShared(*shared);
  return shared;
}

Shape* RegExpObject::assignInitialShape(JSContext* cx,
                                        Handle<RegExpObject*> self) {
  MOZ_ASSERT(self->empty());

  JS_STATIC_ASSERT(LAST_INDEX_SLOT == 0);

  /* The lastIndex property alone is writable but non-configurable. */
  return NativeObject::addDataProperty(cx, self, cx->names().lastIndex,
                                       LAST_INDEX_SLOT, JSPROP_PERMANENT);
}

void RegExpObject::initIgnoringLastIndex(JSAtom* source, RegExpFlags flags) {
  // If this is a re-initialization with an existing RegExpShared, 'flags'
  // may not match getShared()->flags, so forget the RegExpShared.
  sharedRef() = nullptr;

  setSource(source);
  setFlags(flags);
}

void RegExpObject::initAndZeroLastIndex(JSAtom* source, RegExpFlags flags,
                                        JSContext* cx) {
  initIgnoringLastIndex(source, flags);
  zeroLastIndex(cx);
}

static MOZ_ALWAYS_INLINE bool IsRegExpLineTerminator(const JS::Latin1Char c) {
  return c == '\n' || c == '\r';
}

static MOZ_ALWAYS_INLINE bool IsRegExpLineTerminator(const char16_t c) {
  return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029;
}

static MOZ_ALWAYS_INLINE bool AppendEscapedLineTerminator(
    StringBuffer& sb, const JS::Latin1Char c) {
  switch (c) {
    case '\n':
      if (!sb.append('n')) {
        return false;
      }
      break;
    case '\r':
      if (!sb.append('r')) {
        return false;
      }
      break;
    default:
      MOZ_CRASH("Bad LineTerminator");
  }
  return true;
}

static MOZ_ALWAYS_INLINE bool AppendEscapedLineTerminator(StringBuffer& sb,
                                                          const char16_t c) {
  switch (c) {
    case '\n':
      if (!sb.append('n')) {
        return false;
      }
      break;
    case '\r':
      if (!sb.append('r')) {
        return false;
      }
      break;
    case 0x2028:
      if (!sb.append("u2028")) {
        return false;
      }
      break;
    case 0x2029:
      if (!sb.append("u2029")) {
        return false;
      }
      break;
    default:
      MOZ_CRASH("Bad LineTerminator");
  }
  return true;
}

template <typename CharT>
static MOZ_ALWAYS_INLINE bool SetupBuffer(StringBuffer& sb,
                                          const CharT* oldChars, size_t oldLen,
                                          const CharT* it) {
  if (mozilla::IsSame<CharT, char16_t>::value && !sb.ensureTwoByteChars()) {
    return false;
  }

  if (!sb.reserve(oldLen + 1)) {
    return false;
  }

  sb.infallibleAppend(oldChars, size_t(it - oldChars));
  return true;
}

// Note: leaves the string buffer empty if no escaping need be performed.
template <typename CharT>
static bool EscapeRegExpPattern(StringBuffer& sb, const CharT* oldChars,
                                size_t oldLen) {
  bool inBrackets = false;
  bool previousCharacterWasBackslash = false;

  for (const CharT* it = oldChars; it < oldChars + oldLen; ++it) {
    CharT ch = *it;
    if (!previousCharacterWasBackslash) {
      if (inBrackets) {
        if (ch == ']') {
          inBrackets = false;
        }
      } else if (ch == '/') {
        // There's a forward slash that needs escaping.
        if (sb.empty()) {
          // This is the first char we've seen that needs escaping,
          // copy everything up to this point.
          if (!SetupBuffer(sb, oldChars, oldLen, it)) {
            return false;
          }
        }
        if (!sb.append('\\')) {
          return false;
        }
      } else if (ch == '[') {
        inBrackets = true;
      }
    }

    if (IsRegExpLineTerminator(ch)) {
      // There's LineTerminator that needs escaping.
      if (sb.empty()) {
        // This is the first char we've seen that needs escaping,
        // copy everything up to this point.
        if (!SetupBuffer(sb, oldChars, oldLen, it)) {
          return false;
        }
      }
      if (!previousCharacterWasBackslash) {
        if (!sb.append('\\')) {
          return false;
        }
      }
      if (!AppendEscapedLineTerminator(sb, ch)) {
        return false;
      }
    } else if (!sb.empty()) {
      if (!sb.append(ch)) {
        return false;
      }
    }

    if (previousCharacterWasBackslash) {
      previousCharacterWasBackslash = false;
    } else if (ch == '\\') {
      previousCharacterWasBackslash = true;
    }
  }

  return true;
}

// ES6 draft rev32 21.2.3.2.4.
JSAtom* js::EscapeRegExpPattern(JSContext* cx, HandleAtom src) {
  // Step 2.
  if (src->length() == 0) {
    return cx->names().emptyRegExp;
  }

  // We may never need to use |sb|. Start using it lazily.
  StringBuffer sb(cx);

  if (src->hasLatin1Chars()) {
    JS::AutoCheckCannotGC nogc;
    if (!::EscapeRegExpPattern(sb, src->latin1Chars(nogc), src->length())) {
      return nullptr;
    }
  } else {
    JS::AutoCheckCannotGC nogc;
    if (!::EscapeRegExpPattern(sb, src->twoByteChars(nogc), src->length())) {
      return nullptr;
    }
  }

  // Step 3.
  return sb.empty() ? src : sb.finishAtom();
}

// ES6 draft rev32 21.2.5.14. Optimized for RegExpObject.
JSFlatString* RegExpObject::toString(JSContext* cx) const {
  // Steps 3-4.
  RootedAtom src(cx, getSource());
  if (!src) {
    return nullptr;
  }
  RootedAtom escapedSrc(cx, EscapeRegExpPattern(cx, src));

  // Step 7.
  JSStringBuilder sb(cx);
  size_t len = escapedSrc->length();
  if (!sb.reserve(len + 2)) {
    return nullptr;
  }
  sb.infallibleAppend('/');
  if (!sb.append(escapedSrc)) {
    return nullptr;
  }
  sb.infallibleAppend('/');

  // Steps 5-7.
  if (global() && !sb.append('g')) {
    return nullptr;
  }
  if (ignoreCase() && !sb.append('i')) {
    return nullptr;
  }
  if (multiline() && !sb.append('m')) {
    return nullptr;
  }
  if (unicode() && !sb.append('u')) {
    return nullptr;
  }
  if (sticky() && !sb.append('y')) {
    return nullptr;
  }

  return sb.finishString();
}

#ifdef DEBUG
/* static */
bool RegExpShared::dumpBytecode(JSContext* cx, MutableHandleRegExpShared re,
                                bool match_only, HandleLinearString input) {
  CompilationMode mode = match_only ? MatchOnly : Normal;
  if (!RegExpShared::compileIfNecessary(cx, re, input, mode, ForceByteCode)) {
    return false;
  }

  const uint8_t* byteCode =
      re->compilation(mode, input->hasLatin1Chars()).byteCode;
  const uint8_t* pc = byteCode;

  auto Load32Aligned = [](const uint8_t* pc) -> int32_t {
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(pc) & 3) == 0);
    return *reinterpret_cast<const int32_t*>(pc);
  };

  auto Load16Aligned = [](const uint8_t* pc) -> int32_t {
    MOZ_ASSERT((reinterpret_cast<uintptr_t>(pc) & 1) == 0);
    return *reinterpret_cast<const uint16_t*>(pc);
  };

  int32_t numRegisters = Load32Aligned(pc);
  fprintf(stderr, "numRegisters: %d\n", numRegisters);
  pc += 4;

  fprintf(stderr, "loc     op\n");
  fprintf(stderr, "-----   --\n");

  auto DumpLower = [](const char* text) {
    while (*text) {
      fprintf(stderr, "%c", unicode::ToLowerCase(*text));
      text++;
    }
  };

#  define BYTECODE(NAME)      \
    case irregexp::BC_##NAME: \
      DumpLower(#NAME);
#  define ADVANCE(NAME)                 \
    fprintf(stderr, "\n");              \
    pc += irregexp::BC_##NAME##_LENGTH; \
    maxPc = js::Max(maxPc, pc);         \
    break;
#  define STOP(NAME)                    \
    fprintf(stderr, "\n");              \
    pc += irregexp::BC_##NAME##_LENGTH; \
    break;
#  define JUMP(NAME, OFFSET)                   \
    fprintf(stderr, "\n");                     \
    maxPc = js::Max(maxPc, byteCode + OFFSET); \
    pc += irregexp::BC_##NAME##_LENGTH;        \
    break;
#  define BRANCH(NAME, OFFSET)                              \
    fprintf(stderr, "\n");                                  \
    pc += irregexp::BC_##NAME##_LENGTH;                     \
    maxPc = js::Max(maxPc, js::Max(pc, byteCode + OFFSET)); \
    break;

  // Bytecode has no end marker, we need to calculate the bytecode length by
  // tracing jumps and branches.
  const uint8_t* maxPc = pc;
  while (pc <= maxPc) {
    fprintf(stderr, "%05d:  ", int32_t(pc - byteCode));
    int32_t insn = Load32Aligned(pc);
    switch (insn & irregexp::BYTECODE_MASK) {
      BYTECODE(BREAK) { STOP(BREAK); }
      BYTECODE(PUSH_CP) { ADVANCE(PUSH_CP); }
      BYTECODE(PUSH_BT) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d", offset);
        // Pushed value is used by POP_BT for jumping.
        // Resolve maxPc here.
        BRANCH(PUSH_BT, offset);
      }
      BYTECODE(PUSH_REGISTER) {
        fprintf(stderr, " reg[%d]", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(PUSH_REGISTER);
      }
      BYTECODE(SET_REGISTER) {
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4));
        ADVANCE(SET_REGISTER);
      }
      BYTECODE(ADVANCE_REGISTER) {
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4));
        ADVANCE(ADVANCE_REGISTER);
      }
      BYTECODE(SET_REGISTER_TO_CP) {
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4));
        ADVANCE(SET_REGISTER_TO_CP);
      }
      BYTECODE(SET_CP_TO_REGISTER) {
        fprintf(stderr, " reg[%d]", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(SET_CP_TO_REGISTER);
      }
      BYTECODE(SET_REGISTER_TO_SP) {
        fprintf(stderr, " reg[%d]", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(SET_REGISTER_TO_SP);
      }
      BYTECODE(SET_SP_TO_REGISTER) {
        fprintf(stderr, " reg[%d]", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(SET_SP_TO_REGISTER);
      }
      BYTECODE(POP_CP) { ADVANCE(POP_CP); }
      BYTECODE(POP_BT) {
        // Jump is already resolved in PUSH_BT.
        STOP(POP_BT);
      }
      BYTECODE(POP_REGISTER) {
        fprintf(stderr, " reg[%d]", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(POP_REGISTER);
      }
      BYTECODE(FAIL) { ADVANCE(FAIL); }
      BYTECODE(SUCCEED) { ADVANCE(SUCCEED); }
      BYTECODE(ADVANCE_CP) {
        fprintf(stderr, " %d", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(ADVANCE_CP);
      }
      BYTECODE(GOTO) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d", offset);
        JUMP(GOTO, offset);
      }
      BYTECODE(ADVANCE_CP_AND_GOTO) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        JUMP(ADVANCE_CP_AND_GOTO, offset);
      }
      BYTECODE(CHECK_GREEDY) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d", offset);
        BRANCH(CHECK_GREEDY, offset);
      }
      BYTECODE(LOAD_CURRENT_CHAR) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(LOAD_CURRENT_CHAR, offset);
      }
      BYTECODE(LOAD_CURRENT_CHAR_UNCHECKED) {
        fprintf(stderr, " %d", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(LOAD_CURRENT_CHAR_UNCHECKED);
      }
      BYTECODE(LOAD_2_CURRENT_CHARS) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(LOAD_2_CURRENT_CHARS, offset);
      }
      BYTECODE(LOAD_2_CURRENT_CHARS_UNCHECKED) {
        fprintf(stderr, " %d", insn >> irregexp::BYTECODE_SHIFT);
        ADVANCE(LOAD_2_CURRENT_CHARS_UNCHECKED);
      }
      BYTECODE(LOAD_4_CURRENT_CHARS) { ADVANCE(LOAD_4_CURRENT_CHARS); }
      BYTECODE(LOAD_4_CURRENT_CHARS_UNCHECKED) {
        ADVANCE(LOAD_4_CURRENT_CHARS_UNCHECKED);
      }
      BYTECODE(CHECK_4_CHARS) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d", Load32Aligned(pc + 4), offset);
        BRANCH(CHECK_4_CHARS, offset);
      }
      BYTECODE(CHECK_CHAR) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(CHECK_CHAR, offset);
      }
      BYTECODE(CHECK_NOT_4_CHARS) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d", Load32Aligned(pc + 4), offset);
        BRANCH(CHECK_NOT_4_CHARS, offset);
      }
      BYTECODE(CHECK_NOT_CHAR) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(CHECK_NOT_CHAR, offset);
      }
      BYTECODE(AND_CHECK_4_CHARS) {
        int32_t offset = Load32Aligned(pc + 12);
        fprintf(stderr, " %d, %d, %d", Load32Aligned(pc + 4),
                Load32Aligned(pc + 8), offset);
        BRANCH(AND_CHECK_4_CHARS, offset);
      }
      BYTECODE(AND_CHECK_CHAR) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4), offset);
        BRANCH(AND_CHECK_CHAR, offset);
      }
      BYTECODE(AND_CHECK_NOT_4_CHARS) {
        int32_t offset = Load32Aligned(pc + 12);
        fprintf(stderr, " %d, %d, %d", Load32Aligned(pc + 4),
                Load32Aligned(pc + 8), offset);
        BRANCH(AND_CHECK_NOT_4_CHARS, offset);
      }
      BYTECODE(AND_CHECK_NOT_CHAR) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4), offset);
        BRANCH(AND_CHECK_NOT_CHAR, offset);
      }
      BYTECODE(MINUS_AND_CHECK_NOT_CHAR) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d, %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load16Aligned(pc + 4), Load16Aligned(pc + 6), offset);
        BRANCH(MINUS_AND_CHECK_NOT_CHAR, offset);
      }
      BYTECODE(CHECK_CHAR_IN_RANGE) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d, %d", Load16Aligned(pc + 4),
                Load16Aligned(pc + 6), offset);
        BRANCH(CHECK_CHAR_IN_RANGE, offset);
      }
      BYTECODE(CHECK_CHAR_NOT_IN_RANGE) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " %d, %d, %d", Load16Aligned(pc + 4),
                Load16Aligned(pc + 6), offset);
        BRANCH(CHECK_CHAR_NOT_IN_RANGE, offset);
      }
      BYTECODE(CHECK_BIT_IN_TABLE) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr,
                " %d, "
                "%02x %02x %02x %02x %02x %02x %02x %02x "
                "%02x %02x %02x %02x %02x %02x %02x %02x",
                offset, pc[8], pc[9], pc[10], pc[11], pc[12], pc[13], pc[14],
                pc[15], pc[16], pc[17], pc[18], pc[19], pc[20], pc[21], pc[22],
                pc[23]);
        BRANCH(CHECK_BIT_IN_TABLE, offset);
      }
      BYTECODE(CHECK_LT) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(CHECK_LT, offset);
      }
      BYTECODE(CHECK_GT) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d, %d", insn >> irregexp::BYTECODE_SHIFT, offset);
        BRANCH(CHECK_GT, offset);
      }
      BYTECODE(CHECK_REGISTER_LT) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " reg[%d], %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4), offset);
        BRANCH(CHECK_REGISTER_LT, offset);
      }
      BYTECODE(CHECK_REGISTER_GE) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " reg[%d], %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4), offset);
        BRANCH(CHECK_REGISTER_GE, offset);
      }
      BYTECODE(CHECK_REGISTER_EQ_POS) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                offset);
        BRANCH(CHECK_REGISTER_EQ_POS, offset);
      }
      BYTECODE(CHECK_NOT_REGS_EQUAL) {
        int32_t offset = Load32Aligned(pc + 8);
        fprintf(stderr, " reg[%d], %d, %d", insn >> irregexp::BYTECODE_SHIFT,
                Load32Aligned(pc + 4), offset);
        BRANCH(CHECK_NOT_REGS_EQUAL, offset);
      }
      BYTECODE(CHECK_NOT_BACK_REF) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                offset);
        BRANCH(CHECK_NOT_BACK_REF, offset);
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                offset);
        BRANCH(CHECK_NOT_BACK_REF_NO_CASE, offset);
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " reg[%d], %d", insn >> irregexp::BYTECODE_SHIFT,
                offset);
        BRANCH(CHECK_NOT_BACK_REF_NO_CASE_UNICODE, offset);
      }
      BYTECODE(CHECK_AT_START) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d", offset);
        BRANCH(CHECK_AT_START, offset);
      }
      BYTECODE(CHECK_NOT_AT_START) {
        int32_t offset = Load32Aligned(pc + 4);
        fprintf(stderr, " %d", offset);
        BRANCH(CHECK_NOT_AT_START, offset);
      }
      BYTECODE(SET_CURRENT_POSITION_FROM_END) {
        fprintf(stderr, " %u",
                static_cast<uint32_t>(insn) >> irregexp::BYTECODE_SHIFT);
        ADVANCE(SET_CURRENT_POSITION_FROM_END);
      }
      default:
        MOZ_CRASH("Bad bytecode");
    }
  }

#  undef BYTECODE
#  undef ADVANCE
#  undef STOP
#  undef JUMP
#  undef BRANCH

  return true;
}

/* static */
bool RegExpObject::dumpBytecode(JSContext* cx, Handle<RegExpObject*> regexp,
                                bool match_only, HandleLinearString input) {
  RootedRegExpShared shared(cx, getShared(cx, regexp));
  if (!shared) {
    return false;
  }

  return RegExpShared::dumpBytecode(cx, &shared, match_only, input);
}
#endif

template <typename CharT>
static MOZ_ALWAYS_INLINE bool IsRegExpMetaChar(CharT ch) {
  switch (ch) {
    /* ES 2016 draft Mar 25, 2016 21.2.1 SyntaxCharacter. */
    case '^':
    case '$':
    case '\\':
    case '.':
    case '*':
    case '+':
    case '?':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
      return true;
    default:
      return false;
  }
}

template <typename CharT>
bool js::HasRegExpMetaChars(const CharT* chars, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (IsRegExpMetaChar<CharT>(chars[i])) {
      return true;
    }
  }
  return false;
}

template bool js::HasRegExpMetaChars<Latin1Char>(const Latin1Char* chars,
                                                 size_t length);

template bool js::HasRegExpMetaChars<char16_t>(const char16_t* chars,
                                               size_t length);

bool js::StringHasRegExpMetaChars(JSLinearString* str) {
  AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return HasRegExpMetaChars(str->latin1Chars(nogc), str->length());
  }

  return HasRegExpMetaChars(str->twoByteChars(nogc), str->length());
}

/* RegExpShared */

RegExpShared::RegExpShared(JSAtom* source, RegExpFlags flags)
    : source(source), parenCount(0), flags(flags), canStringMatch(false) {}

void RegExpShared::traceChildren(JSTracer* trc) {
  // Discard code to avoid holding onto ExecutablePools.
  if (IsMarkingTrace(trc) && trc->runtime()->gc.isShrinkingGC()) {
    discardJitCode();
  }

  TraceNullableEdge(trc, &source, "RegExpShared source");
  for (auto& comp : compilationArray) {
    TraceNullableEdge(trc, &comp.jitCode, "RegExpShared code");
  }
}

void RegExpShared::discardJitCode() {
  for (auto& comp : compilationArray) {
    comp.jitCode = nullptr;
  }

  // We can also purge the tables used by JIT code.
  tables.clearAndFree();
}

void RegExpShared::finalize(JSFreeOp* fop) {
  for (auto& comp : compilationArray) {
    if (comp.byteCode) {
      size_t length = comp.byteCodeLength();
      fop->free_(this, comp.byteCode, length, MemoryUse::RegExpSharedBytecode);
    }
  }
  tables.~JitCodeTables();
}

/* static */
bool RegExpShared::compile(JSContext* cx, MutableHandleRegExpShared re,
                           HandleLinearString input, CompilationMode mode,
                           ForceByteCodeEnum force) {
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  AutoTraceLog logCompile(logger, TraceLogger_IrregexpCompile);

  RootedAtom pattern(cx, re->source);
  return compile(cx, re, pattern, input, mode, force);
}

/* static */
bool RegExpShared::compile(JSContext* cx, MutableHandleRegExpShared re,
                           HandleAtom pattern, HandleLinearString input,
                           CompilationMode mode, ForceByteCodeEnum force) {
  if (!re->ignoreCase() && !StringHasRegExpMetaChars(pattern)) {
    re->canStringMatch = true;
  }

  CompileOptions options(cx);
  frontend::TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);

  /* Parse the pattern. The RegExpCompileData is allocated in LifoAlloc and
   * will only be live while LifoAllocScope is on stack. */
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  irregexp::RegExpCompileData data;
  if (!irregexp::ParsePattern(dummyTokenStream, allocScope.alloc(), pattern,
                              mode == MatchOnly, re->getFlags(), &data)) {
    return false;
  }

  re->parenCount = data.capture_count;

  JitCodeTables tables;
  irregexp::RegExpCode code = irregexp::CompilePattern(
      cx, allocScope.alloc(), re, &data, input, false /* global() */,
      re->ignoreCase(), input->hasLatin1Chars(), mode == MatchOnly,
      force == ForceByteCode, re->sticky(), re->unicode(), tables);
  if (code.empty()) {
    return false;
  }

  MOZ_ASSERT(!code.jitCode || !code.byteCode);
  MOZ_ASSERT_IF(force == ForceByteCode, code.byteCode);

  RegExpCompilation& compilation =
      re->compilation(mode, input->hasLatin1Chars());
  if (code.jitCode) {
    // First copy the tables. GC can purge the tables if the RegExpShared
    // has no JIT code, so it's important to do this right before setting
    // compilation.jitCode (to ensure no purging happens between adding the
    // tables and setting the JIT code).
    for (size_t i = 0; i < tables.length(); i++) {
      if (!re->addTable(std::move(tables[i]))) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
    compilation.jitCode = code.jitCode;
  } else if (code.byteCode) {
    MOZ_ASSERT(tables.empty(), "RegExpInterpreter does not use data tables");
    compilation.byteCode = code.byteCode;
    AddCellMemory(re, compilation.byteCodeLength(),
                  MemoryUse::RegExpSharedBytecode);
  }

  return true;
}

/* static */
bool RegExpShared::compileIfNecessary(JSContext* cx,
                                      MutableHandleRegExpShared re,
                                      HandleLinearString input,
                                      CompilationMode mode,
                                      ForceByteCodeEnum force) {
  if (re->isCompiled(mode, input->hasLatin1Chars(), force)) {
    return true;
  }
  return compile(cx, re, input, mode, force);
}

/* static */
RegExpRunStatus RegExpShared::execute(JSContext* cx,
                                      MutableHandleRegExpShared re,
                                      HandleLinearString input, size_t start,
                                      VectorMatchPairs* matches,
                                      size_t* endIndex) {
  MOZ_ASSERT_IF(matches, !endIndex);
  MOZ_ASSERT_IF(!matches, endIndex);
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);

  CompilationMode mode = matches ? Normal : MatchOnly;

  /* Compile the code at point-of-use. */
  if (!compileIfNecessary(cx, re, input, mode, DontForceByteCode)) {
    return RegExpRunStatus_Error;
  }

  /*
   * Ensure sufficient memory for output vector.
   * No need to initialize it. The RegExp engine fills them in on a match.
   */
  if (matches && !matches->allocOrExpandArray(re->pairCount())) {
    ReportOutOfMemory(cx);
    return RegExpRunStatus_Error;
  }

  size_t length = input->length();

  // Reset the Irregexp backtrack stack if it grows during execution.
  irregexp::RegExpStackScope stackScope(cx);

  if (re->canStringMatch) {
    MOZ_ASSERT(re->pairCount() == 1);
    size_t sourceLength = re->source->length();
    if (re->sticky()) {
      // First part checks size_t overflow.
      if (sourceLength + start < sourceLength ||
          sourceLength + start > length) {
        return RegExpRunStatus_Success_NotFound;
      }
      if (!HasSubstringAt(input, re->source, start)) {
        return RegExpRunStatus_Success_NotFound;
      }

      if (matches) {
        (*matches)[0].start = start;
        (*matches)[0].limit = start + sourceLength;

        matches->checkAgainst(length);
      } else if (endIndex) {
        *endIndex = start + sourceLength;
      }
      return RegExpRunStatus_Success;
    }

    int res = StringFindPattern(input, re->source, start);
    if (res == -1) {
      return RegExpRunStatus_Success_NotFound;
    }

    if (matches) {
      (*matches)[0].start = res;
      (*matches)[0].limit = res + sourceLength;

      matches->checkAgainst(length);
    } else if (endIndex) {
      *endIndex = res + sourceLength;
    }
    return RegExpRunStatus_Success;
  }

  do {
    jit::JitCode* code = re->compilation(mode, input->hasLatin1Chars()).jitCode;
    if (!code) {
      break;
    }

    RegExpRunStatus result;
    {
      AutoTraceLog logJIT(logger, TraceLogger_IrregexpExecute);
      AutoCheckCannotGC nogc;
      if (input->hasLatin1Chars()) {
        const Latin1Char* chars = input->latin1Chars(nogc);
        result = irregexp::ExecuteCode(cx, code, chars, start, length, matches,
                                       endIndex);
      } else {
        const char16_t* chars = input->twoByteChars(nogc);
        result = irregexp::ExecuteCode(cx, code, chars, start, length, matches,
                                       endIndex);
      }
    }

    if (result == RegExpRunStatus_Error) {
      // An 'Error' result is returned if a stack overflow guard or
      // interrupt guard failed. If CheckOverRecursed doesn't throw, break
      // out and retry the regexp in the bytecode interpreter, which can
      // execute while tolerating future interrupts. Otherwise, if we keep
      // getting interrupted we will never finish executing the regexp.
      if (!jit::CheckOverRecursed(cx)) {
        return RegExpRunStatus_Error;
      }
      break;
    }

    if (result == RegExpRunStatus_Success_NotFound) {
      return RegExpRunStatus_Success_NotFound;
    }

    MOZ_ASSERT(result == RegExpRunStatus_Success);

    if (matches) {
      matches->checkAgainst(length);
    }
    return RegExpRunStatus_Success;
  } while (false);

  // Compile bytecode for the RegExp if necessary.
  if (!compileIfNecessary(cx, re, input, mode, ForceByteCode)) {
    return RegExpRunStatus_Error;
  }

  uint8_t* byteCode = re->compilation(mode, input->hasLatin1Chars()).byteCode;
  AutoTraceLog logInterpreter(logger, TraceLogger_IrregexpExecute);

  AutoStableStringChars inputChars(cx);
  if (!inputChars.init(cx, input)) {
    return RegExpRunStatus_Error;
  }

  RegExpRunStatus result;
  if (inputChars.isLatin1()) {
    const Latin1Char* chars = inputChars.latin1Range().begin().get();
    result = irregexp::InterpretCode(cx, byteCode, chars, start, length,
                                     matches, endIndex);
  } else {
    const char16_t* chars = inputChars.twoByteRange().begin().get();
    result = irregexp::InterpretCode(cx, byteCode, chars, start, length,
                                     matches, endIndex);
  }

  if (result == RegExpRunStatus_Success && matches) {
    matches->checkAgainst(length);
  }
  return result;
}

size_t RegExpShared::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  size_t n = 0;

  for (size_t i = 0; i < ArrayLength(compilationArray); i++) {
    const RegExpCompilation& compilation = compilationArray[i];
    if (compilation.byteCode) {
      n += mallocSizeOf(compilation.byteCode);
    }
  }

  n += tables.sizeOfExcludingThis(mallocSizeOf);
  for (size_t i = 0; i < tables.length(); i++) {
    n += mallocSizeOf(tables[i].get());
  }

  return n;
}

/* RegExpRealm */

RegExpRealm::RegExpRealm()
    : matchResultTemplateObject_(nullptr),
      optimizableRegExpPrototypeShape_(nullptr),
      optimizableRegExpInstanceShape_(nullptr) {}

ArrayObject* RegExpRealm::createMatchResultTemplateObject(JSContext* cx) {
  MOZ_ASSERT(!matchResultTemplateObject_);

  /* Create template array object */
  RootedArrayObject templateObject(
      cx, NewDenseUnallocatedArray(cx, RegExpObject::MaxPairCount, nullptr,
                                   TenuredObject));
  if (!templateObject) {
    return matchResultTemplateObject_;  // = nullptr
  }

  // Create a new group for the template.
  Rooted<TaggedProto> proto(cx, templateObject->taggedProto());
  ObjectGroup* group = ObjectGroupRealm::makeGroup(
      cx, templateObject->realm(), templateObject->getClass(), proto);
  if (!group) {
    return matchResultTemplateObject_;  // = nullptr
  }
  templateObject->setGroup(group);

  /* Set dummy index property */
  RootedValue index(cx, Int32Value(0));
  if (!NativeDefineDataProperty(cx, templateObject, cx->names().index, index,
                                JSPROP_ENUMERATE)) {
    return matchResultTemplateObject_;  // = nullptr
  }

  /* Set dummy input property */
  RootedValue inputVal(cx, StringValue(cx->runtime()->emptyString));
  if (!NativeDefineDataProperty(cx, templateObject, cx->names().input, inputVal,
                                JSPROP_ENUMERATE)) {
    return matchResultTemplateObject_;  // = nullptr
  }

  // Make sure that the properties are in the right slots.
  DebugOnly<Shape*> shape = templateObject->lastProperty();
  MOZ_ASSERT(shape->previous()->slot() == MatchResultObjectIndexSlot &&
             shape->previous()->propidRef() == NameToId(cx->names().index));
  MOZ_ASSERT(shape->slot() == MatchResultObjectInputSlot &&
             shape->propidRef() == NameToId(cx->names().input));

  // Make sure type information reflects the indexed properties which might
  // be added.
  AddTypePropertyId(cx, templateObject, JSID_VOID, TypeSet::StringType());
  AddTypePropertyId(cx, templateObject, JSID_VOID, TypeSet::UndefinedType());

  matchResultTemplateObject_.set(templateObject);

  return matchResultTemplateObject_;
}

void RegExpRealm::sweep() {
  if (matchResultTemplateObject_ &&
      IsAboutToBeFinalized(&matchResultTemplateObject_)) {
    matchResultTemplateObject_.set(nullptr);
  }

  if (optimizableRegExpPrototypeShape_ &&
      IsAboutToBeFinalized(&optimizableRegExpPrototypeShape_)) {
    optimizableRegExpPrototypeShape_.set(nullptr);
  }

  if (optimizableRegExpInstanceShape_ &&
      IsAboutToBeFinalized(&optimizableRegExpInstanceShape_)) {
    optimizableRegExpInstanceShape_.set(nullptr);
  }
}

RegExpShared* RegExpZone::get(JSContext* cx, HandleAtom source,
                              RegExpFlags flags) {
  DependentAddPtr<Set> p(cx, set_, Key(source, flags));
  if (p) {
    return *p;
  }

  auto shared = Allocate<RegExpShared>(cx);
  if (!shared) {
    return nullptr;
  }

  new (shared) RegExpShared(source, flags);

  if (!p.add(cx, set_, Key(source, flags), shared)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return shared;
}

RegExpShared* RegExpZone::get(JSContext* cx, HandleAtom atom, JSString* opt) {
  RegExpFlags flags = RegExpFlag::NoFlags;
  if (opt && !ParseRegExpFlags(cx, opt, &flags)) {
    return nullptr;
  }

  return get(cx, atom, flags);
}

size_t RegExpZone::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  return set_.sizeOfExcludingThis(mallocSizeOf);
}

RegExpZone::RegExpZone(Zone* zone) : set_(zone, zone) {}

/* Functions */

JSObject* js::CloneRegExpObject(JSContext* cx, Handle<RegExpObject*> regex) {
  // Unlike RegExpAlloc, all clones must use |regex|'s group.
  RootedObjectGroup group(cx, regex->group());
  Rooted<RegExpObject*> clone(
      cx, NewObjectWithGroup<RegExpObject>(cx, group, GenericObject));
  if (!clone) {
    return nullptr;
  }
  clone->initPrivate(nullptr);

  if (!EmptyShape::ensureInitialCustomShape<RegExpObject>(cx, clone)) {
    return nullptr;
  }

  RegExpShared* shared = RegExpObject::getShared(cx, regex);
  if (!shared) {
    return nullptr;
  }

  clone->initAndZeroLastIndex(shared->getSource(), shared->getFlags(), cx);
  clone->setShared(*shared);

  return clone;
}

template <typename CharT>
static bool ParseRegExpFlags(const CharT* chars, size_t length,
                             RegExpFlags* flagsOut, char16_t* invalidFlag) {
  *flagsOut = RegExpFlag::NoFlags;

  for (size_t i = 0; i < length; i++) {
    uint8_t flag;
    switch (chars[i]) {
      case 'g':
        flag = RegExpFlag::Global;
        break;
      case 'i':
        flag = RegExpFlag::IgnoreCase;
        break;
      case 'm':
        flag = RegExpFlag::Multiline;
        break;
      case 'u':
        flag = RegExpFlag::Unicode;
        break;
      case 'y':
        flag = RegExpFlag::Sticky;
        break;
      default:
        *invalidFlag = chars[i];
        return false;
    }
    if (*flagsOut & flag) {
      *invalidFlag = chars[i];
      return false;
    }
    *flagsOut |= flag;
  }

  return true;
}

bool js::ParseRegExpFlags(JSContext* cx, JSString* flagStr,
                          RegExpFlags* flagsOut) {
  JSLinearString* linear = flagStr->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  size_t len = linear->length();

  bool ok;
  char16_t invalidFlag;
  if (linear->hasLatin1Chars()) {
    AutoCheckCannotGC nogc;
    ok = ::ParseRegExpFlags(linear->latin1Chars(nogc), len, flagsOut,
                            &invalidFlag);
  } else {
    AutoCheckCannotGC nogc;
    ok = ::ParseRegExpFlags(linear->twoByteChars(nogc), len, flagsOut,
                            &invalidFlag);
  }

  if (!ok) {
    TwoByteChars range(&invalidFlag, 1);
    UniqueChars utf8(JS::CharsToNewUTF8CharsZ(cx, range).c_str());
    if (!utf8) {
      return false;
    }
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_BAD_REGEXP_FLAG, utf8.get());
    return false;
  }

  return true;
}

template <XDRMode mode>
XDRResult js::XDRScriptRegExpObject(XDRState<mode>* xdr,
                                    MutableHandle<RegExpObject*> objp) {
  /* NB: Keep this in sync with CloneScriptRegExpObject. */

  RootedAtom source(xdr->cx());
  uint8_t flags = 0;

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(objp);
    RegExpObject& reobj = *objp;
    source = reobj.getSource();
    flags = reobj.getFlags().value();
  }
  MOZ_TRY(XDRAtom(xdr, &source));
  MOZ_TRY(xdr->codeUint8(&flags));
  if (mode == XDR_DECODE) {
    RegExpObject* reobj = RegExpObject::create(
        xdr->cx(), source, RegExpFlags(flags), TenuredObject);
    if (!reobj) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    objp.set(reobj);
  }
  return Ok();
}

template XDRResult js::XDRScriptRegExpObject(XDRState<XDR_ENCODE>* xdr,
                                             MutableHandle<RegExpObject*> objp);

template XDRResult js::XDRScriptRegExpObject(XDRState<XDR_DECODE>* xdr,
                                             MutableHandle<RegExpObject*> objp);

JSObject* js::CloneScriptRegExpObject(JSContext* cx, RegExpObject& reobj) {
  /* NB: Keep this in sync with XDRScriptRegExpObject. */

  RootedAtom source(cx, reobj.getSource());
  cx->markAtom(source);

  return RegExpObject::create(cx, source, reobj.getFlags(), TenuredObject);
}

JS_FRIEND_API RegExpShared* js::RegExpToSharedNonInline(JSContext* cx,
                                                        HandleObject obj) {
  return RegExpToShared(cx, obj);
}

JS::ubi::Node::Size JS::ubi::Concrete<RegExpShared>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return js::gc::Arena::thingSize(gc::AllocKind::REGEXP_SHARED) +
         get().sizeOfExcludingThis(mallocSizeOf);
}

/*
 * Regular Expressions.
 */
JS_PUBLIC_API JSObject* JS::NewRegExpObject(JSContext* cx, const char* bytes,
                                            size_t length, RegExpFlags flags) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  UniqueTwoByteChars chars(InflateString(cx, bytes, length));
  if (!chars) {
    return nullptr;
  }

  return RegExpObject::create(cx, chars.get(), length, flags, GenericObject);
}

JS_PUBLIC_API JSObject* JS::NewUCRegExpObject(JSContext* cx,
                                              const char16_t* chars,
                                              size_t length,
                                              RegExpFlags flags) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  return RegExpObject::create(cx, chars, length, flags, GenericObject);
}

JS_PUBLIC_API bool JS::SetRegExpInput(JSContext* cx, HandleObject obj,
                                      HandleString input) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(input);

  Handle<GlobalObject*> global = obj.as<GlobalObject>();
  RegExpStatics* res = GlobalObject::getRegExpStatics(cx, global);
  if (!res) {
    return false;
  }

  res->reset(input);
  return true;
}

JS_PUBLIC_API bool JS::ClearRegExpStatics(JSContext* cx, HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  MOZ_ASSERT(obj);

  Handle<GlobalObject*> global = obj.as<GlobalObject>();
  RegExpStatics* res = GlobalObject::getRegExpStatics(cx, global);
  if (!res) {
    return false;
  }

  res->clear();
  return true;
}

JS_PUBLIC_API bool JS::ExecuteRegExp(JSContext* cx, HandleObject obj,
                                     HandleObject reobj, char16_t* chars,
                                     size_t length, size_t* indexp, bool test,
                                     MutableHandleValue rval) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  Handle<GlobalObject*> global = obj.as<GlobalObject>();
  RegExpStatics* res = GlobalObject::getRegExpStatics(cx, global);
  if (!res) {
    return false;
  }

  RootedLinearString input(cx, NewStringCopyN<CanGC>(cx, chars, length));
  if (!input) {
    return false;
  }

  return ExecuteRegExpLegacy(cx, res, reobj.as<RegExpObject>(), input, indexp,
                             test, rval);
}

JS_PUBLIC_API bool JS::ExecuteRegExpNoStatics(JSContext* cx, HandleObject obj,
                                              const char16_t* chars,
                                              size_t length, size_t* indexp,
                                              bool test,
                                              MutableHandleValue rval) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  RootedLinearString input(cx, NewStringCopyN<CanGC>(cx, chars, length));
  if (!input) {
    return false;
  }

  return ExecuteRegExpLegacy(cx, nullptr, obj.as<RegExpObject>(), input, indexp,
                             test, rval);
}

JS_PUBLIC_API bool JS::ObjectIsRegExp(JSContext* cx, HandleObject obj,
                                      bool* isRegExp) {
  cx->check(obj);

  ESClass cls;
  if (!GetBuiltinClass(cx, obj, &cls)) {
    return false;
  }

  *isRegExp = cls == ESClass::RegExp;
  return true;
}

JS_PUBLIC_API RegExpFlags JS::GetRegExpFlags(JSContext* cx, HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  RegExpShared* shared = RegExpToShared(cx, obj);
  if (!shared) {
    return RegExpFlag::NoFlags;
  }
  return shared->getFlags();
}

JS_PUBLIC_API JSString* JS::GetRegExpSource(JSContext* cx, HandleObject obj) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);

  RegExpShared* shared = RegExpToShared(cx, obj);
  if (!shared) {
    return nullptr;
  }
  return shared->getSource();
}
