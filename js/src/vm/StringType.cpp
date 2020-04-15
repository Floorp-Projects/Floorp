/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StringType-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Latin1.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"

#include <algorithm>    // std::{all_of,copy_n,enable_if,is_const,move}
#include <type_traits>  // std::is_same, std::is_unsigned

#include "jsfriendapi.h"

#include "frontend/BytecodeCompiler.h"
#include "gc/Marking.h"
#include "gc/MaybeRooted.h"
#include "gc/Nursery.h"
#include "js/CharacterEncoding.h"
#include "js/StableStringChars.h"
#include "js/Symbol.h"
#include "js/UbiNode.h"
#include "util/StringBuffer.h"
#include "util/Unicode.h"
#include "vm/GeckoProfiler.h"
#include "vm/ToSource.h"  // js::ValueToSource

#include "vm/GeckoProfiler-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"

using namespace js;

using mozilla::ArrayEqual;
using mozilla::AssertedCast;
using mozilla::AsWritableChars;
using mozilla::ConvertLatin1toUtf16;
using mozilla::IsAsciiDigit;
using mozilla::IsUtf16Latin1;
using mozilla::LossyConvertUtf16toLatin1;
using mozilla::MakeSpan;
using mozilla::PodCopy;
using mozilla::RangedPtr;
using mozilla::RoundUpPow2;
using mozilla::Unused;

using JS::AutoCheckCannotGC;
using JS::AutoStableStringChars;

using UniqueLatin1Chars = UniquePtr<Latin1Char[], JS::FreePolicy>;

size_t JSString::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  // JSRope: do nothing, we'll count all children chars when we hit the leaf
  // strings.
  if (isRope()) {
    return 0;
  }

  MOZ_ASSERT(isLinear());

  // JSDependentString: do nothing, we'll count the chars when we hit the base
  // string.
  if (isDependent()) {
    return 0;
  }

  // JSExternalString: Ask the embedding to tell us what's going on.
  if (isExternal()) {
    // Our callback isn't supposed to cause GC.
    JS::AutoSuppressGCAnalysis nogc;
    return asExternal().callbacks()->sizeOfBuffer(asExternal().twoByteChars(),
                                                  mallocSizeOf);
  }

  // JSExtensibleString: count the full capacity, not just the used space.
  if (isExtensible()) {
    JSExtensibleString& extensible = asExtensible();
    return extensible.hasLatin1Chars()
               ? mallocSizeOf(extensible.rawLatin1Chars())
               : mallocSizeOf(extensible.rawTwoByteChars());
  }

  // JSInlineString, JSFatInlineString [JSInlineAtom, JSFatInlineAtom]: the
  // chars are inline.
  if (isInline()) {
    return 0;
  }

  // Everything else: measure the space for the chars.
  JSLinearString& linear = asLinear();
  MOZ_ASSERT(linear.ownsMallocedChars());
  return linear.hasLatin1Chars() ? mallocSizeOf(linear.rawLatin1Chars())
                                 : mallocSizeOf(linear.rawTwoByteChars());
}

JS::ubi::Node::Size JS::ubi::Concrete<JSString>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  JSString& str = get();
  size_t size;
  if (str.isAtom()) {
    size =
        str.isFatInline() ? sizeof(js::FatInlineAtom) : sizeof(js::NormalAtom);
  } else {
    size = str.isFatInline() ? sizeof(JSFatInlineString) : sizeof(JSString);
  }

  if (IsInsideNursery(&str)) {
    size += Nursery::stringHeaderSize();
  }

  size += str.sizeOfExcludingThis(mallocSizeOf);

  return size;
}

const char16_t JS::ubi::Concrete<JSString>::concreteTypeName[] = u"JSString";

mozilla::Maybe<mozilla::Tuple<size_t, size_t> > JSString::encodeUTF8Partial(
    const JS::AutoRequireNoGC& nogc, mozilla::Span<char> buffer) const {
  mozilla::Vector<const JSString*, 16, SystemAllocPolicy> stack;
  const JSString* current = this;
  char16_t pendingLeadSurrogate = 0;  // U+0000 means no pending lead surrogate
  size_t totalRead = 0;
  size_t totalWritten = 0;
  for (;;) {
    if (current->isRope()) {
      JSRope& rope = current->asRope();
      if (!stack.append(rope.rightChild())) {
        // OOM
        return mozilla::Nothing();
      }
      current = rope.leftChild();
      continue;
    }

    JSLinearString& linear = current->asLinear();
    if (MOZ_LIKELY(linear.hasLatin1Chars())) {
      if (MOZ_UNLIKELY(pendingLeadSurrogate)) {
        if (buffer.Length() < 3) {
          return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
        }
        buffer[0] = '\xEF';
        buffer[1] = '\xBF';
        buffer[2] = '\xBD';
        buffer = buffer.From(3);
        totalRead += 1;  // pendingLeadSurrogate
        totalWritten += 3;
        pendingLeadSurrogate = 0;
      }
      auto src = mozilla::AsChars(
          mozilla::MakeSpan(linear.latin1Chars(nogc), linear.length()));
      size_t read;
      size_t written;
      mozilla::Tie(read, written) =
          mozilla::ConvertLatin1toUtf8Partial(src, buffer);
      buffer = buffer.From(written);
      totalRead += read;
      totalWritten += written;
      if (read < src.Length()) {
        return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
      }
    } else {
      auto src = mozilla::MakeSpan(linear.twoByteChars(nogc), linear.length());
      if (MOZ_UNLIKELY(pendingLeadSurrogate)) {
        char16_t first = 0;
        if (!src.IsEmpty()) {
          first = src[0];
        }
        if (unicode::IsTrailSurrogate(first)) {
          // Got a surrogate pair
          if (buffer.Length() < 4) {
            return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
          }
          uint32_t astral = unicode::UTF16Decode(pendingLeadSurrogate, first);
          buffer[0] = char(0b1111'0000 | (astral >> 18));
          buffer[1] = char(0b1000'0000 | ((astral >> 12) & 0b11'1111));
          buffer[2] = char(0b1000'0000 | ((astral >> 6) & 0b11'1111));
          buffer[3] = char(0b1000'0000 | (astral & 0b11'1111));
          src = src.From(1);
          buffer = buffer.From(4);
          totalRead += 2;  // both pendingLeadSurrogate and first!
          totalWritten += 4;
        } else {
          // unpaired surrogate
          if (buffer.Length() < 3) {
            return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
          }
          buffer[0] = '\xEF';
          buffer[1] = '\xBF';
          buffer[2] = '\xBD';
          buffer = buffer.From(3);
          totalRead += 1;  // pendingLeadSurrogate
          totalWritten += 3;
        }
        pendingLeadSurrogate = 0;
      }
      if (src.IsEmpty()) {
        return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
      }
      char16_t last = src[src.Length() - 1];
      if (unicode::IsLeadSurrogate(last)) {
        src = src.To(src.Length() - 1);
        pendingLeadSurrogate = last;
      } else {
        MOZ_ASSERT(!pendingLeadSurrogate);
      }
      size_t read;
      size_t written;
      mozilla::Tie(read, written) =
          mozilla::ConvertUtf16toUtf8Partial(src, buffer);
      buffer = buffer.From(written);
      totalRead += read;
      totalWritten += written;
      if (read < src.Length()) {
        return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
      }
    }
    if (stack.empty()) {
      break;
    }
    current = stack.popCopy();
  }
  if (MOZ_UNLIKELY(pendingLeadSurrogate)) {
    if (buffer.Length() < 3) {
      return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
    }
    buffer[0] = '\xEF';
    buffer[1] = '\xBF';
    buffer[2] = '\xBD';
    // No need to update buffer and pendingLeadSurrogate anymore
    totalRead += 1;
    totalWritten += 3;
  }
  return mozilla::Some(mozilla::MakeTuple(totalRead, totalWritten));
}

#if defined(DEBUG) || defined(JS_JITSPEW)

template <typename CharT>
/*static */
void JSString::dumpChars(const CharT* s, size_t n, js::GenericPrinter& out) {
  if (n == SIZE_MAX) {
    n = 0;
    while (s[n]) {
      n++;
    }
  }

  out.put("\"");
  for (size_t i = 0; i < n; i++) {
    char16_t c = s[i];
    if (c == '\n') {
      out.put("\\n");
    } else if (c == '\t') {
      out.put("\\t");
    } else if (c >= 32 && c < 127) {
      out.putChar((char)s[i]);
    } else if (c <= 255) {
      out.printf("\\x%02x", unsigned(c));
    } else {
      out.printf("\\u%04x", unsigned(c));
    }
  }
  out.putChar('"');
}

template void JSString::dumpChars(const Latin1Char* s, size_t n,
                                  js::GenericPrinter& out);

template void JSString::dumpChars(const char16_t* s, size_t n,
                                  js::GenericPrinter& out);

void JSString::dumpCharsNoNewline(js::GenericPrinter& out) {
  if (JSLinearString* linear = ensureLinear(nullptr)) {
    AutoCheckCannotGC nogc;
    if (hasLatin1Chars()) {
      out.put("[Latin 1]");
      dumpChars(linear->latin1Chars(nogc), length(), out);
    } else {
      out.put("[2 byte]");
      dumpChars(linear->twoByteChars(nogc), length(), out);
    }
  } else {
    out.put("(oom in JSString::dumpCharsNoNewline)");
  }
}

void JSString::dump() {
  js::Fprinter out(stderr);
  dump(out);
}

void JSString::dump(js::GenericPrinter& out) {
  dumpNoNewline(out);
  out.putChar('\n');
}

