/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StringType-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"

#include <algorithm>    // std::{all_of,copy_n,enable_if,is_const,move}
#include <type_traits>  // std::is_unsigned

#include "jsfriendapi.h"

#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "gc/Nursery.h"
#include "js/CharacterEncoding.h"
#include "js/StableStringChars.h"
#include "js/Symbol.h"
#include "js/UbiNode.h"
#include "util/StringBuffer.h"
#include "vm/GeckoProfiler.h"

#include "vm/GeckoProfiler-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"

using namespace js;

using mozilla::ArrayEqual;
using mozilla::AssertedCast;
using mozilla::IsAsciiDigit;
using mozilla::IsNegativeZero;
using mozilla::IsSame;
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

  // JSExternalString: Ask the embedding to tell us what's going on.  If it
  // doesn't want to say, don't count, the chars could be stored anywhere.
  if (isExternal()) {
    if (auto* cb =
            runtimeFromMainThread()->externalStringSizeofCallback.ref()) {
      // Our callback isn't supposed to cause GC.
      JS::AutoSuppressGCAnalysis nogc;
      return cb(this, mallocSizeOf);
    }
    return 0;
  }

  MOZ_ASSERT(isFlat());

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

  // JSAtom, JSUndependedString: measure the space for the chars.  For
  // JSUndependedString, there is no need to count the base string, for the
  // same reason as JSDependentString above.
  JSFlatString& flat = asFlat();
  return flat.hasLatin1Chars() ? mallocSizeOf(flat.rawLatin1Chars())
                               : mallocSizeOf(flat.rawTwoByteChars());
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
      dumpChars(linear->latin1Chars(nogc), length(), out);
    } else {
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
  } else if (isFlat()) {
    asFlat().dumpRepresentation(out, indent);
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
  if (flags & HAS_BASE_BIT) out.put(" HAS_BASE");
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
   * String length doesn't include the null char, so include it here before
   * doubling. Adding the null char after doubling would interact poorly with
   * round-up malloc schemes.
   */
  size_t numChars = length + 1;

  /*
   * Grow by 12.5% if the buffer is very large. Otherwise, round up to the
   * next power of 2. This is similar to what we do with arrays; see
   * JSObject::ensureDenseArrayElements.
   */
  static const size_t DOUBLING_MAX = 1024 * 1024;
  numChars = numChars > DOUBLING_MAX ? numChars + (numChars / 8)
                                     : RoundUpPow2(numChars);

  /* Like length, capacity does not include the null char, so take it out. */
  *capacity = numChars - 1;

  JS_STATIC_ASSERT(JSString::MAX_LENGTH * sizeof(CharT) < UINT32_MAX);
  *chars = str->zone()->pod_malloc<CharT>(numChars, js::StringBufferArena);
  return *chars != nullptr;
}

UniqueLatin1Chars JSRope::copyLatin1CharsZ(JSContext* maybecx,
                                           arena_id_t destArenaId) const {
  return copyCharsInternal<Latin1Char>(maybecx, true, destArenaId);
}

UniqueTwoByteChars JSRope::copyTwoByteCharsZ(JSContext* maybecx,
                                             arena_id_t destArenaId) const {
  return copyCharsInternal<char16_t>(maybecx, true, destArenaId);
}

UniqueLatin1Chars JSRope::copyLatin1Chars(JSContext* maybecx,
                                          arena_id_t destArenaId) const {
  return copyCharsInternal<Latin1Char>(maybecx, false, destArenaId);
}

UniqueTwoByteChars JSRope::copyTwoByteChars(JSContext* maybecx,
                                            arena_id_t destArenaId) const {
  return copyCharsInternal<char16_t>(maybecx, false, destArenaId);
}

template <typename CharT>
UniquePtr<CharT[], JS::FreePolicy> JSRope::copyCharsInternal(
    JSContext* maybecx, bool nullTerminate, arena_id_t destArenaId) const {
  // Left-leaning ropes are far more common than right-leaning ropes, so
  // perform a non-destructive traversal of the rope, right node first,
  // splatting each node's characters into a contiguous buffer.

  size_t n = length();

  UniquePtr<CharT[], JS::FreePolicy> out;
  if (maybecx) {
    out.reset(maybecx->pod_malloc<CharT>(n + 1, destArenaId));
  } else {
    out.reset(js_pod_arena_malloc<CharT>(destArenaId, n + 1));
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

  if (nullTerminate) {
    out[n] = 0;
  }

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
    for (size_t i = 0; i < len; i++) {
      MOZ_ASSERT(chars[i] <= JSString::MAX_LATIN1_CHAR);
      dest[i] = chars[i];
    }
  }
}

} /* namespace js */

