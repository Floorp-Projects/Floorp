/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Xdr.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Utf8.h"

#include <algorithm>  // std::transform
#include <string.h>
#include <type_traits>  // std::is_same
#include <utility>      // std::move

#include "jsapi.h"

#include "debugger/DebugAPI.h"
#include "js/BuildId.h"  // JS::BuildIdCharVector
#include "vm/EnvironmentObject.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"
#include "vm/TraceLogging.h"

using namespace js;

using mozilla::ArrayEqual;
using mozilla::Utf8Unit;

#ifdef DEBUG
bool XDRCoderBase::validateResultCode(JSContext* cx,
                                      JS::TranscodeResult code) const {
  // NOTE: This function is called to verify that we do not have a pending
  // exception on the JSContext at the same time as a TranscodeResult failure.
  if (cx->isHelperThreadContext()) {
    return true;
  }
  return cx->isExceptionPending() == bool(code == JS::TranscodeResult_Throw);
}
#endif

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(char* chars, size_t nchars) {
  return codeBytes(chars, nchars);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(Latin1Char* chars, size_t nchars) {
  static_assert(sizeof(Latin1Char) == 1,
                "Latin1Char must be 1 byte for nchars below to be the "
                "proper count of bytes");
  static_assert(std::is_same_v<Latin1Char, unsigned char>,
                "Latin1Char must be unsigned char to C++-safely reinterpret "
                "the bytes generically copied below as Latin1Char");
  return codeBytes(chars, nchars);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(Utf8Unit* units, size_t count) {
  if (count == 0) {
    return Ok();
  }

  if (mode == XDR_ENCODE) {
    uint8_t* ptr = buf->write(count);
    if (!ptr) {
      return fail(JS::TranscodeResult_Throw);
    }

    std::transform(units, units + count, ptr,
                   [](const Utf8Unit& unit) { return unit.toUint8(); });
  } else {
    const uint8_t* ptr = buf->read(count);
    if (!ptr) {
      return fail(JS::TranscodeResult_Failure_BadDecode);
    }

    std::transform(ptr, ptr + count, units,
                   [](const uint8_t& value) { return Utf8Unit(value); });
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(char16_t* chars, size_t nchars) {
  if (nchars == 0) {
    return Ok();
  }

  size_t nbytes = nchars * sizeof(char16_t);
  if (mode == XDR_ENCODE) {
    uint8_t* ptr = buf->write(nbytes);
    if (!ptr) {
      return fail(JS::TranscodeResult_Throw);
    }

    // |mozilla::NativeEndian| correctly handles writing into unaligned |ptr|.
    mozilla::NativeEndian::copyAndSwapToLittleEndian(ptr, chars, nchars);
  } else {
    const uint8_t* ptr = buf->read(nbytes);
    if (!ptr) {
      return fail(JS::TranscodeResult_Failure_BadDecode);
    }

    // |mozilla::NativeEndian| correctly handles reading from unaligned |ptr|.
    mozilla::NativeEndian::copyAndSwapFromLittleEndian(chars, ptr, nchars);
  }
  return Ok();
}

template <XDRMode mode, typename CharT>
static XDRResult XDRCodeCharsZ(XDRState<mode>* xdr,
                               XDRTranscodeString<CharT>& buffer) {
  MOZ_ASSERT_IF(mode == XDR_ENCODE, !buffer.empty());
  MOZ_ASSERT_IF(mode == XDR_DECODE, buffer.empty());

  using OwnedString = js::UniquePtr<CharT[], JS::FreePolicy>;
  OwnedString owned;

  static_assert(JSString::MAX_LENGTH <= INT32_MAX,
                "String length must fit in int32_t");

  uint32_t length = 0;
  CharT* chars = nullptr;

  if (mode == XDR_ENCODE) {
    chars = const_cast<CharT*>(buffer.template ref<const CharT*>());

    // Set a reasonable limit on string length.
    size_t lengthSizeT = std::char_traits<CharT>::length(chars);
    if (lengthSizeT > JSString::MAX_LENGTH) {
      ReportAllocationOverflow(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    length = static_cast<uint32_t>(lengthSizeT);
  }
  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_DECODE) {
    owned = xdr->cx()->template make_pod_array<CharT>(length + 1);
    if (!owned) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    chars = owned.get();
  }

  MOZ_TRY(xdr->codeChars(chars, length));
  if (mode == XDR_DECODE) {
    // Null-terminate and transfer ownership to caller.
    owned[length] = '\0';
    buffer.template construct<OwnedString>(std::move(owned));
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeCharsZ(XDRTranscodeString<char>& buffer) {
  return XDRCodeCharsZ(this, buffer);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeCharsZ(XDRTranscodeString<char16_t>& buffer) {
  return XDRCodeCharsZ(this, buffer);
}

template <XDRMode mode>
static XDRResult VersionCheck(XDRState<mode>* xdr) {
  JS::BuildIdCharVector buildId;
  uint8_t profileSize = 0;
  MOZ_ASSERT(GetBuildId);
  if (!GetBuildId(&buildId)) {
    ReportOutOfMemory(xdr->cx());
    return xdr->fail(JS::TranscodeResult_Throw);
  }
  MOZ_ASSERT(!buildId.empty());

  uint32_t buildIdLength;
  if (mode == XDR_ENCODE) {
    buildIdLength = buildId.length();
    profileSize = sizeof(uintptr_t);
  }

  MOZ_TRY(xdr->codeUint32(&buildIdLength));
  MOZ_TRY(xdr->codeUint8(&profileSize));

  if (mode == XDR_DECODE && buildIdLength != buildId.length()) {
    return xdr->fail(JS::TranscodeResult_Failure_BadBuildId);
  }

  if (mode == XDR_ENCODE) {
    MOZ_TRY(xdr->codeBytes(buildId.begin(), buildIdLength));
  } else {
    JS::BuildIdCharVector decodedBuildId;

    // Checks to make sure we are not decoding profiles of a
    // different size than what was encoded.
    if (profileSize != sizeof(uintptr_t)) {
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }

    // buildIdLength is already checked against the length of current
    // buildId.
    if (!decodedBuildId.resize(buildIdLength)) {
      ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    MOZ_TRY(xdr->codeBytes(decodedBuildId.begin(), buildIdLength));

    // We do not provide binary compatibility with older scripts.
    if (!ArrayEqual(decodedBuildId.begin(), buildId.begin(), buildIdLength)) {
      return xdr->fail(JS::TranscodeResult_Failure_BadBuildId);
    }
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeModuleObject(MutableHandleModuleObject modp) {
#ifdef DEBUG
  auto sanityCheck = mozilla::MakeScopeExit(
      [&] { MOZ_ASSERT(validateResultCode(cx(), resultCode())); });
#endif
  if (mode == XDR_DECODE) {
    modp.set(nullptr);
  } else {
    MOZ_ASSERT(modp->status() < MODULE_STATUS_INSTANTIATING);
  }

  MOZ_TRY(XDRModuleObject(this, modp));
  return Ok();
}

template <XDRMode mode>
static XDRResult XDRAtomCount(XDRState<mode>* xdr, uint32_t* atomCount) {
  return xdr->codeUint32(atomCount);
}

template <XDRMode mode>
static XDRResult AtomTable(XDRState<mode>* xdr) {
  uint8_t atomHeader = false;
  if (mode == XDR_ENCODE) {
    if (xdr->hasAtomMap()) {
      atomHeader = true;
    }
  }

  MOZ_TRY(xdr->codeUint8(&atomHeader));

  // If we are incrementally encoding, the atom table will be built up over the
  // course of the encoding. In XDRIncrementalEncoder::linearize, we will write
  // the number of atoms into the header, then append the completed atom table.
  // If we are decoding, then we read the length and decode the atom table now.
  if (atomHeader && mode == XDR_DECODE) {
    uint32_t atomCount;
    MOZ_TRY(XDRAtomCount(xdr, &atomCount));
    MOZ_ASSERT(!xdr->hasAtomTable());

    for (uint32_t i = 0; i < atomCount; i++) {
      RootedAtom atom(xdr->cx());
      MOZ_TRY(XDRAtom(xdr, &atom));
      if (!xdr->atomTable().append(atom)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
    xdr->finishAtomTable();
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeFunction(MutableHandleFunction funp,
                                       HandleScriptSourceObject sourceObject) {
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx());
  TraceLoggerTextId event = mode == XDR_DECODE ? TraceLogger_DecodeFunction
                                               : TraceLogger_EncodeFunction;
  AutoTraceLog tl(logger, event);

#ifdef DEBUG
  auto sanityCheck = mozilla::MakeScopeExit(
      [&] { MOZ_ASSERT(validateResultCode(cx(), resultCode())); });
#endif
  auto guard = mozilla::MakeScopeExit([&] { funp.set(nullptr); });
  RootedScope scope(cx(), &cx()->global()->emptyGlobalScope());
  if (mode == XDR_DECODE) {
    MOZ_ASSERT(!sourceObject);
    funp.set(nullptr);
  } else if (getTreeKey(funp) != AutoXDRTree::noKey) {
    MOZ_ASSERT(sourceObject);
    scope = funp->enclosingScope();
  } else {
    MOZ_ASSERT(!sourceObject);
    MOZ_ASSERT(funp->enclosingScope()->is<GlobalScope>());
  }

  MOZ_TRY(VersionCheck(this));
  MOZ_TRY(XDRInterpretedFunction(this, scope, sourceObject, funp));

  guard.release();
  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeScript(MutableHandleScript scriptp) {
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx());
  TraceLoggerTextId event =
      mode == XDR_DECODE ? TraceLogger_DecodeScript : TraceLogger_EncodeScript;
  AutoTraceLog tl(logger, event);

#ifdef DEBUG
  auto sanityCheck = mozilla::MakeScopeExit(
      [&] { MOZ_ASSERT(validateResultCode(cx(), resultCode())); });
#endif
  auto guard = mozilla::MakeScopeExit([&] { scriptp.set(nullptr); });

  AutoXDRTree scriptTree(this, getTopLevelTreeKey());

  if (mode == XDR_DECODE) {
    scriptp.set(nullptr);
  } else {
    MOZ_ASSERT(!scriptp->enclosingScope());
  }

  // Only write to separate header buffer if we are incrementally encoding.
  bool useHeader = this->hasAtomMap();
  if (useHeader) {
    switchToHeaderBuf();
  }
  MOZ_TRY(VersionCheck(this));
  MOZ_TRY(AtomTable(this));
  if (useHeader) {
    switchToMainBuf();
  }
  MOZ_ASSERT(isMainBuf());
  MOZ_TRY(XDRScript(this, nullptr, nullptr, nullptr, scriptp));

  guard.release();
  return Ok();
}

template class js::XDRState<XDR_ENCODE>;
template class js::XDRState<XDR_DECODE>;

AutoXDRTree::AutoXDRTree(XDRCoderBase* xdr, AutoXDRTree::Key key)
    : key_(key), parent_(this), xdr_(xdr) {
  if (key_ != AutoXDRTree::noKey) {
    xdr->createOrReplaceSubTree(this);
  }
}

AutoXDRTree::~AutoXDRTree() {
  if (key_ != AutoXDRTree::noKey) {
    xdr_->endSubTree();
  }
}

constexpr AutoXDRTree::Key AutoXDRTree::noKey;
constexpr AutoXDRTree::Key AutoXDRTree::noSubTree;
constexpr AutoXDRTree::Key AutoXDRTree::topLevel;

class XDRIncrementalEncoder::DepthFirstSliceIterator {
 public:
  DepthFirstSliceIterator(JSContext* cx, const SlicesTree& tree)
      : stack_(cx), tree_(tree) {}

  template <typename SliceFun>
  bool iterate(SliceFun&& f) {
    MOZ_ASSERT(stack_.empty());

    if (!appendChildrenForKey(AutoXDRTree::topLevel)) {
      return false;
    }

    while (!done()) {
      SlicesNode::ConstRange& iter = next();
      Slice slice = iter.popCopyFront();
      // These fields have different meaning, but they should be
      // correlated if the tree is well formatted.
      MOZ_ASSERT_IF(slice.child == AutoXDRTree::noSubTree, iter.empty());
      if (iter.empty()) {
        pop();
      }

      if (!f(slice)) {
        return false;
      }

      // If we are at the end, go back to the parent script.
      if (slice.child == AutoXDRTree::noSubTree) {
        continue;
      }

      if (!appendChildrenForKey(slice.child)) {
        return false;
      }
    }

    return true;
  }

 private:
  bool done() const { return stack_.empty(); }
  SlicesNode::ConstRange& next() { return stack_.back(); }
  void pop() { stack_.popBack(); }

  MOZ_MUST_USE bool appendChildrenForKey(AutoXDRTree::Key key) {
    MOZ_ASSERT(key != AutoXDRTree::noSubTree);

    SlicesTree::Ptr p = tree_.lookup(key);
    MOZ_ASSERT(p);
    return stack_.append(((const SlicesNode&)p->value()).all());
  }

  Vector<SlicesNode::ConstRange> stack_;
  const SlicesTree& tree_;
};

AutoXDRTree::Key XDRIncrementalEncoder::getTopLevelTreeKey() const {
  return AutoXDRTree::topLevel;
}

AutoXDRTree::Key XDRIncrementalEncoder::getTreeKey(JSFunction* fun) const {
  if (fun->hasBaseScript()) {
    static_assert(sizeof(fun->baseScript()->sourceStart()) == 4 &&
                      sizeof(fun->baseScript()->sourceEnd()) == 4,
                  "AutoXDRTree key requires BaseScript positions to be uint32");
    return uint64_t(fun->baseScript()->sourceStart()) << 32 |
           fun->baseScript()->sourceEnd();
  }

  return AutoXDRTree::noKey;
}

void XDRIncrementalEncoder::createOrReplaceSubTree(AutoXDRTree* child) {
  AutoXDRTree* parent = scope_;
  child->parent_ = parent;
  scope_ = child;
  if (oom_) {
    return;
  }

  size_t cursor = buf->cursor();

  // End the parent slice here, set the key to the child.
  if (parent) {
    Slice& last = node_->back();
    last.sliceLength = cursor - last.sliceBegin;
    last.child = child->key_;
    MOZ_ASSERT_IF(uint32_t(parent->key_) != 0,
                  uint32_t(parent->key_ >> 32) <= uint32_t(child->key_ >> 32) &&
                      uint32_t(child->key_) <= uint32_t(parent->key_));
  }

  // Create or replace the part with what is going to be encoded next.
  SlicesTree::AddPtr p = tree_.lookupForAdd(child->key_);
  SlicesNode tmp;
  if (!p) {
    // Create a new sub-tree node.
    if (!tree_.add(p, child->key_, std::move(tmp))) {
      oom_ = true;
      return;
    }
  } else {
    // Replace an exisiting sub-tree.
    p->value() = std::move(tmp);
  }
  node_ = &p->value();

  // Add content to the root of the new sub-tree,
  // i-e an empty slice with no children.
  if (!node_->append(Slice{cursor, 0, AutoXDRTree::noSubTree})) {
    MOZ_CRASH("SlicesNode have a reserved space of 1.");
  }
}

void XDRIncrementalEncoder::endSubTree() {
  AutoXDRTree* child = scope_;
  AutoXDRTree* parent = child->parent_;
  scope_ = parent;
  if (oom_) {
    return;
  }

  size_t cursor = buf->cursor();

  // End the child sub-tree.
  Slice& last = node_->back();
  last.sliceLength = cursor - last.sliceBegin;
  MOZ_ASSERT(last.child == AutoXDRTree::noSubTree);

  // Stop at the top-level.
  if (!parent) {
    node_ = nullptr;
    return;
  }

  // Restore the parent node.
  SlicesTree::Ptr p = tree_.lookup(parent->key_);
  node_ = &p->value();

  // Append the new slice in the parent node.
  if (!node_->append(Slice{cursor, 0, AutoXDRTree::noSubTree})) {
    oom_ = true;
    return;
  }
}

XDRResult XDRIncrementalEncoder::linearize(JS::TranscodeBuffer& buffer) {
  if (oom_) {
    ReportOutOfMemory(cx());
    return fail(JS::TranscodeResult_Throw);
  }

  // Do not linearize while we are currently adding bytes.
  MOZ_ASSERT(scope_ == nullptr);

  // Write the size of the atom buffer to the header.
  switchToHeaderBuf();
  MOZ_TRY(XDRAtomCount(this, &natoms_));
  switchToMainBuf();

  // Visit the tree parts in a depth first order to linearize the bits.
  // Calculate the total length first so we don't incur repeated copying
  // and zeroing of memory for large trees.
  DepthFirstSliceIterator dfs(cx(), tree_);

  size_t totalLength = buffer.length() + header_.length() + atoms_.length();
  auto sliceCounter = [&](const Slice& slice) -> bool {
    totalLength += slice.sliceLength;
    return true;
  };

  if (!dfs.iterate(sliceCounter)) {
    ReportOutOfMemory(cx());
    return fail(JS::TranscodeResult_Throw);
  };

  if (!buffer.reserve(totalLength)) {
    ReportOutOfMemory(cx());
    return fail(JS::TranscodeResult_Throw);
  }

  buffer.infallibleAppend(header_.begin(), header_.length());
  buffer.infallibleAppend(atoms_.begin(), atoms_.length());

  auto sliceCopier = [&](const Slice& slice) -> bool {
    // Copy the bytes associated with the current slice to the transcode
    // buffer which would be serialized.
    MOZ_ASSERT(slice.sliceBegin <= slices_.length());
    MOZ_ASSERT(slice.sliceBegin + slice.sliceLength <= slices_.length());

    buffer.infallibleAppend(slices_.begin() + slice.sliceBegin,
                            slice.sliceLength);
    return true;
  };

  if (!dfs.iterate(sliceCopier)) {
    ReportOutOfMemory(cx());
    return fail(JS::TranscodeResult_Throw);
  }

  tree_.clearAndCompact();
  slices_.clearAndFree();
  return Ok();
}

void XDRDecoder::trace(JSTracer* trc) { atomTable_.trace(trc); }

void XDRIncrementalEncoder::trace(JSTracer* trc) { atomMap_.trace(trc); }