void JSString::dumpNoNewline(js::GenericPrinter& out) {
  if (JSLinearString* linear = ensureLinear(nullptr)) {
    AutoCheckCannotGC nogc;
    if (hasLatin1Chars()) {
      const Latin1Char* chars = linear->latin1Chars(nogc);
      out.printf("JSString* (%p) = Latin1Char * (%p) = ", (void*)this,
                 (void*)chars);
      dumpChars(chars, length(), out);
    } else {
      const char16_t* chars = linear->twoByteChars(nogc);
      out.printf("JSString* (%p) = char16_t * (%p) = ", (void*)this,
                 (void*)chars);
      dumpChars(chars, length(), out);
    }
  } else {
    out.put("(oom in JSString::dump)");
  }
}

void JSString::dumpRepresentation(js::GenericPrinter& out, int indent) const {
  if (isRope()) {
    asRope().dumpRepresentation(out, indent);
  } else if (isDependent()) {
    asDependent().dumpRepresentation(out, indent);
  } else if (isExternal()) {
    asExternal().dumpRepresentation(out, indent);
  } else if (isExtensible()) {
    asExtensible().dumpRepresentation(out, indent);
  } else if (isInline()) {
    asInline().dumpRepresentation(out, indent);
  } else if (isLinear()) {
    asLinear().dumpRepresentation(out, indent);
  } else {
    MOZ_CRASH("Unexpected JSString representation");
  }
}

void JSString::dumpRepresentationHeader(js::GenericPrinter& out,
                                        const char* subclass) const {
  uint32_t flags = JSString::flags();
  // Print the string's address as an actual C++ expression, to facilitate
  // copy-and-paste into a debugger.
  out.printf("((%s*) %p) length: %zu  flags: 0x%x", subclass, this, length(),
             flags);
  if (flags & LINEAR_BIT) out.put(" LINEAR");
  if (flags & DEPENDENT_BIT) out.put(" DEPENDENT");
  if (flags & INLINE_CHARS_BIT) out.put(" INLINE_CHARS");
  if (flags & NON_ATOM_BIT)
    out.put(" NON_ATOM");
  else
    out.put(" (ATOM)");
  if (isPermanentAtom()) out.put(" PERMANENT");
  if (flags & LATIN1_CHARS_BIT) out.put(" LATIN1");
  if (flags & INDEX_VALUE_BIT) out.printf(" INDEX_VALUE(%u)", getIndexValue());
  if (!isTenured()) out.put(" NURSERY");
  out.putChar('\n');
}

void JSLinearString::dumpRepresentationChars(js::GenericPrinter& out,
                                             int indent) const {
  if (hasLatin1Chars()) {
    out.printf("%*schars: ((Latin1Char*) %p) ", indent, "", rawLatin1Chars());
    dumpChars(rawLatin1Chars(), length(), out);
  } else {
    out.printf("%*schars: ((char16_t*) %p) ", indent, "", rawTwoByteChars());
    dumpChars(rawTwoByteChars(), length(), out);
  }
  out.putChar('\n');
}

bool JSString::equals(const char* s) {
  JSLinearString* linear = ensureLinear(nullptr);
  if (!linear) {
    // This is DEBUG-only code.
    fprintf(stderr, "OOM in JSString::equals!\n");
    return false;
  }

  return StringEqualsAscii(linear, s);
}
#endif /* defined(DEBUG) || defined(JS_JITSPEW) */

template <typename CharT>
static MOZ_ALWAYS_INLINE bool AllocChars(JSString* str, size_t length,
                                         CharT** chars, size_t* capacity) {
  /*
   * Grow by 12.5% if the buffer is very large. Otherwise, round up to the
   * next power of 2. This is similar to what we do with arrays; see
   * JSObject::ensureDenseArrayElements.
   */
  static const size_t DOUBLING_MAX = 1024 * 1024;
  *capacity =
      length > DOUBLING_MAX ? length + (length / 8) : RoundUpPow2(length);

  static_assert(JSString::MAX_LENGTH * sizeof(CharT) <= UINT32_MAX);
  *chars =
      str->zone()->pod_arena_malloc<CharT>(js::StringBufferArena, *capacity);
  return *chars != nullptr;
}

UniqueLatin1Chars JSRope::copyLatin1Chars(JSContext* maybecx,
                                          arena_id_t destArenaId) const {
  return copyCharsInternal<Latin1Char>(maybecx, destArenaId);
}

UniqueTwoByteChars JSRope::copyTwoByteChars(JSContext* maybecx,
                                            arena_id_t destArenaId) const {
  return copyCharsInternal<char16_t>(maybecx, destArenaId);
}

template <typename CharT>
UniquePtr<CharT[], JS::FreePolicy> JSRope::copyCharsInternal(
    JSContext* maybecx, arena_id_t destArenaId) const {
  // Left-leaning ropes are far more common than right-leaning ropes, so
  // perform a non-destructive traversal of the rope, right node first,
  // splatting each node's characters into a contiguous buffer.

  size_t n = length();

  UniquePtr<CharT[], JS::FreePolicy> out;
  if (maybecx) {
    out.reset(maybecx->pod_arena_malloc<CharT>(destArenaId, n));
  } else {
    out.reset(js_pod_arena_malloc<CharT>(destArenaId, n));
  }

  if (!out) {
    return nullptr;
  }

  Vector<const JSString*, 8, SystemAllocPolicy> nodeStack;
  const JSString* str = this;
  CharT* end = out.get() + str->length();
  while (true) {
    if (str->isRope()) {
      if (!nodeStack.append(str->asRope().leftChild())) {
        if (maybecx) {
          ReportOutOfMemory(maybecx);
        }
        return nullptr;
      }
      str = str->asRope().rightChild();
    } else {
      end -= str->length();
      CopyChars(end, str->asLinear());
      if (nodeStack.empty()) {
        break;
      }
      str = nodeStack.popCopy();
    }
  }
  MOZ_ASSERT(end == out.get());

  return out;
}

template <typename CharT>
void AddStringToHash(uint32_t* hash, const CharT* chars, size_t len) {
  // It's tempting to use |HashString| instead of this loop, but that's
  // slightly different than our existing implementation for non-ropes. We
  // want to pretend we have a contiguous set of chars so we need to
  // accumulate char by char rather than generate a new hash for substring
  // and then accumulate that.
  for (size_t i = 0; i < len; i++) {
    *hash = mozilla::AddToHash(*hash, chars[i]);
  }
}

void AddStringToHash(uint32_t* hash, const JSString* str) {
  AutoCheckCannotGC nogc;
  const auto& s = str->asLinear();
  if (s.hasLatin1Chars()) {
    AddStringToHash(hash, s.latin1Chars(nogc), s.length());
  } else {
    AddStringToHash(hash, s.twoByteChars(nogc), s.length());
  }
}