template <JSRope::UsingBarrier b, typename CharT>
JSFlatString* JSRope::flattenInternal(JSContext* maybecx) {
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
        left.hasTwoByteChars() == IsSame<CharT, char16_t>::value) {
      wholeChars = const_cast<CharT*>(left.nonInlineChars<CharT>(nogc));
      wholeCapacity = capacity;

      // registerMallocedBuffer is fallible, so attempt it first before doing
      // anything irreversible.
      Nursery& nursery = runtimeFromMainThread()->gc.nursery();
      bool inTenured = !bufferIfNursery;
      if (!inTenured && left.isTenured()) {
        // tenured leftmost child is giving its chars buffer to the
        // nursery-allocated root node.
        if (!nursery.registerMallocedBuffer(wholeChars)) {
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
        nursery.removeMallocedBuffer(wholeChars);
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

      if (IsSame<CharT, char16_t>::value) {
        left.setLengthAndFlags(left_len, DEPENDENT_FLAGS);
      } else {
        left.setLengthAndFlags(left_len, DEPENDENT_FLAGS | LATIN1_CHARS_BIT);
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
    if (!nursery.registerMallocedBuffer(wholeChars)) {
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
    *pos = '\0';
    if (IsSame<CharT, char16_t>::value) {
      str->setLengthAndFlags(wholeLength, EXTENSIBLE_FLAGS);
    } else {
      str->setLengthAndFlags(wholeLength, EXTENSIBLE_FLAGS | LATIN1_CHARS_BIT);
    }
    str->setNonInlineChars(wholeChars);
    str->d.s.u3.capacity = wholeCapacity;

    if (str->isTenured()) {
      AddCellMemory(str, str->asFlat().allocSize(), MemoryUse::StringContents);
    }

    return &this->asFlat();
  }
  uintptr_t flattenData;
  uint32_t len = pos - str->nonInlineCharsRaw<CharT>();
  if (IsSame<CharT, char16_t>::value) {
    flattenData = str->unsetFlattenData(len, DEPENDENT_FLAGS);
  } else {
    flattenData =
        str->unsetFlattenData(len, DEPENDENT_FLAGS | LATIN1_CHARS_BIT);
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
JSFlatString* JSRope::flattenInternal(JSContext* maybecx) {
  if (hasTwoByteChars()) {
    return flattenInternal<b, char16_t>(maybecx);
  }
  return flattenInternal<b, Latin1Char>(maybecx);
}

JSFlatString* JSRope::flatten(JSContext* maybecx) {
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
      latin1Buf[wholeLength] = 0;
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
      twoByteBuf[wholeLength] = 0;
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
template <typename Dest, typename Source>
static void FillAndTerminate(Dest* dest, Source src, size_t length) {
  static_assert(sizeof(src[length]) <= sizeof(dest[length]),
                "source → destination conversion must not narrow");

  for (size_t i = 0; i < length; i++) {
    auto srcChar = src[i];
    static_assert(std::is_unsigned<decltype(srcChar)>::value &&
                      std::is_unsigned<Dest>::value,
                  "source/destination characters are unsigned for simplicity");
    *dest++ = srcChar;
  }

  *dest = '\0';
}

template <typename Dest, typename Src,
          typename = typename std::enable_if<!std::is_const<Src>::value>::type>
static void FillAndTerminate(Dest* dest, Src* src, size_t length) {
  FillAndTerminate(dest, const_cast<const Src*>(src), length);
}

/**
 * Copy |src[0..length]| to |dest[0..length]| when copying *does* narrow, but
 * the user guarantees every runtime |src[i]| value can be stored without change
 * of value in |dest[i]|.
 */
template <typename Dest, typename Source>
static void FillFromCompatibleAndTerminate(Dest* dest, Source src,
                                           size_t length) {
  static_assert(sizeof(src[length]) > sizeof(dest[length]),
                "source → destination conversion must be narrowing");

  for (size_t i = 0; i < length; i++) {
    auto srcChar = src[i];
    static_assert(std::is_unsigned<decltype(srcChar)>::value &&
                      std::is_unsigned<Dest>::value,
                  "source/destination characters are unsigned for simplicity");
    *dest++ = AssertedCast<Dest>(src[i]);
  }

  *dest = '\0';
}

template <typename Dest, typename Src,
          typename = typename std::enable_if<!std::is_const<Src>::value>::type>
static void FillFromCompatibleAndTerminate(Dest* dest, Src* src,
                                           size_t length) {
  FillFromCompatibleAndTerminate(dest, const_cast<const Src*>(src), length);
}

template <typename CharT>
JSFlatString* JSDependentString::undependInternal(JSContext* cx) {
  size_t n = length();
  auto s = cx->make_pod_array<CharT>(n + 1, js::StringBufferArena);
  if (!s) {
    return nullptr;
  }

  if (!isTenured()) {
    if (!cx->runtime()->gc.nursery().registerMallocedBuffer(s.get())) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  } else {
    AddCellMemory(this, (n + 1) * sizeof(CharT), MemoryUse::StringContents);
  }

  AutoCheckCannotGC nogc;
  FillAndTerminate(s.get(), nonInlineChars<CharT>(nogc), n);
  setNonInlineChars<CharT>(s.release());

  /*
   * Transform *this into an undepended string so 'base' will remain rooted
   * for the benefit of any other dependent string that depends on *this.
   */
  if (IsSame<CharT, Latin1Char>::value) {
    setLengthAndFlags(n, UNDEPENDED_FLAGS | LATIN1_CHARS_BIT);
  } else {
    setLengthAndFlags(n, UNDEPENDED_FLAGS);
  }

  return &this->asFlat();
}

JSFlatString* JSDependentString::undepend(JSContext* cx) {
  MOZ_ASSERT(JSString::isDependent());
  return hasLatin1Chars() ? undependInternal<Latin1Char>(cx)
                          : undependInternal<char16_t>(cx);
}

#if defined(DEBUG) || defined(JS_JITSPEW)
void JSDependentString::dumpRepresentation(js::GenericPrinter& out,
                                           int indent) const {
  dumpRepresentationHeader(out, "JSDependentString");
  indent += 2;

  if (mozilla::Maybe<size_t> offset = baseOffset()) {
    out.printf("%*soffset: %zu\n", indent, "", *offset);
  }

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
  auto containsOnlyAsciiCharacters = [](const auto* chars, size_t length) {
    return std::all_of(chars, chars + length,
                       [](auto c) { return mozilla::IsAscii(c); });
  };

  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars() ? containsOnlyAsciiCharacters(
                                     str->latin1Chars(nogc), str->length())
                               : containsOnlyAsciiCharacters(
                                     str->twoByteChars(nogc), str->length());
}

bool js::StringEqualsAscii(JSLinearString* str, const char* asciiBytes) {
  MOZ_ASSERT(JS::StringIsASCII(asciiBytes));

  size_t length = strlen(asciiBytes);
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
bool JSFlatString::isIndexSlow(const CharT* s, size_t length,
                               uint32_t* indexp) {
  CharT ch = *s;

  if (!IsAsciiDigit(ch)) {
    return false;
  }

  if (length > UINT32_CHAR_BUFFER_LENGTH) {
    return false;
  }

  /*
   * Make sure to account for the '\0' at the end of characters, dereferenced
   * in the loop below.
   */
  RangedPtr<const CharT> cp(s, length + 1);
  const RangedPtr<const CharT> end(s + length, s, length + 1);

  uint32_t index = AsciiDigitToNumber(*cp++);
  uint32_t oldIndex = 0;
  uint32_t c = 0;

  if (index != 0) {
    while (IsAsciiDigit(*cp)) {
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

template bool JSFlatString::isIndexSlow(const Latin1Char* s, size_t length,
                                        uint32_t* indexp);

template bool JSFlatString::isIndexSlow(const char16_t* s, size_t length,
                                        uint32_t* indexp);

/*
 * Set up some tools to make it easier to generate large tables. After constant
 * folding, for each n, Rn(0) is the comma-separated list R(0), R(1), ...,
 * R(2^n-1). Similary, Rn(k) (for any k and n) generates the list R(k), R(k+1),
 * ..., R(k+2^n-1). To use this, define R appropriately, then use Rn(0) (for
 * some value of n), then undefine R.
 */
#define R2(n) R(n), R((n) + (1 << 0)), R((n) + (2 << 0)), R((n) + (3 << 0))
#define R4(n) R2(n), R2((n) + (1 << 2)), R2((n) + (2 << 2)), R2((n) + (3 << 2))
#define R6(n) R4(n), R4((n) + (1 << 4)), R4((n) + (2 << 4)), R4((n) + (3 << 4))
#define R7(n) R6(n), R6((n) + (1 << 6))

/*
 * This is used when we generate our table of short strings, so the compiler is
 * happier if we use |c| as few times as possible.
 */
// clang-format off
#define FROM_SMALL_CHAR(c) Latin1Char((c) + ((c) < 10 ? '0' :      \
                                             (c) < 36 ? 'a' - 10 : \
                                             'A' - 36))
// clang-format on

/*
 * Declare length-2 strings. We only store strings where both characters are
 * alphanumeric. The lower 10 short chars are the numerals, the next 26 are
 * the lowercase letters, and the next 26 are the uppercase letters.
 */
// clang-format off
#define TO_SMALL_CHAR(c) ((c) >= '0' && (c) <= '9' ? (c) - '0' :              \
                          (c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 10 :         \
                          (c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 36 :         \
                          StaticStrings::INVALID_SMALL_CHAR)
// clang-format on

#define R TO_SMALL_CHAR
const StaticStrings::SmallChar StaticStrings::toSmallChar[] = {R7(0)};
#undef R

#undef R2
#undef R4
#undef R6
#undef R7

bool StaticStrings::init(JSContext* cx) {
  AutoAllocInAtomsZone az(cx);

  static_assert(UNIT_STATIC_LIMIT - 1 <= JSString::MAX_LATIN1_CHAR,
                "Unit strings must fit in Latin1Char.");

  using Latin1Range = mozilla::Range<const Latin1Char>;

  for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++) {
    Latin1Char buffer[] = {Latin1Char(i), '\0'};
    JSFlatString* s = NewInlineString<NoGC>(cx, Latin1Range(buffer, 1));
    if (!s) {
      return false;
    }
    HashNumber hash = mozilla::HashString(buffer, 1);
    unitStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom(hash);
  }

  for (uint32_t i = 0; i < NUM_SMALL_CHARS * NUM_SMALL_CHARS; i++) {
    Latin1Char buffer[] = {FROM_SMALL_CHAR(i >> 6), FROM_SMALL_CHAR(i & 0x3F),
                           '\0'};
    JSFlatString* s = NewInlineString<NoGC>(cx, Latin1Range(buffer, 2));
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
      size_t index = ((size_t)TO_SMALL_CHAR((i / 10) + '0') << 6) +
                     TO_SMALL_CHAR((i % 10) + '0');
      intStaticTable[i] = length2StaticTable[index];
    } else {
      Latin1Char buffer[] = {Latin1Char('0' + (i / 100)),
                             Latin1Char('0' + ((i / 10) % 10)),
                             Latin1Char('0' + (i % 10)), '\0'};
      JSFlatString* s = NewInlineString<NoGC>(cx, Latin1Range(buffer, 3));
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

template <typename CharT>
/* static */
bool StaticStrings::isStatic(const CharT* chars, size_t length) {
  switch (length) {
    case 1: {
      char16_t c = chars[0];
      return c < UNIT_STATIC_LIMIT;
    }
    case 2:
      return fitsInSmallChar(chars[0]) && fitsInSmallChar(chars[1]);
    case 3:
      if ('1' <= chars[0] && chars[0] <= '9' && '0' <= chars[1] &&
          chars[1] <= '9' && '0' <= chars[2] && chars[2] <= '9') {
        int i =
            (chars[0] - '0') * 100 + (chars[1] - '0') * 10 + (chars[2] - '0');

        return unsigned(i) < INT_STATIC_LIMIT;
      }
      return false;
    default:
      return false;
  }
}

/* static */
bool StaticStrings::isStatic(JSAtom* atom) {
  AutoCheckCannotGC nogc;
  return atom->hasLatin1Chars()
             ? isStatic(atom->latin1Chars(nogc), atom->length())
             : isStatic(atom->twoByteChars(nogc), atom->length());
}

bool AutoStableStringChars::init(JSContext* cx, JSString* s) {
  if (!s->isAtom()) {
    s->setNonDeduplicatable();
  }

  RootedLinearString linearString(cx, s->ensureLinear(cx));
  if (!linearString) {
    return false;
  }

  MOZ_ASSERT(state_ == Uninitialized);

  if (linearString->isExternal() && !linearString->ensureFlat(cx)) {
    return false;
  }

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

  if (linearString->isExternal() && !linearString->ensureFlat(cx)) {
    return false;
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
      InlineCapacity >= sizeof(JS::Latin1Char) *
                            (JSFatInlineString::MAX_LENGTH_LATIN1 + 1) &&
          InlineCapacity >=
              sizeof(char16_t) * (JSFatInlineString::MAX_LENGTH_TWO_BYTE + 1),
      "InlineCapacity too small to hold fat inline strings");

  static_assert((JSString::MAX_LENGTH &
                 mozilla::tl::MulOverflowMask<sizeof(T)>::value) == 0,
                "Size calculation can overflow");
  MOZ_ASSERT(count <= (JSString::MAX_LENGTH + 1));
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
  char16_t* chars = allocOwnChars<char16_t>(cx, linearString->length() + 1);
  if (!chars) {
    return false;
  }

  FillAndTerminate(chars, linearString->rawLatin1Chars(),
                   linearString->length());

  state_ = TwoByte;
  twoByteChars_ = chars;
  s_ = linearString;
  return true;
}

bool AutoStableStringChars::copyLatin1Chars(JSContext* cx,
                                            HandleLinearString linearString) {
  size_t length = linearString->length();
  JS::Latin1Char* chars = allocOwnChars<JS::Latin1Char>(cx, length + 1);
  if (!chars) {
    return false;
  }

  FillAndTerminate(chars, linearString->rawLatin1Chars(), length);

  state_ = Latin1;
  latin1Chars_ = chars;
  s_ = linearString;
  return true;
}

bool AutoStableStringChars::copyTwoByteChars(JSContext* cx,
                                             HandleLinearString linearString) {
  size_t length = linearString->length();
  char16_t* chars = allocOwnChars<char16_t>(cx, length + 1);
  if (!chars) {
    return false;
  }

  FillAndTerminate(chars, linearString->rawTwoByteChars(), length);

  state_ = TwoByte;
  twoByteChars_ = chars;
  s_ = linearString;
  return true;
}

JSFlatString* JSString::ensureFlat(JSContext* cx) {
  if (isFlat()) {
    return &asFlat();
  }
  if (isDependent()) {
    return asDependent().undepend(cx);
  }
  if (isRope()) {
    return asRope().flatten(cx);
  }
  return asExternal().ensureFlat(cx);
}

JSFlatString* JSExternalString::ensureFlat(JSContext* cx) {
  MOZ_ASSERT(hasTwoByteChars());

  size_t n = length();
  auto s = cx->make_pod_array<char16_t>(n + 1, js::StringBufferArena);
  if (!s) {
    return nullptr;
  }

  // Copy the chars before finalizing the string.
  {
    AutoCheckCannotGC nogc;
    FillAndTerminate(s.get(), nonInlineChars<char16_t>(nogc), n);
  }

  // Release the external chars.
  finalize(cx->runtime()->defaultFreeOp());

  MOZ_ASSERT(isTenured());
  AddCellMemory(this, (n + 1) * sizeof(char16_t), MemoryUse::StringContents);

  // Transform the string into a non-external, flat string. Note that the
  // resulting string will still be in an AllocKind::EXTERNAL_STRING arena,
  // but will no longer be an external string.
  setLengthAndFlags(n, INIT_FLAT_FLAGS);
  setNonInlineChars<char16_t>(s.release());

  return &this->asFlat();
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

  out.printf("%*sfinalizer: ((JSStringFinalizer*) %p)\n", indent, "",
             externalFinalizer());
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

static bool CanStoreCharsAsLatin1(const char16_t* s, size_t length) {
  for (const char16_t* end = s + length; s < end; ++s) {
    if (*s > JSString::MAX_LATIN1_CHAR) {
      return false;
    }
  }

  return true;
}

static bool CanStoreCharsAsLatin1(const Latin1Char* s, size_t length) {
  MOZ_CRASH("Shouldn't be called for Latin1 chars");
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
  FillFromCompatibleAndTerminate(storage, chars, len);
  return str;
}

template <AllowGC allowGC>
static JSFlatString* NewStringDeflated(JSContext* cx, const char16_t* s,
                                       size_t n) {
  if (JSFlatString* str = TryEmptyOrStaticString(cx, s, n)) {
    return str;
  }

  if (JSInlineString::lengthFits<Latin1Char>(n)) {
    return NewInlineStringDeflated<allowGC>(
        cx, mozilla::Range<const char16_t>(s, n));
  }

  auto news = cx->make_pod_array<Latin1Char>(n + 1, js::StringBufferArena);
  if (!news) {
    if (!allowGC) {
      cx->recoverFromOutOfMemory();
    }
    return nullptr;
  }

  MOZ_ASSERT(CanStoreCharsAsLatin1(s, n));
  FillFromCompatibleAndTerminate(news.get(), s, n);

  return JSFlatString::new_<allowGC>(cx, std::move(news), n);
}

template <AllowGC allowGC>
static JSFlatString* NewStringDeflated(JSContext* cx, const Latin1Char* s,
                                       size_t n) {
  MOZ_CRASH("Shouldn't be called for Latin1 chars");
}

static JSFlatString* NewStringDeflatedFromLittleEndianNoGC(
    JSContext* cx, LittleEndianChars chars, size_t length) {
  MOZ_ASSERT(CanStoreCharsAsLatin1(chars, length));

  if (JSInlineString::lengthFits<Latin1Char>(length)) {
    Latin1Char* storage;
    JSInlineString* str = AllocateInlineString<NoGC>(cx, length, &storage);
    if (!str) {
      return nullptr;
    }

    FillFromCompatibleAndTerminate(storage, chars, length);
    return str;
  }

  auto news = cx->make_pod_array<Latin1Char>(length + 1, js::StringBufferArena);
  if (!news) {
    cx->recoverFromOutOfMemory();
    return nullptr;
  }

  FillFromCompatibleAndTerminate(news.get(), chars, length);

  return JSFlatString::new_<NoGC>(cx, std::move(news), length);
}

template <typename CharT>
JSFlatString* js::NewStringDontDeflate(JSContext* cx,
                                       UniquePtr<CharT[], JS::FreePolicy> chars,
                                       size_t length) {
  if (JSFlatString* str = TryEmptyOrStaticString(cx, chars.get(), length)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(length)) {
    // |chars.get()| is safe because 1) |NewInlineString| necessarily *copies*,
    // and 2) |chars| frees its contents only when this function returns.
    return NewInlineString<CanGC>(
        cx, mozilla::Range<const CharT>(chars.get(), length));
  }

  return JSFlatString::new_<CanGC>(cx, std::move(chars), length);
}

template JSFlatString* js::NewStringDontDeflate(JSContext* cx,
                                                UniqueTwoByteChars chars,
                                                size_t length);

template JSFlatString* js::NewStringDontDeflate(JSContext* cx,
                                                UniqueLatin1Chars chars,
                                                size_t length);

template <typename CharT>
JSFlatString* js::NewString(JSContext* cx,
                            UniquePtr<CharT[], JS::FreePolicy> chars,
                            size_t length) {
  if (IsSame<CharT, char16_t>::value &&
      CanStoreCharsAsLatin1(chars.get(), length)) {
    // Deflating copies from |chars.get()| and lets |chars| be freed on return.
    return NewStringDeflated<CanGC>(cx, chars.get(), length);
  }

  return NewStringDontDeflate(cx, std::move(chars), length);
}

template JSFlatString* js::NewString(JSContext* cx, UniqueTwoByteChars chars,
                                     size_t length);

template JSFlatString* js::NewString(JSContext* cx, UniqueLatin1Chars chars,
                                     size_t length);

template <AllowGC allowGC, typename CharT>
JSFlatString* js::NewStringDontDeflate(JSContext* cx,
                                       UniquePtr<CharT[], JS::FreePolicy> chars,
                                       size_t length) {
  if (JSFlatString* str = TryEmptyOrStaticString(cx, chars.get(), length)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(length)) {
    return NewInlineString<allowGC>(
        cx, mozilla::Range<const CharT>(chars.get(), length));
  }

  return JSFlatString::new_<allowGC>(cx, std::move(chars), length);
}

template JSFlatString* js::NewStringDontDeflate<CanGC>(JSContext* cx,
                                                       UniqueTwoByteChars chars,
                                                       size_t length);

template JSFlatString* js::NewStringDontDeflate<NoGC>(JSContext* cx,
                                                      UniqueTwoByteChars chars,
                                                      size_t length);

template JSFlatString* js::NewStringDontDeflate<CanGC>(JSContext* cx,
                                                       UniqueLatin1Chars chars,
                                                       size_t length);

template JSFlatString* js::NewStringDontDeflate<NoGC>(JSContext* cx,
                                                      UniqueLatin1Chars chars,
                                                      size_t length);

template <AllowGC allowGC, typename CharT>
JSFlatString* js::NewString(JSContext* cx,
                            UniquePtr<CharT[], JS::FreePolicy> chars,
                            size_t length) {
  if (IsSame<CharT, char16_t>::value &&
      CanStoreCharsAsLatin1(chars.get(), length)) {
    return NewStringDeflated<allowGC>(cx, chars.get(), length);
  }

  return NewStringDontDeflate<allowGC>(cx, std::move(chars), length);
}

template JSFlatString* js::NewString<CanGC>(JSContext* cx,
                                            UniqueTwoByteChars chars,
                                            size_t length);

template JSFlatString* js::NewString<NoGC>(JSContext* cx,
                                           UniqueTwoByteChars chars,
                                           size_t length);

template JSFlatString* js::NewString<CanGC>(JSContext* cx,
                                            UniqueLatin1Chars chars,
                                            size_t length);

template JSFlatString* js::NewString<NoGC>(JSContext* cx,
                                           UniqueLatin1Chars chars,
                                           size_t length);

namespace js {

template <AllowGC allowGC, typename CharT>
JSFlatString* NewStringCopyNDontDeflate(JSContext* cx, const CharT* s,
                                        size_t n) {
  if (JSFlatString* str = TryEmptyOrStaticString(cx, s, n)) {
    return str;
  }

  if (JSInlineString::lengthFits<CharT>(n)) {
    return NewInlineString<allowGC>(cx, mozilla::Range<const CharT>(s, n));
  }

  auto news = cx->make_pod_array<CharT>(n + 1, js::StringBufferArena);
  if (!news) {
    if (!allowGC) {
      cx->recoverFromOutOfMemory();
    }
    return nullptr;
  }

  FillAndTerminate(news.get(), s, n);

  return JSFlatString::new_<allowGC>(cx, std::move(news), n);
}

template JSFlatString* NewStringCopyNDontDeflate<CanGC>(JSContext* cx,
                                                        const char16_t* s,
                                                        size_t n);

template JSFlatString* NewStringCopyNDontDeflate<NoGC>(JSContext* cx,
                                                       const char16_t* s,
                                                       size_t n);

template JSFlatString* NewStringCopyNDontDeflate<CanGC>(JSContext* cx,
                                                        const Latin1Char* s,
                                                        size_t n);

template JSFlatString* NewStringCopyNDontDeflate<NoGC>(JSContext* cx,
                                                       const Latin1Char* s,
                                                       size_t n);

static JSFlatString* NewUndeflatedStringFromLittleEndianNoGC(
    JSContext* cx, LittleEndianChars chars, size_t length) {
  if (JSInlineString::lengthFits<char16_t>(length)) {
    char16_t* storage;
    JSInlineString* str = AllocateInlineString<NoGC>(cx, length, &storage);
    if (!str) {
      return nullptr;
    }

    FillAndTerminate(storage, chars, length);
    return str;
  }

  auto news = cx->make_pod_array<char16_t>(length + 1, js::StringBufferArena);
  if (!news) {
    cx->recoverFromOutOfMemory();
    return nullptr;
  }

  FillAndTerminate(news.get(), chars, length);

  return JSFlatString::new_<NoGC>(cx, std::move(news), length);
}

JSFlatString* NewLatin1StringZ(JSContext* cx, UniqueChars chars) {
  size_t length = strlen(chars.get());
  UniqueLatin1Chars latin1(reinterpret_cast<Latin1Char*>(chars.release()));
  return NewString<CanGC>(cx, std::move(latin1), length);
}

template <AllowGC allowGC, typename CharT>
JSFlatString* NewStringCopyN(JSContext* cx, const CharT* s, size_t n) {
  if (IsSame<CharT, char16_t>::value && CanStoreCharsAsLatin1(s, n)) {
    return NewStringDeflated<allowGC>(cx, s, n);
  }

  return NewStringCopyNDontDeflate<allowGC>(cx, s, n);
}

template JSFlatString* NewStringCopyN<CanGC>(JSContext* cx, const char16_t* s,
                                             size_t n);

template JSFlatString* NewStringCopyN<NoGC>(JSContext* cx, const char16_t* s,
                                            size_t n);

template JSFlatString* NewStringCopyN<CanGC>(JSContext* cx, const Latin1Char* s,
                                             size_t n);

template JSFlatString* NewStringCopyN<NoGC>(JSContext* cx, const Latin1Char* s,
                                            size_t n);

JSFlatString* NewStringFromLittleEndianNoGC(JSContext* cx,
                                            LittleEndianChars chars,
                                            size_t length) {
  if (JSFlatString* str = TryEmptyOrStaticString(cx, chars, length)) {
    return str;
  }

  if (CanStoreCharsAsLatin1(chars, length)) {
    return NewStringDeflatedFromLittleEndianNoGC(cx, chars, length);
  }

  return NewUndeflatedStringFromLittleEndianNoGC(cx, chars, length);
}

template <js::AllowGC allowGC>
JSFlatString* NewStringCopyUTF8N(JSContext* cx, const JS::UTF8Chars utf8) {
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

template JSFlatString* NewStringCopyUTF8N<CanGC>(JSContext* cx,
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
                                 const JSStringFinalizer* fin,
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

  JSString* str = JSExternalString::new_(cx, s, n, fin);
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

void JSFlatString::dumpRepresentation(js::GenericPrinter& out,
                                      int indent) const {
  dumpRepresentationHeader(out, "JSFlatString");
  indent += 2;

  dumpRepresentationChars(out, indent);
}
#endif

static void FinalizeRepresentativeExternalString(const JSStringFinalizer* fin,
                                                 char16_t* chars);

static const JSStringFinalizer RepresentativeExternalStringFinalizer = {
    FinalizeRepresentativeExternalString};

static void FinalizeRepresentativeExternalString(const JSStringFinalizer* fin,
                                                 char16_t* chars) {
  // Constant chars, nothing to free.
  MOZ_ASSERT(fin == &RepresentativeExternalStringFinalizer);
}

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

  // Normal flat string.
  RootedString flat1(cx, NewStringCopyN<CanGC>(cx, chars, len));
  if (!flat1 || !AppendString(cx, array, index, flat1)) {
    return false;
  }
  MOZ_ASSERT(flat1->isFlat());

  // Inline string.
  RootedString flat2(cx, NewStringCopyN<CanGC>(cx, chars, 3));
  if (!flat2 || !AppendString(cx, array, index, flat2)) {
    return false;
  }
  MOZ_ASSERT(flat2->isFlat());
  MOZ_ASSERT(flat2->isInline());

  // Fat inline string.
  RootedString flat3(cx, NewStringCopyN<CanGC>(cx, chars, fatInlineMaxLength));
  if (!flat3 || !AppendString(cx, array, index, flat3)) {
    return false;
  }
  MOZ_ASSERT(flat3->isFlat());
  MOZ_ASSERT(flat3->isFatInline());

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

  // Undepended.
  RootedString undep(cx, NewDependentString(cx, atom1, 0, len - 3));
  if (!undep || !undep->ensureFlat(cx) ||
      !AppendString(cx, array, index, undep)) {
    return false;
  }
  MOZ_ASSERT(undep->isUndepended());

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
  if (IsSame<CharT, char16_t>::value) {
    external1 = JS_NewExternalString(cx, (const char16_t*)chars, len,
                                     &RepresentativeExternalStringFinalizer);
    if (!external1 || !AppendString(cx, array, index, external1)) {
      return false;
    }
    MOZ_ASSERT(external1->isExternal());

    external2 = JS_NewExternalString(cx, (const char16_t*)chars, 2,
                                     &RepresentativeExternalStringFinalizer);
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

  MOZ_ASSERT(flat1->isFlat());
  MOZ_ASSERT(flat2->isFlat());
  MOZ_ASSERT(flat3->isFlat());
  MOZ_ASSERT(flat2->isInline());
  MOZ_ASSERT(flat3->isFatInline());

  MOZ_ASSERT(rope->isRope());
  MOZ_ASSERT(dep->isDependent());
  MOZ_ASSERT(undep->isUndepended());
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

  MOZ_ASSERT(index == 44);

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

  FillAndTerminate(buf, linear->latin1Chars(nogc), len);
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

/*
 * Convert a JSString to its source expression; returns null after reporting an
 * error, otherwise returns a new string reference. No Handle needed since the
 * input is dead after the GC.
 */
static JSString* StringToSource(JSContext* cx, JSString* str) {
  UniqueChars chars = QuoteString(cx, str, '"');
  if (!chars) {
    return nullptr;
  }
  return NewStringCopyZ<CanGC>(cx, chars.get());
}

static JSString* SymbolToSource(JSContext* cx, Symbol* symbol) {
  RootedString desc(cx, symbol->description());
  SymbolCode code = symbol->code();
  if (code != SymbolCode::InSymbolRegistry &&
      code != SymbolCode::UniqueSymbol) {
    // Well-known symbol.
    MOZ_ASSERT(uint32_t(code) < JS::WellKnownSymbolLimit);
    return desc;
  }

  JSStringBuilder buf(cx);
  if (code == SymbolCode::InSymbolRegistry ? !buf.append("Symbol.for(")
                                           : !buf.append("Symbol(")) {
    return nullptr;
  }
  if (desc) {
    UniqueChars quoted = QuoteString(cx, desc, '"');
    if (!quoted || !buf.append(quoted.get(), strlen(quoted.get()))) {
      return nullptr;
    }
  }
  if (!buf.append(')')) {
    return nullptr;
  }
  return buf.finishString();
}

JSString* js::ValueToSource(JSContext* cx, HandleValue v) {
  if (!CheckRecursionLimit(cx)) {
    return nullptr;
  }
  cx->check(v);

  if (v.isUndefined()) {
    return cx->names().void0;
  }
  if (v.isString()) {
    return StringToSource(cx, v.toString());
  }
  if (v.isSymbol()) {
    return SymbolToSource(cx, v.toSymbol());
  }
  if (v.isPrimitive()) {
    /* Special case to preserve negative zero, _contra_ toString. */
    if (v.isDouble() && IsNegativeZero(v.toDouble())) {
      static const Latin1Char negativeZero[] = {'-', '0'};

      return NewStringCopyN<CanGC>(cx, negativeZero,
                                   mozilla::ArrayLength(negativeZero));
    }
    return ToString<CanGC>(cx, v);
  }

  RootedValue fval(cx);
  RootedObject obj(cx, &v.toObject());
  if (!GetProperty(cx, obj, obj, cx->names().toSource, &fval)) {
    return nullptr;
  }
  if (IsCallable(fval)) {
    RootedValue v(cx);
    if (!js::Call(cx, fval, obj, &v)) {
      return nullptr;
    }

    return ToString<CanGC>(cx, v);
  }

  return ObjectToSource(cx, obj);
}