bool JSRope::hash(uint32_t* outHash) const {
  Vector<const JSString*, 8, SystemAllocPolicy> nodeStack;
  const JSString* str = this;

  *outHash = 0;

  while (true) {
    if (str->isRope()) {
      if (!nodeStack.append(str->asRope().rightChild())) {
        return false;
      }
      str = str->asRope().leftChild();
    } else {
      AddStringToHash(outHash, str);
      if (nodeStack.empty()) {
        break;
      }
      str = nodeStack.popCopy();
    }
  }

  return true;
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void JSRope::dumpRepresentation(js::GenericPrinter& out, int indent) const {
  dumpRepresentationHeader(out, "JSRope");
  indent += 2;

  out.printf("%*sleft:  ", indent, "");
  leftChild()->dumpRepresentation(out, indent);

  out.printf("%*sright: ", indent, "");
  rightChild()->dumpRepresentation(out, indent);
}
#endif

namespace js {

template <>
void CopyChars(char16_t* dest, const JSLinearString& str) {
  AutoCheckCannotGC nogc;
  if (str.hasTwoByteChars()) {
    PodCopy(dest, str.twoByteChars(nogc), str.length());
  } else {
    CopyAndInflateChars(dest, str.latin1Chars(nogc), str.length());
  }
}

template <>
void CopyChars(Latin1Char* dest, const JSLinearString& str) {
  AutoCheckCannotGC nogc;
  if (str.hasLatin1Chars()) {
    PodCopy(dest, str.latin1Chars(nogc), str.length());
  } else {
    /*
     * When we flatten a TwoByte rope, we turn child ropes (including Latin1
     * ropes) into TwoByte dependent strings. If one of these strings is
     * also part of another Latin1 rope tree, we can have a Latin1 rope with
     * a TwoByte descendent and we end up here when we flatten it. Although
     * the chars are stored as TwoByte, we know they must be in the Latin1
     * range, so we can safely deflate here.
     */
    size_t len = str.length();
    const char16_t* chars = str.twoByteChars(nogc);
    auto src = MakeSpan(chars, len);
    MOZ_ASSERT(IsUtf16Latin1(src));
    LossyConvertUtf16toLatin1(src, AsWritableChars(MakeSpan(dest, len)));
  }
}

} /* namespace js */

template <JSRope::UsingBarrier b, typename CharT>
JSLinearString* JSRope::flattenInternal(JSContext* maybecx) {
  /*
   * Consider the DAG of JSRopes rooted at this JSRope, with non-JSRopes as
   * its leaves. Mutate the root JSRope into a JSExtensibleString containing
   * the full flattened text that the root represents, and mutate all other
   * JSRopes in the interior of the DAG into JSDependentStrings that refer to
   * this new JSExtensibleString.
   *
   * If the leftmost leaf of our DAG is a JSExtensibleString, consider
   * stealing its buffer for use in our new root, and transforming it into a
   * JSDependentString too. Do not mutate any of the other leaves.
   *
   * Perform a depth-first dag traversal, splatting each node's characters
   * into a contiguous buffer. Visit each rope node three times:
   *   1. record position in the buffer and recurse into left child;
   *   2. recurse into the right child;
   *   3. transform the node into a dependent string.
   * To avoid maintaining a stack, tree nodes are mutated to indicate how many
   * times they have been visited. Since ropes can be dags, a node may be
   * encountered multiple times during traversal. However, step 3 above leaves
   * a valid dependent string, so everything works out.
   *
   * While ropes avoid all sorts of quadratic cases with string concatenation,
   * they can't help when ropes are immediately flattened. One idiomatic case
   * that we'd like to keep linear (and has traditionally been linear in SM
   * and other JS engines) is:
   *
   *   while (...) {
   *     s += ...
   *     s.flatten
   *   }
   *
   * Two behaviors accomplish this:
   *
   * - When the leftmost non-rope in the DAG we're flattening is a
   *   JSExtensibleString with sufficient capacity to hold the entire
   *   flattened string, we just flatten the DAG into its buffer. Then, when
   *   we transform the root of the DAG from a JSRope into a
   *   JSExtensibleString, we steal that buffer, and change the victim from a
   *   JSExtensibleString to a JSDependentString. In this case, the left-hand
   *   side of the string never needs to be copied.
   *
   * - Otherwise, we round up the total flattened size and create a fresh
   *   JSExtensibleString with that much capacity. If this in turn becomes the
   *   leftmost leaf of a subsequent flatten, we will hopefully be able to
   *   fill it, as in the case above.
   *
   * Note that, even though the code for creating JSDependentStrings avoids
   * creating dependents of dependents, we can create that situation here: the
   * JSExtensibleStrings we transform into JSDependentStrings might have
   * JSDependentStrings pointing to them already. Stealing the buffer doesn't
   * change its address, only its owning JSExtensibleString, so all chars()
   * pointers in the JSDependentStrings are still valid.
   */
  const size_t wholeLength = length();
  size_t wholeCapacity;
  CharT* wholeChars;
  JSString* str = this;
  CharT* pos;

  // JSString::setFlattenData() is used to store a tagged pointer to the
  // parent node. The tag indicates what to do when we return to the parent.
  static const uintptr_t Tag_Mask = 0x3;
  static const uintptr_t Tag_FinishNode = 0x0;
  static const uintptr_t Tag_VisitRightChild = 0x1;

  AutoCheckCannotGC nogc;

  gc::StoreBuffer* bufferIfNursery = storeBuffer();

  /* Find the left most string, containing the first string. */
  JSRope* leftMostRope = this;
  while (leftMostRope->leftChild()->isRope()) {
    leftMostRope = &leftMostRope->leftChild()->asRope();
  }

  if (leftMostRope->leftChild()->isExtensible()) {
    JSExtensibleString& left = leftMostRope->leftChild()->asExtensible();
    size_t capacity = left.capacity();
    if (capacity >= wholeLength &&
        left.hasTwoByteChars() == std::is_same_v<CharT, char16_t>) {
      wholeChars = const_cast<CharT*>(left.nonInlineChars<CharT>(nogc));
      wholeCapacity = capacity;

      // registerMallocedBuffer is fallible, so attempt it first before doing
      // anything irreversible.
      Nursery& nursery = runtimeFromMainThread()->gc.nursery();
      bool inTenured = !bufferIfNursery;
      if (!inTenured && left.isTenured()) {
        // tenured leftmost child is giving its chars buffer to the
        // nursery-allocated root node.
        if (!nursery.registerMallocedBuffer(wholeChars,
                                            wholeCapacity * sizeof(CharT))) {
          if (maybecx) {
            ReportOutOfMemory(maybecx);
          }
          return nullptr;
        }
        // leftmost child -> root is a tenured -> nursery edge.
        bufferIfNursery->putWholeCell(&left);
      } else if (inTenured && !left.isTenured()) {
        // leftmost child is giving its nursery-held chars buffer to a
        // tenured string.
        nursery.removeMallocedBuffer(wholeChars, wholeCapacity * sizeof(CharT));
      }

      /*
       * Simulate a left-most traversal from the root to leftMost->leftChild()
       * via first_visit_node
       */
      MOZ_ASSERT(str->isRope());
      while (str != leftMostRope) {
        if (b == WithIncrementalBarrier) {
          JSString::writeBarrierPre(str->d.s.u2.left);
          JSString::writeBarrierPre(str->d.s.u3.right);
        }
        JSString* child = str->d.s.u2.left;
        // 'child' will be post-barriered during the later traversal.
        MOZ_ASSERT(child->isRope());
        str->setNonInlineChars(wholeChars);
        child->setFlattenData(uintptr_t(str) | Tag_VisitRightChild);
        str = child;
      }
      if (b == WithIncrementalBarrier) {
        JSString::writeBarrierPre(str->d.s.u2.left);
        JSString::writeBarrierPre(str->d.s.u3.right);
      }
      str->setNonInlineChars(wholeChars);
      uint32_t left_len = left.length();
      pos = wholeChars + left_len;

      // Remove memory association for left node we're about to make into a
      // dependent string.
      if (left.isTenured()) {
        RemoveCellMemory(&left, left.allocSize(), MemoryUse::StringContents);
      }

      if constexpr (std::is_same_v<CharT, char16_t>) {
        left.setLengthAndFlags(left_len, INIT_DEPENDENT_FLAGS);
      } else {
        left.setLengthAndFlags(left_len,
                               INIT_DEPENDENT_FLAGS | LATIN1_CHARS_BIT);
      }
      left.d.s.u3.base = (JSLinearString*)this; /* will be true on exit */
      goto visit_right_child;
    }
  }

  if (!AllocChars(this, wholeLength, &wholeChars, &wholeCapacity)) {
    if (maybecx) {
      ReportOutOfMemory(maybecx);
    }
    return nullptr;
  }

  if (!isTenured()) {
    Nursery& nursery = runtimeFromMainThread()->gc.nursery();
    if (!nursery.registerMallocedBuffer(wholeChars,
                                        wholeCapacity * sizeof(CharT))) {
      js_free(wholeChars);
      if (maybecx) {
        ReportOutOfMemory(maybecx);
      }
      return nullptr;
    }
  }

  pos = wholeChars;
first_visit_node : {
  if (b == WithIncrementalBarrier) {
    JSString::writeBarrierPre(str->d.s.u2.left);
    JSString::writeBarrierPre(str->d.s.u3.right);
  }

  JSString& left = *str->d.s.u2.left;
  str->setNonInlineChars(pos);
  if (left.isRope()) {
    /* Return to this node when 'left' done, then goto visit_right_child. */
    left.setFlattenData(uintptr_t(str) | Tag_VisitRightChild);
    str = &left;
    goto first_visit_node;
  }
  CopyChars(pos, left.asLinear());
  pos += left.length();
}
visit_right_child : {
  JSString& right = *str->d.s.u3.right;
  if (right.isRope()) {
    /* Return to this node when 'right' done, then goto finish_node. */
    right.setFlattenData(uintptr_t(str) | Tag_FinishNode);
    str = &right;
    goto first_visit_node;
  }
  CopyChars(pos, right.asLinear());
  pos += right.length();
}

finish_node : {
  if (str == this) {
    MOZ_ASSERT(pos == wholeChars + wholeLength);
    if constexpr (std::is_same_v<CharT, char16_t>) {
      str->setLengthAndFlags(wholeLength, EXTENSIBLE_FLAGS);
    } else {
      str->setLengthAndFlags(wholeLength, EXTENSIBLE_FLAGS | LATIN1_CHARS_BIT);
    }
    str->setNonInlineChars(wholeChars);
    str->d.s.u3.capacity = wholeCapacity;

    if (str->isTenured()) {
      AddCellMemory(str, str->asLinear().allocSize(),
                    MemoryUse::StringContents);
    }

    return &this->asLinear();
  }
  uintptr_t flattenData;
  uint32_t len = pos - str->nonInlineCharsRaw<CharT>();
  if constexpr (std::is_same_v<CharT, char16_t>) {
    flattenData = str->unsetFlattenData(len, INIT_DEPENDENT_FLAGS);
  } else {
    flattenData =
        str->unsetFlattenData(len, INIT_DEPENDENT_FLAGS | LATIN1_CHARS_BIT);
  }
  str->d.s.u3.base = (JSLinearString*)this; /* will be true on exit */

  // Every interior (rope) node in the rope's tree will be visited during
  // the traversal and post-barriered here, so earlier additions of
  // dependent.base -> root pointers are handled by this barrier as well.
  //
  // The only time post-barriers need do anything is when the root is in
  // the nursery. Note that the root was a rope but will be an extensible
  // string when we return, so it will not point to any strings and need
  // not be barriered.
  gc::StoreBuffer* bufferIfNursery = storeBuffer();
  if (bufferIfNursery && str->isTenured()) {
    bufferIfNursery->putWholeCell(str);
  }

  str = (JSString*)(flattenData & ~Tag_Mask);
  if ((flattenData & Tag_Mask) == Tag_VisitRightChild) {
    goto visit_right_child;
  }
  MOZ_ASSERT((flattenData & Tag_Mask) == Tag_FinishNode);
  goto finish_node;
}
}

template <JSRope::UsingBarrier b>
JSLinearString* JSRope::flattenInternal(JSContext* maybecx) {
  if (hasTwoByteChars()) {
    return flattenInternal<b, char16_t>(maybecx);
  }
  return flattenInternal<b, Latin1Char>(maybecx);
}

JSLinearString* JSRope::flatten(JSContext* maybecx) {
  mozilla::Maybe<AutoGeckoProfilerEntry> entry;
  if (maybecx && !maybecx->isHelperThreadContext()) {
    entry.emplace(maybecx, "JSRope::flatten");
  }

  if (zone()->needsIncrementalBarrier()) {
    return flattenInternal<WithIncrementalBarrier>(maybecx);
  }
  return flattenInternal<NoBarrier>(maybecx);
}

template <AllowGC allowGC>
static JSLinearString* EnsureLinear(
    JSContext* cx,
    typename MaybeRooted<JSString*, allowGC>::HandleType string) {
  JSLinearString* linear = string->ensureLinear(cx);
  // Don't report an exception if GC is not allowed, just return nullptr.
  if (!linear && !allowGC) {
    cx->recoverFromOutOfMemory();
  }
  return linear;
}

template <AllowGC allowGC>
JSString* js::ConcatStrings(
    JSContext* cx, typename MaybeRooted<JSString*, allowGC>::HandleType left,
    typename MaybeRooted<JSString*, allowGC>::HandleType right) {
  MOZ_ASSERT_IF(!left->isAtom(), cx->isInsideCurrentZone(left));
  MOZ_ASSERT_IF(!right->isAtom(), cx->isInsideCurrentZone(right));

  size_t leftLen = left->length();
  if (leftLen == 0) {
    return right;
  }

  size_t rightLen = right->length();
  if (rightLen == 0) {
    return left;
  }

  size_t wholeLength = leftLen + rightLen;
  if (MOZ_UNLIKELY(wholeLength > JSString::MAX_LENGTH)) {
    // Don't report an exception if GC is not allowed, just return nullptr.
    if (allowGC) {
      js::ReportAllocationOverflow(cx);
    }
    return nullptr;
  }

  bool isLatin1 = left->hasLatin1Chars() && right->hasLatin1Chars();
  bool canUseInline = isLatin1
                          ? JSInlineString::lengthFits<Latin1Char>(wholeLength)
                          : JSInlineString::lengthFits<char16_t>(wholeLength);
  if (canUseInline) {
    Latin1Char* latin1Buf = nullptr;  // initialize to silence GCC warning
    char16_t* twoByteBuf = nullptr;   // initialize to silence GCC warning
    JSInlineString* str =
        isLatin1 ? AllocateInlineString<allowGC>(cx, wholeLength, &latin1Buf)
                 : AllocateInlineString<allowGC>(cx, wholeLength, &twoByteBuf);
    if (!str) {
      return nullptr;
    }

    AutoCheckCannotGC nogc;
    JSLinearString* leftLinear = EnsureLinear<allowGC>(cx, left);
    if (!leftLinear) {
      return nullptr;
    }
    JSLinearString* rightLinear = EnsureLinear<allowGC>(cx, right);
    if (!rightLinear) {
      return nullptr;
    }

    if (isLatin1) {
      PodCopy(latin1Buf, leftLinear->latin1Chars(nogc), leftLen);
      PodCopy(latin1Buf + leftLen, rightLinear->latin1Chars(nogc), rightLen);
    } else {
      if (leftLinear->hasTwoByteChars()) {
        PodCopy(twoByteBuf, leftLinear->twoByteChars(nogc), leftLen);
      } else {
        CopyAndInflateChars(twoByteBuf, leftLinear->latin1Chars(nogc), leftLen);
      }
      if (rightLinear->hasTwoByteChars()) {
        PodCopy(twoByteBuf + leftLen, rightLinear->twoByteChars(nogc),
                rightLen);
      } else {
        CopyAndInflateChars(twoByteBuf + leftLen,
                            rightLinear->latin1Chars(nogc), rightLen);
      }
    }

    return str;
  }

  return JSRope::new_<allowGC>(cx, left, right, wholeLength);
}

template JSString* js::ConcatStrings<CanGC>(JSContext* cx, HandleString left,
                                            HandleString right);

template JSString* js::ConcatStrings<NoGC>(JSContext* cx, JSString* const& left,
                                           JSString* const& right);

/**
 * Copy |src[0..length]| to |dest[0..length]| when copying doesn't narrow and
 * therefore can't lose information.
 */
static inline void FillChars(char16_t* dest, const unsigned char* src,
                             size_t length) {
  ConvertLatin1toUtf16(AsChars(MakeSpan(src, length)), MakeSpan(dest, length));
}

static inline void FillChars(char16_t* dest, const char16_t* src,
                             size_t length) {
  PodCopy(dest, src, length);
}

static inline void FillChars(unsigned char* dest, const unsigned char* src,
                             size_t length) {
  PodCopy(dest, src, length);
}

static inline void FillChars(char16_t* dest, LittleEndianChars src,
                             size_t length) {
#if MOZ_LITTLE_ENDIAN()
  memcpy(dest, src.get(), length * sizeof(char16_t));
#else
  for (size_t i = 0; i < length; ++i) {
    dest[i] = src[i];
  }
#endif
}

/**
 * Copy |src[0..length]| to |dest[0..length]| when copying *does* narrow, but
 * the user guarantees every runtime |src[i]| value can be stored without change
 * of value in |dest[i]|.
 */
static inline void FillFromCompatible(unsigned char* dest, const char16_t* src,
                                      size_t length) {
  LossyConvertUtf16toLatin1(MakeSpan(src, length),
                            AsWritableChars(MakeSpan(dest, length)));
}

static inline void FillFromCompatible(unsigned char* dest,
                                      LittleEndianChars src, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    dest[i] = AssertedCast<unsigned char>(src[i]);
  }
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void JSDependentString::dumpRepresentation(js::GenericPrinter& out,
                                           int indent) const {
  dumpRepresentationHeader(out, "JSDependentString");
  indent += 2;
  out.printf("%*soffset: %zu\n", indent, "", baseOffset());
  out.printf("%*sbase: ", indent, "");
  base()->dumpRepresentation(out, indent);
}
#endif

bool js::EqualChars(JSLinearString* str1, JSLinearString* str2) {
  MOZ_ASSERT(str1->length() == str2->length());

  size_t len = str1->length();

  AutoCheckCannotGC nogc;
  if (str1->hasTwoByteChars()) {
    if (str2->hasTwoByteChars()) {
      return ArrayEqual(str1->twoByteChars(nogc), str2->twoByteChars(nogc),
                        len);
    }

    return EqualChars(str2->latin1Chars(nogc), str1->twoByteChars(nogc), len);
  }

  if (str2->hasLatin1Chars()) {
    return ArrayEqual(str1->latin1Chars(nogc), str2->latin1Chars(nogc), len);
  }

  return EqualChars(str1->latin1Chars(nogc), str2->twoByteChars(nogc), len);
}

bool js::HasSubstringAt(JSLinearString* text, JSLinearString* pat,
                        size_t start) {
  MOZ_ASSERT(start + pat->length() <= text->length());

  size_t patLen = pat->length();

  AutoCheckCannotGC nogc;
  if (text->hasLatin1Chars()) {
    const Latin1Char* textChars = text->latin1Chars(nogc) + start;
    if (pat->hasLatin1Chars()) {
      return ArrayEqual(textChars, pat->latin1Chars(nogc), patLen);
    }

    return EqualChars(textChars, pat->twoByteChars(nogc), patLen);
  }

  const char16_t* textChars = text->twoByteChars(nogc) + start;
  if (pat->hasTwoByteChars()) {
    return ArrayEqual(textChars, pat->twoByteChars(nogc), patLen);
  }

  return EqualChars(pat->latin1Chars(nogc), textChars, patLen);
}

bool js::EqualStrings(JSContext* cx, JSString* str1, JSString* str2,
                      bool* result) {
  if (str1 == str2) {
    *result = true;
    return true;
  }

  size_t length1 = str1->length();
  if (length1 != str2->length()) {
    *result = false;
    return true;
  }

  JSLinearString* linear1 = str1->ensureLinear(cx);
  if (!linear1) {
    return false;
  }
  JSLinearString* linear2 = str2->ensureLinear(cx);
  if (!linear2) {
    return false;
  }

  *result = EqualChars(linear1, linear2);
  return true;
}

bool js::EqualStrings(JSLinearString* str1, JSLinearString* str2) {
  if (str1 == str2) {
    return true;
  }

  size_t length1 = str1->length();
  if (length1 != str2->length()) {
    return false;
  }

  return EqualChars(str1, str2);
}

int32_t js::CompareChars(const char16_t* s1, size_t len1, JSLinearString* s2) {
  AutoCheckCannotGC nogc;
  return s2->hasLatin1Chars()
             ? CompareChars(s1, len1, s2->latin1Chars(nogc), s2->length())
             : CompareChars(s1, len1, s2->twoByteChars(nogc), s2->length());
}

static int32_t CompareStringsImpl(JSLinearString* str1, JSLinearString* str2) {
  size_t len1 = str1->length();
  size_t len2 = str2->length();

  AutoCheckCannotGC nogc;
  if (str1->hasLatin1Chars()) {
    const Latin1Char* chars1 = str1->latin1Chars(nogc);
    return str2->hasLatin1Chars()
               ? CompareChars(chars1, len1, str2->latin1Chars(nogc), len2)
               : CompareChars(chars1, len1, str2->twoByteChars(nogc), len2);
  }

  const char16_t* chars1 = str1->twoByteChars(nogc);
  return str2->hasLatin1Chars()
             ? CompareChars(chars1, len1, str2->latin1Chars(nogc), len2)
             : CompareChars(chars1, len1, str2->twoByteChars(nogc), len2);
}

bool js::CompareStrings(JSContext* cx, JSString* str1, JSString* str2,
                        int32_t* result) {
  MOZ_ASSERT(str1);
  MOZ_ASSERT(str2);

  if (str1 == str2) {
    *result = 0;
    return true;
  }

  JSLinearString* linear1 = str1->ensureLinear(cx);
  if (!linear1) {
    return false;
  }

  JSLinearString* linear2 = str2->ensureLinear(cx);
  if (!linear2) {
    return false;
  }

  *result = CompareStringsImpl(linear1, linear2);
  return true;
}

int32_t js::CompareAtoms(JSAtom* atom1, JSAtom* atom2) {
  return CompareStringsImpl(atom1, atom2);
}

bool js::StringIsAscii(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  if (str->hasLatin1Chars()) {
    return mozilla::IsAscii(
        AsChars(MakeSpan(str->latin1Chars(nogc), str->length())));
  }
  return mozilla::IsAscii(MakeSpan(str->twoByteChars(nogc), str->length()));
}

bool js::StringEqualsAscii(JSLinearString* str, const char* asciiBytes) {
  return StringEqualsAscii(str, asciiBytes, strlen(asciiBytes));
}

bool js::StringEqualsAscii(JSLinearString* str, const char* asciiBytes,
                           size_t length) {
  MOZ_ASSERT(JS::StringIsASCII(MakeSpan(asciiBytes, length)));

  if (length != str->length()) {
    return false;
  }

  const Latin1Char* latin1 = reinterpret_cast<const Latin1Char*>(asciiBytes);

  AutoCheckCannotGC nogc;
  return str->hasLatin1Chars()
             ? ArrayEqual(latin1, str->latin1Chars(nogc), length)
             : EqualChars(latin1, str->twoByteChars(nogc), length);
}

template <typename CharT>
/* static */
bool JSLinearString::isIndexSlow(const CharT* s, size_t length,
                                 uint32_t* indexp) {
  MOZ_ASSERT(length > 0);
  MOZ_ASSERT(length <= UINT32_CHAR_BUFFER_LENGTH);
  MOZ_ASSERT(IsAsciiDigit(*s),
             "caller's fast path must have checked first char");

  RangedPtr<const CharT> cp(s, length);
  const RangedPtr<const CharT> end(s + length, s, length);

  uint32_t index = AsciiDigitToNumber(*cp++);
  uint32_t oldIndex = 0;
  uint32_t c = 0;

  if (index != 0) {
    while (cp < end && IsAsciiDigit(*cp)) {
      oldIndex = index;
      c = AsciiDigitToNumber(*cp);
      index = 10 * index + c;
      cp++;
    }
  }

  /* It's not an element if there are characters after the number. */
  if (cp != end) {
    return false;
  }

  /*
   * Look out for "4294967296" and larger-number strings that fit in
   * UINT32_CHAR_BUFFER_LENGTH: only unsigned 32-bit integers shall pass.
   */
  if (oldIndex < UINT32_MAX / 10 ||
      (oldIndex == UINT32_MAX / 10 && c <= (UINT32_MAX % 10))) {
    *indexp = index;
    return true;
  }

  return false;
}

template bool JSLinearString::isIndexSlow(const Latin1Char* s, size_t length,
                                          uint32_t* indexp);

template bool JSLinearString::isIndexSlow(const char16_t* s, size_t length,
                                          uint32_t* indexp);

/*
 * Declare length-2 strings. We only store strings where both characters are
 * alphanumeric. The lower 10 short chars are the numerals, the next 26 are
 * the lowercase letters, and the next 26 are the uppercase letters.
 */

constexpr Latin1Char StaticStrings::fromSmallChar(SmallChar c) {
  if (c < 10) {
    return c + '0';
  }
  if (c < 36) {
    return c + 'a' - 10;
  }
  return c + 'A' - 36;
}

constexpr StaticStrings::SmallChar StaticStrings::toSmallChar(uint32_t c) {
  if (mozilla::IsAsciiDigit(c)) {
    return c - '0';
  }
  if (mozilla::IsAsciiLowercaseAlpha(c)) {
    return c - 'a' + 10;
  }
  if (mozilla::IsAsciiUppercaseAlpha(c)) {
    return c - 'A' + 36;
  }
  return StaticStrings::INVALID_SMALL_CHAR;
}

constexpr StaticStrings::SmallCharArray StaticStrings::createSmallCharArray() {
  SmallCharArray array{};
  for (size_t i = 0; i < SMALL_CHAR_LIMIT; i++) {
    array[i] = toSmallChar(i);
  }
  return array;
}

const StaticStrings::SmallCharArray StaticStrings::toSmallCharArray =
    createSmallCharArray();

bool StaticStrings::init(JSContext* cx) {
  AutoAllocInAtomsZone az(cx);

  static_assert(UNIT_STATIC_LIMIT - 1 <= JSString::MAX_LATIN1_CHAR,
                "Unit strings must fit in Latin1Char.");

  using Latin1Range = mozilla::Range<const Latin1Char>;

  for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++) {
    Latin1Char ch = Latin1Char(i);
    JSLinearString* s = NewInlineString<NoGC>(cx, Latin1Range(&ch, 1));
    if (!s) {
      return false;
    }
    HashNumber hash = mozilla::HashString(&ch, 1);
    unitStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
  }

  for (uint32_t i = 0; i < NUM_SMALL_CHARS * NUM_SMALL_CHARS; i++) {
    Latin1Char buffer[] = {fromSmallChar(i >> 6), fromSmallChar(i & 0x3F)};
    JSLinearString* s = NewInlineString<NoGC>(cx, Latin1Range(buffer, 2));
    if (!s) {
      return false;
    }
    HashNumber hash = mozilla::HashString(buffer, 2);
    length2StaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
  }

  for (uint32_t i = 0; i < INT_STATIC_LIMIT; i++) {
    if (i < 10) {
      intStaticTable[i] = unitStaticTable[i + '0'];
    } else if (i < 100) {
      size_t index = ((size_t)toSmallChar((i / 10) + '0') << 6) +
                     toSmallChar((i % 10) + '0');
      intStaticTable[i] = length2StaticTable[index];
    } else {
      Latin1Char buffer[] = {Latin1Char('0' + (i / 100)),
                             Latin1Char('0' + ((i / 10) % 10)),
                             Latin1Char('0' + (i % 10))};
      JSLinearString* s = NewInlineString<NoGC>(cx, Latin1Range(buffer, 3));
      if (!s) {
        return false;
      }
      HashNumber hash = mozilla::HashString(buffer, 3);
      intStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
    }

    // Static string initialization can not race, so allow even without the
    // lock.
    intStaticTable[i]->maybeInitializeIndex(i, true);
  }

  return true;
}

inline void TraceStaticString(JSTracer* trc, JSAtom* atom, const char* name) {
  MOZ_ASSERT(atom->isPinned());
  TraceProcessGlobalRoot(trc, atom, name);
}

void StaticStrings::trace(JSTracer* trc) {
  /* These strings never change, so barriers are not needed. */

  for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++) {
    TraceStaticString(trc, unitStaticTable[i], "unit-static-string");
  }

  for (uint32_t i = 0; i < NUM_SMALL_CHARS * NUM_SMALL_CHARS; i++) {
    TraceStaticString(trc, length2StaticTable[i], "length2-static-string");
  }

  /* This may mark some strings more than once, but so be it. */
  for (uint32_t i = 0; i < INT_STATIC_LIMIT; i++) {
    TraceStaticString(trc, intStaticTable[i], "int-static-string");
  }
}

bool AutoStableStringChars::init(JSContext* cx, JSString* s) {
  RootedLinearString linearString(cx, s->ensureLinear(cx));
  if (!linearString) {
    return false;
  }

  MOZ_ASSERT(state_ == Uninitialized);

  // If the chars are inline then we need to copy them since they may be moved
  // by a compacting GC.
  if (baseIsInline(linearString)) {
    return linearString->hasTwoByteChars() ? copyTwoByteChars(cx, linearString)
                                           : copyLatin1Chars(cx, linearString);
  }

  if (linearString->hasLatin1Chars()) {
    state_ = Latin1;
    latin1Chars_ = linearString->rawLatin1Chars();
  } else {
    state_ = TwoByte;
    twoByteChars_ = linearString->rawTwoByteChars();
  }

  s_ = linearString;
  return true;
}

bool AutoStableStringChars::initTwoByte(JSContext* cx, JSString* s) {
  RootedLinearString linearString(cx, s->ensureLinear(cx));
  if (!linearString) {
    return false;
  }

  MOZ_ASSERT(state_ == Uninitialized);

  if (linearString->hasLatin1Chars()) {
    return copyAndInflateLatin1Chars(cx, linearString);
  }

  // If the chars are inline then we need to copy them since they may be moved
  // by a compacting GC.
  if (baseIsInline(linearString)) {
    return copyTwoByteChars(cx, linearString);
  }

  state_ = TwoByte;
  twoByteChars_ = linearString->rawTwoByteChars();
  s_ = linearString;
  return true;
}

bool AutoStableStringChars::baseIsInline(HandleLinearString linearString) {
  JSString* base = linearString;
  while (base->isDependent()) {
    base = base->asDependent().base();
  }
  return base->isInline();
}

template <typename T>
T* AutoStableStringChars::allocOwnChars(JSContext* cx, size_t count) {
  static_assert(
      InlineCapacity >=
              sizeof(JS::Latin1Char) * JSFatInlineString::MAX_LENGTH_LATIN1 &&
          InlineCapacity >=
              sizeof(char16_t) * JSFatInlineString::MAX_LENGTH_TWO_BYTE,
      "InlineCapacity too small to hold fat inline strings");

  static_assert((JSString::MAX_LENGTH &
                 mozilla::tl::MulOverflowMask<sizeof(T)>::value) == 0,
                "Size calculation can overflow");
  MOZ_ASSERT(count <= JSString::MAX_LENGTH);
  size_t size = sizeof(T) * count;

  ownChars_.emplace(cx);
  if (!ownChars_->resize(size)) {
    ownChars_.reset();
    return nullptr;
  }

  return reinterpret_cast<T*>(ownChars_->begin());
}

bool AutoStableStringChars::copyAndInflateLatin1Chars(
    JSContext* cx, HandleLinearString linearString) {
  char16_t* chars = allocOwnChars<char16_t>(cx, linearString->length());
  if (!chars) {
    return false;
  }

  FillChars(chars, linearString->rawLatin1Chars(), linearString->length());

  state_ = TwoByte;
  twoByteChars_ = chars;
  s_ = linearString;
  return true;
}

bool AutoStableStringChars::copyLatin1Chars(JSContext* cx,
                                            HandleLinearString linearString) {
  size_t length = linearString->length();
  JS::Latin1Char* chars = allocOwnChars<JS::Latin1Char>(cx, length);
  if (!chars) {
    return false;
  }

  FillChars(chars, linearString->rawLatin1Chars(), length);

  state_ = Latin1;
  latin1Chars_ = chars;
  s_ = linearString;
  return true;
}

bool AutoStableStringChars::copyTwoByteChars(JSContext* cx,
                                             HandleLinearString linearString) {
  size_t length = linearString->length();
  char16_t* chars = allocOwnChars<char16_t>(cx, length);
  if (!chars) {
    return false;
  }

  FillChars(chars, linearString->rawTwoByteChars(), length);

  state_ = TwoByte;
  twoByteChars_ = chars;
  s_ = linearString;
  return true;
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void JSAtom::dump(js::GenericPrinter& out) {
  out.printf("JSAtom* (%p) = ", (void*)this);
  this->JSString::dump(out);
}

void JSAtom::dump() {
  Fprinter out(stderr);
  dump(out);
}

void JSExternalString::dumpRepresentation(js::GenericPrinter& out,
                                          int indent) const {
  dumpRepresentationHeader(out, "JSExternalString");
  indent += 2;

  out.printf("%*sfinalizer: ((JSExternalStringCallbacks*) %p)\n", indent, "",
             callbacks());
  dumpRepresentationChars(out, indent);
}
#endif /* defined(DEBUG) || defined(JS_JITSPEW) */

JSLinearString* js::NewDependentString(JSContext* cx, JSString* baseArg,
                                       size_t start, size_t length) {
  if (length == 0) {
    return cx->emptyString();
  }

  JSLinearString* base = baseArg->ensureLinear(cx);
  if (!base) {
    return nullptr;
  }

  if (start == 0 && length == base->length()) {
    return base;
  }

  if (base->hasTwoByteChars()) {
    AutoCheckCannotGC nogc;
    const char16_t* chars = base->twoByteChars(nogc) + start;
    if (JSLinearString* staticStr = cx->staticStrings().lookup(chars, length)) {
      return staticStr;
    }
  } else {
    AutoCheckCannotGC nogc;
    const Latin1Char* chars = base->latin1Chars(nogc) + start;
    if (JSLinearString* staticStr = cx->staticStrings().lookup(chars, length)) {
      return staticStr;
    }
  }

  return JSDependentString::new_(cx, base, start, length);
}

static inline bool CanStoreCharsAsLatin1(const char16_t* s, size_t length) {
  return IsUtf16Latin1(MakeSpan(s, length));
}

static bool CanStoreCharsAsLatin1(LittleEndianChars chars, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (chars[i] > JSString::MAX_LATIN1_CHAR) {
      return false;
    }
  }

  return true;
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE JSInlineString* NewInlineStringDeflated(
    JSContext* cx, mozilla::Range<const char16_t> chars) {
  size_t len = chars.length();
  Latin1Char* storage;
  JSInlineString* str = AllocateInlineString<allowGC>(cx, len, &storage);
  if (!str) {
    return nullptr;
  }

  MOZ_ASSERT(CanStoreCharsAsLatin1(chars.begin().get(), len));
  FillFromCompatible(storage, chars.begin().get(), len);
  return str;
}

template <AllowGC allowGC>
static JSLinearString* NewStringDeflated(JSContext* cx, const char16_t* s,
                                         size_t n) {
  if (JSLinearString* str = TryEmptyOrStaticString(cx, s, n)) {
    return str;
  }

  if (JSInlineString::lengthFits<Latin1Char>(n)) {
    return NewInlineStringDeflated<allowGC>(
        cx, mozilla::Range<const char16_t>(s, n));
  }

  auto news = cx->make_pod_arena_array<Latin1Char>(js::StringBufferArena, n);
  if (!news) {
    if (!allowGC) {
      cx->recoverFromOutOfMemory();
    }
    return nullptr;
  }

  MOZ_ASSERT(CanStoreCharsAsLatin1(s, n));
  FillFromCompatible(news.get(), s, n);

  return JSLinearString::new_<allowGC>(cx, std::move(news), n);
}

static JSLinearString* NewStringDeflatedFromLittleEndianNoGC(
    JSContext* cx, LittleEndianChars chars, size_t length) {
  MOZ_ASSERT(CanStoreCharsAsLatin1(chars, length));

  if (JSInlineString::lengthFits<Latin1Char>(length)) {
    Latin1Char* storage;
    JSInlineString* str = AllocateInlineString<NoGC>(cx, length, &storage);
    if (!str) {
      return nullptr;
    }

    FillFromCompatible(storage, chars, length);
    return str;
  }

  auto news =
      cx->make_pod_arena_array<Latin1Char>(js::StringBufferArena, length);
  if (!news) {
    cx->recoverFromOutOfMemory();
    return nullptr;
  }

  FillFromCompatible(news.get(), chars, length);

  return JSLinearString::new_<NoGC>(cx, std::move(news), length);
}

template <typename CharT>
JSLinearString* js::NewStringDontDeflate(
    JSContext* cx, UniquePtr<CharT[], JS::FreePolicy> chars, size_t length) {
  if (JSLinearString* str = TryEmptyOrStaticString(cx, chars.get(), length)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(length)) {
    // |chars.get()| is safe because 1) |NewInlineString| necessarily *copies*,
    // and 2) |chars| frees its contents only when this function returns.
    return NewInlineString<CanGC>(
        cx, mozilla::Range<const CharT>(chars.get(), length));
  }

  return JSLinearString::new_<CanGC>(cx, std::move(chars), length);
}

template JSLinearString* js::NewStringDontDeflate(JSContext* cx,
                                                  UniqueTwoByteChars chars,
                                                  size_t length);

template JSLinearString* js::NewStringDontDeflate(JSContext* cx,
                                                  UniqueLatin1Chars chars,
                                                  size_t length);

template <typename CharT>
JSLinearString* js::NewString(JSContext* cx,
                              UniquePtr<CharT[], JS::FreePolicy> chars,
                              size_t length) {
  if constexpr (std::is_same_v<CharT, char16_t>) {
    if (CanStoreCharsAsLatin1(chars.get(), length)) {
      // Deflating copies from |chars.get()| and lets |chars| be freed on
      // return.
      return NewStringDeflated<CanGC>(cx, chars.get(), length);
    }
  }

  return NewStringDontDeflate(cx, std::move(chars), length);
}

template JSLinearString* js::NewString(JSContext* cx, UniqueTwoByteChars chars,
                                       size_t length);

template JSLinearString* js::NewString(JSContext* cx, UniqueLatin1Chars chars,
                                       size_t length);

template <AllowGC allowGC, typename CharT>
JSLinearString* js::NewStringDontDeflate(
    JSContext* cx, UniquePtr<CharT[], JS::FreePolicy> chars, size_t length) {
  if (JSLinearString* str = TryEmptyOrStaticString(cx, chars.get(), length)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(length)) {
    return NewInlineString<allowGC>(
        cx, mozilla::Range<const CharT>(chars.get(), length));
  }

  return JSLinearString::new_<allowGC>(cx, std::move(chars), length);
}

template JSLinearString* js::NewStringDontDeflate<CanGC>(
    JSContext* cx, UniqueTwoByteChars chars, size_t length);

template JSLinearString* js::NewStringDontDeflate<NoGC>(
    JSContext* cx, UniqueTwoByteChars chars, size_t length);

template JSLinearString* js::NewStringDontDeflate<CanGC>(
    JSContext* cx, UniqueLatin1Chars chars, size_t length);

template JSLinearString* js::NewStringDontDeflate<NoGC>(JSContext* cx,
                                                        UniqueLatin1Chars chars,
                                                        size_t length);

template <AllowGC allowGC, typename CharT>
JSLinearString* js::NewString(JSContext* cx,
                              UniquePtr<CharT[], JS::FreePolicy> chars,
                              size_t length) {
  if constexpr (std::is_same_v<CharT, char16_t>) {
    if (CanStoreCharsAsLatin1(chars.get(), length)) {
      return NewStringDeflated<allowGC>(cx, chars.get(), length);
    }
  }

  return NewStringDontDeflate<allowGC>(cx, std::move(chars), length);
}

template JSLinearString* js::NewString<CanGC>(JSContext* cx,
                                              UniqueTwoByteChars chars,
                                              size_t length);

template JSLinearString* js::NewString<NoGC>(JSContext* cx,
                                             UniqueTwoByteChars chars,
                                             size_t length);

template JSLinearString* js::NewString<CanGC>(JSContext* cx,
                                              UniqueLatin1Chars chars,
                                              size_t length);

template JSLinearString* js::NewString<NoGC>(JSContext* cx,
                                             UniqueLatin1Chars chars,
                                             size_t length);

namespace js {

template <AllowGC allowGC, typename CharT>
JSLinearString* NewStringCopyNDontDeflate(JSContext* cx, const CharT* s,
                                          size_t n) {
  if (JSLinearString* str = TryEmptyOrStaticString(cx, s, n)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(n)) {
    return NewInlineString<allowGC>(cx, mozilla::Range<const CharT>(s, n));
  }

  auto news = cx->make_pod_arena_array<CharT>(js::StringBufferArena, n);
  if (!news) {
    if (!allowGC) {
      cx->recoverFromOutOfMemory();
    }
    return nullptr;
  }

  FillChars(news.get(), s, n);

  return JSLinearString::new_<allowGC>(cx, std::move(news), n);
}

template JSLinearString* NewStringCopyNDontDeflate<CanGC>(JSContext* cx,
                                                          const char16_t* s,
                                                          size_t n);

template JSLinearString* NewStringCopyNDontDeflate<NoGC>(JSContext* cx,
                                                         const char16_t* s,
                                                         size_t n);

template JSLinearString* NewStringCopyNDontDeflate<CanGC>(JSContext* cx,
                                                          const Latin1Char* s,
                                                          size_t n);

template JSLinearString* NewStringCopyNDontDeflate<NoGC>(JSContext* cx,
                                                         const Latin1Char* s,
                                                         size_t n);

static JSLinearString* NewUndeflatedStringFromLittleEndianNoGC(
    JSContext* cx, LittleEndianChars chars, size_t length) {
  if (JSInlineString::lengthFits<char16_t>(length)) {
    char16_t* storage;
    JSInlineString* str = AllocateInlineString<NoGC>(cx, length, &storage);
    if (!str) {
      return nullptr;
    }

    FillChars(storage, chars, length);
    return str;
  }

  auto news = cx->make_pod_arena_array<char16_t>(js::StringBufferArena, length);
  if (!news) {
    cx->recoverFromOutOfMemory();
    return nullptr;
  }

  FillChars(news.get(), chars, length);

  return JSLinearString::new_<NoGC>(cx, std::move(news), length);
}

JSLinearString* NewLatin1StringZ(JSContext* cx, UniqueChars chars) {
  size_t length = strlen(chars.get());
  UniqueLatin1Chars latin1(reinterpret_cast<Latin1Char*>(chars.release()));
  return NewString<CanGC>(cx, std::move(latin1), length);
}

template <AllowGC allowGC, typename CharT>
JSLinearString* NewStringCopyN(JSContext* cx, const CharT* s, size_t n) {
  if constexpr (std::is_same_v<CharT, char16_t>) {
    if (CanStoreCharsAsLatin1(s, n)) {
      return NewStringDeflated<allowGC>(cx, s, n);
    }
  }

  return NewStringCopyNDontDeflate<allowGC>(cx, s, n);
}

template JSLinearString* NewStringCopyN<CanGC>(JSContext* cx, const char16_t* s,
                                               size_t n);

template JSLinearString* NewStringCopyN<NoGC>(JSContext* cx, const char16_t* s,
                                              size_t n);

template JSLinearString* NewStringCopyN<CanGC>(JSContext* cx,
                                               const Latin1Char* s, size_t n);

template JSLinearString* NewStringCopyN<NoGC>(JSContext* cx,
                                              const Latin1Char* s, size_t n);

JSLinearString* NewStringFromLittleEndianNoGC(JSContext* cx,
                                              LittleEndianChars chars,
                                              size_t length) {
  if (JSLinearString* str = TryEmptyOrStaticString(cx, chars, length)) {
    return str;
  }

  if (CanStoreCharsAsLatin1(chars, length)) {
    return NewStringDeflatedFromLittleEndianNoGC(cx, chars, length);
  }

  return NewUndeflatedStringFromLittleEndianNoGC(cx, chars, length);
}

template <js::AllowGC allowGC>
JSLinearString* NewStringCopyUTF8N(JSContext* cx, const JS::UTF8Chars utf8) {
  JS::SmallestEncoding encoding = JS::FindSmallestEncoding(utf8);
  if (encoding == JS::SmallestEncoding::ASCII) {
    return NewStringCopyN<allowGC>(cx, utf8.begin().get(), utf8.length());
  }

  size_t length;
  if (encoding == JS::SmallestEncoding::Latin1) {
    UniqueLatin1Chars latin1(
        UTF8CharsToNewLatin1CharsZ(cx, utf8, &length, js::StringBufferArena)
            .get());
    if (!latin1) {
      return nullptr;
    }

    return NewString<allowGC>(cx, std::move(latin1), length);
  }

  MOZ_ASSERT(encoding == JS::SmallestEncoding::UTF16);

  UniqueTwoByteChars utf16(
      UTF8CharsToNewTwoByteCharsZ(cx, utf8, &length, js::StringBufferArena)
          .get());
  if (!utf16) {
    return nullptr;
  }

  return NewString<allowGC>(cx, std::move(utf16), length);
}

template JSLinearString* NewStringCopyUTF8N<CanGC>(JSContext* cx,
                                                   const JS::UTF8Chars utf8);

MOZ_ALWAYS_INLINE JSString* ExternalStringCache::lookup(const char16_t* chars,
                                                        size_t len) const {
  AutoCheckCannotGC nogc;

  for (size_t i = 0; i < NumEntries; i++) {
    JSString* str = entries_[i];
    if (!str || str->length() != len) {
      continue;
    }

    const char16_t* strChars = str->asLinear().nonInlineTwoByteChars(nogc);
    if (chars == strChars) {
      // Note that we don't need an incremental barrier here or below.
      // The cache is purged on GC so any string we get from the cache
      // must have been allocated after the GC started.
      return str;
    }

    // Compare the chars. Don't do this for long strings as it will be
    // faster to allocate a new external string.
    static const size_t MaxLengthForCharComparison = 100;
    if (len <= MaxLengthForCharComparison && ArrayEqual(chars, strChars, len)) {
      return str;
    }
  }

  return nullptr;
}

MOZ_ALWAYS_INLINE void ExternalStringCache::put(JSString* str) {
  MOZ_ASSERT(str->isExternal());

  for (size_t i = NumEntries - 1; i > 0; i--) {
    entries_[i] = entries_[i - 1];
  }

  entries_[0] = str;
}

JSString* NewMaybeExternalString(JSContext* cx, const char16_t* s, size_t n,
                                 const JSExternalStringCallbacks* callbacks,
                                 bool* allocatedExternal) {
  if (JSString* str = TryEmptyOrStaticString(cx, s, n)) {
    *allocatedExternal = false;
    return str;
  }

  if (JSThinInlineString::lengthFits<Latin1Char>(n) &&
      CanStoreCharsAsLatin1(s, n)) {
    *allocatedExternal = false;
    return NewInlineStringDeflated<AllowGC::CanGC>(
        cx, mozilla::Range<const char16_t>(s, n));
  }

  ExternalStringCache& cache = cx->zone()->externalStringCache();
  if (JSString* str = cache.lookup(s, n)) {
    *allocatedExternal = false;
    return str;
  }

  JSString* str = JSExternalString::new_(cx, s, n, callbacks);
  if (!str) {
    return nullptr;
  }

  *allocatedExternal = true;
  cache.put(str);
  return str;
}

} /* namespace js */

#if defined(DEBUG) || defined(JS_JITSPEW)
void JSExtensibleString::dumpRepresentation(js::GenericPrinter& out,
                                            int indent) const {
  dumpRepresentationHeader(out, "JSExtensibleString");
  indent += 2;

  out.printf("%*scapacity: %zu\n", indent, "", capacity());
  dumpRepresentationChars(out, indent);
}

void JSInlineString::dumpRepresentation(js::GenericPrinter& out,
                                        int indent) const {
  dumpRepresentationHeader(
      out, isFatInline() ? "JSFatInlineString" : "JSThinInlineString");
  indent += 2;

  dumpRepresentationChars(out, indent);
}

void JSLinearString::dumpRepresentation(js::GenericPrinter& out,
                                        int indent) const {
  dumpRepresentationHeader(out, "JSLinearString");
  indent += 2;

  dumpRepresentationChars(out, indent);
}
#endif

struct RepresentativeExternalString : public JSExternalStringCallbacks {
  void finalize(char16_t* chars) const override {
    // Constant chars, nothing to do.
  }
  size_t sizeOfBuffer(const char16_t* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    // This string's buffer is not heap-allocated, so its malloc size is 0.
    return 0;
  }
};

static const RepresentativeExternalString RepresentativeExternalStringCallbacks;

template <typename CheckString, typename CharT>
static bool FillWithRepresentatives(JSContext* cx, HandleArrayObject array,
                                    uint32_t* index, const CharT* chars,
                                    size_t len, size_t fatInlineMaxLength,
                                    const CheckString& check) {
  auto AppendString = [&check](JSContext* cx, HandleArrayObject array,
                               uint32_t* index, HandleString s) {
    MOZ_ASSERT(check(s));
    Unused << check;  // silence clang -Wunused-lambda-capture in opt builds
    RootedValue val(cx, StringValue(s));
    return JS_DefineElement(cx, array, (*index)++, val, 0);
  };

  MOZ_ASSERT(len > fatInlineMaxLength);

  // Normal atom.
  RootedString atom1(cx, AtomizeChars(cx, chars, len));
  if (!atom1 || !AppendString(cx, array, index, atom1)) {
    return false;
  }
  MOZ_ASSERT(atom1->isAtom());

  // Inline atom.
  RootedString atom2(cx, AtomizeChars(cx, chars, 2));
  if (!atom2 || !AppendString(cx, array, index, atom2)) {
    return false;
  }
  MOZ_ASSERT(atom2->isAtom());
  MOZ_ASSERT(atom2->isInline());

  // Fat inline atom.
  RootedString atom3(cx, AtomizeChars(cx, chars, fatInlineMaxLength));
  if (!atom3 || !AppendString(cx, array, index, atom3)) {
    return false;
  }
  MOZ_ASSERT(atom3->isAtom());
  MOZ_ASSERT(atom3->isFatInline());

  // Normal linear string.
  RootedString linear1(cx, NewStringCopyN<CanGC>(cx, chars, len));
  if (!linear1 || !AppendString(cx, array, index, linear1)) {
    return false;
  }
  MOZ_ASSERT(linear1->isLinear());

  // Inline string.
  RootedString linear2(cx, NewStringCopyN<CanGC>(cx, chars, 3));
  if (!linear2 || !AppendString(cx, array, index, linear2)) {
    return false;
  }
  MOZ_ASSERT(linear2->isLinear());
  MOZ_ASSERT(linear2->isInline());

  // Fat inline string.
  RootedString linear3(cx,
                       NewStringCopyN<CanGC>(cx, chars, fatInlineMaxLength));
  if (!linear3 || !AppendString(cx, array, index, linear3)) {
    return false;
  }
  MOZ_ASSERT(linear3->isLinear());
  MOZ_ASSERT(linear3->isFatInline());

  // Rope.
  RootedString rope(cx, ConcatStrings<CanGC>(cx, atom1, atom3));
  if (!rope || !AppendString(cx, array, index, rope)) {
    return false;
  }
  MOZ_ASSERT(rope->isRope());

  // Dependent.
  RootedString dep(cx, NewDependentString(cx, atom1, 0, len - 2));
  if (!dep || !AppendString(cx, array, index, dep)) {
    return false;
  }
  MOZ_ASSERT(dep->isDependent());

  // Extensible.
  RootedString temp1(cx, NewStringCopyN<CanGC>(cx, chars, len));
  if (!temp1) {
    return false;
  }
  RootedString extensible(cx, ConcatStrings<CanGC>(cx, temp1, atom3));
  if (!extensible || !extensible->ensureLinear(cx)) {
    return false;
  }
  if (!AppendString(cx, array, index, extensible)) {
    return false;
  }
  MOZ_ASSERT(extensible->isExtensible());

  // External. Note that we currently only support TwoByte external strings.
  RootedString external1(cx), external2(cx);
  if constexpr (std::is_same_v<CharT, char16_t>) {
    external1 = JS_NewExternalString(cx, (const char16_t*)chars, len,
                                     &RepresentativeExternalStringCallbacks);
    if (!external1 || !AppendString(cx, array, index, external1)) {
      return false;
    }
    MOZ_ASSERT(external1->isExternal());

    external2 = JS_NewExternalString(cx, (const char16_t*)chars, 2,
                                     &RepresentativeExternalStringCallbacks);
    if (!external2 || !AppendString(cx, array, index, external2)) {
      return false;
    }
    MOZ_ASSERT(external2->isExternal());
  }

  // Assert the strings still have the types we expect after creating the
  // other strings.

  MOZ_ASSERT(atom1->isAtom());
  MOZ_ASSERT(atom2->isAtom());
  MOZ_ASSERT(atom3->isAtom());
  MOZ_ASSERT(atom2->isInline());
  MOZ_ASSERT(atom3->isFatInline());

  MOZ_ASSERT(linear1->isLinear());
  MOZ_ASSERT(linear2->isLinear());
  MOZ_ASSERT(linear3->isLinear());
  MOZ_ASSERT(linear2->isInline());
  MOZ_ASSERT(linear3->isFatInline());

  MOZ_ASSERT(rope->isRope());
  MOZ_ASSERT(dep->isDependent());
  MOZ_ASSERT(extensible->isExtensible());
  MOZ_ASSERT_IF(external1, external1->isExternal());
  MOZ_ASSERT_IF(external2, external2->isExternal());
  return true;
}

/* static */
bool JSString::fillWithRepresentatives(JSContext* cx, HandleArrayObject array) {
  uint32_t index = 0;

  auto CheckTwoByte = [](JSString* str) { return str->hasTwoByteChars(); };
  auto CheckLatin1 = [](JSString* str) { return str->hasLatin1Chars(); };

  // Append TwoByte strings.
  static const char16_t twoByteChars[] =
      u"\u1234abc\0def\u5678ghijklmasdfa\0xyz0123456789";
  if (!FillWithRepresentatives(cx, array, &index, twoByteChars,
                               mozilla::ArrayLength(twoByteChars) - 1,
                               JSFatInlineString::MAX_LENGTH_TWO_BYTE,
                               CheckTwoByte)) {
    return false;
  }

  // Append Latin1 strings.
  static const Latin1Char latin1Chars[] = "abc\0defghijklmasdfa\0xyz0123456789";
  if (!FillWithRepresentatives(
          cx, array, &index, latin1Chars, mozilla::ArrayLength(latin1Chars) - 1,
          JSFatInlineString::MAX_LENGTH_LATIN1, CheckLatin1)) {
    return false;
  }

  // Now create forcibly-tenured versions of each of these string types. Note
  // that this is best-effort; if nursery strings are disabled, or we GC
  // midway through here, then we may end up with fewer nursery strings than
  // desired. Also, some types of strings are not nursery-allocatable, so
  // this will always produce some number of redundant strings.
  gc::AutoSuppressNurseryCellAlloc suppress(cx);

  // Append TwoByte strings.
  if (!FillWithRepresentatives(cx, array, &index, twoByteChars,
                               mozilla::ArrayLength(twoByteChars) - 1,
                               JSFatInlineString::MAX_LENGTH_TWO_BYTE,
                               CheckTwoByte)) {
    return false;
  }

  // Append Latin1 strings.
  if (!FillWithRepresentatives(
          cx, array, &index, latin1Chars, mozilla::ArrayLength(latin1Chars) - 1,
          JSFatInlineString::MAX_LENGTH_LATIN1, CheckLatin1)) {
    return false;
  }

  MOZ_ASSERT(index == 40);

  return true;
}

/*** Conversions ************************************************************/

UniqueChars js::EncodeLatin1(JSContext* cx, JSString* str) {
  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear) {
    return nullptr;
  }

  JS::AutoCheckCannotGC nogc;
  if (linear->hasTwoByteChars()) {
    Latin1CharsZ chars =
        JS::LossyTwoByteCharsToNewLatin1CharsZ(cx, linear->twoByteRange(nogc));
    return UniqueChars(chars.c_str());
  }

  size_t len = str->length();
  Latin1Char* buf = cx->pod_malloc<Latin1Char>(len + 1);
  if (!buf) {
    return nullptr;
  }

  FillChars(buf, linear->latin1Chars(nogc), len);
  buf[len] = '\0';

  return UniqueChars(reinterpret_cast<char*>(buf));
}

UniqueChars js::EncodeAscii(JSContext* cx, JSString* str) {
  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear) {
    return nullptr;
  }

  MOZ_ASSERT(StringIsAscii(linear));
  return EncodeLatin1(cx, linear);
}

UniqueChars js::IdToPrintableUTF8(JSContext* cx, HandleId id,
                                  IdToPrintableBehavior behavior) {
  // ToString(<symbol>) throws a TypeError, therefore require that callers
  // request source representation when |id| is a property key.
  MOZ_ASSERT_IF(behavior == IdToPrintableBehavior::IdIsIdentifier,
                JSID_IS_ATOM(id) &&
                    frontend::IsIdentifierNameOrPrivateName(JSID_TO_ATOM(id)));

  RootedValue v(cx, IdToValue(id));
  JSString* str;
  if (behavior == IdToPrintableBehavior::IdIsPropertyKey) {
    str = ValueToSource(cx, v);
  } else {
    str = ToString<CanGC>(cx, v);
  }
  if (!str) {
    return nullptr;
  }
  return StringToNewUTF8CharsZ(cx, *str);
}

template <AllowGC allowGC>
JSString* js::ToStringSlow(
    JSContext* cx, typename MaybeRooted<Value, allowGC>::HandleType arg) {
  /* As with ToObjectSlow, callers must verify that |arg| isn't a string. */
  MOZ_ASSERT(!arg.isString());

  Value v = arg;
  if (!v.isPrimitive()) {
    MOZ_ASSERT(!cx->isHelperThreadContext());
    if (!allowGC) {
      return nullptr;
    }
    RootedValue v2(cx, v);
    if (!ToPrimitive(cx, JSTYPE_STRING, &v2)) {
      return nullptr;
    }
    v = v2;
  }

  JSString* str;
  if (v.isString()) {
    str = v.toString();
  } else if (v.isInt32()) {
    str = Int32ToString<allowGC>(cx, v.toInt32());
  } else if (v.isDouble()) {
    str = NumberToString<allowGC>(cx, v.toDouble());
  } else if (v.isBoolean()) {
    str = BooleanToString(cx, v.toBoolean());
  } else if (v.isNull()) {
    str = cx->names().null;
  } else if (v.isSymbol()) {
    MOZ_ASSERT(!cx->isHelperThreadContext());
    if (allowGC) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_SYMBOL_TO_STRING);
    }
    return nullptr;
  } else if (v.isBigInt()) {
    if (!allowGC) {
      return nullptr;
    }
    RootedBigInt i(cx, v.toBigInt());
    str = BigInt::toString<CanGC>(cx, i, 10);
  } else {
    MOZ_ASSERT(v.isUndefined());
    str = cx->names().undefined;
  }
  return str;
}

template JSString* js::ToStringSlow<CanGC>(JSContext* cx, HandleValue arg);

template JSString* js::ToStringSlow<NoGC>(JSContext* cx, const Value& arg);

JS_PUBLIC_API JSString* js::ToStringSlow(JSContext* cx, HandleValue v) {
  return ToStringSlow<CanGC>(cx, v);
}
