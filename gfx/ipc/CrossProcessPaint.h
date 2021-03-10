/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_CrossProcessPaint_h_
#define _include_mozilla_gfx_ipc_CrossProcessPaint_h_

#include "nsISupportsImpl.h"

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/ByteBuf.h"
#include "nsColor.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashtable.h"

class nsIDocShell;

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {

namespace dom {
class DOMRect;
class Promise;
class WindowGlobalParent;
}  // namespace dom

namespace gfx {

class CrossProcessPaint;

enum class CrossProcessPaintFlags {
  None = 0,
  DrawView = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CrossProcessPaintFlags)

/**
 * A fragment of a paint of a cross process document tree.
 */
class PaintFragment final {
 public:
  /// Initializes an empty PaintFragment
  PaintFragment() = default;

  /**
   * Creates a paint fragment by recording the draw commands and dependent tabs
   * for a BrowsingContext.
   *
   * @param aBrowsingContext The frame to record.
   * @param aRect The rectangle relative to the viewport to use. If no
   *   rectangle is specified, then the whole viewport will be used.
   * @param aScale The coordinate scale to use. The size of the resolved
   *   surface will be `aRect.Size() * aScale`, with aScale clamped to
   *   at least kMinPaintScale.
   * @param aBackgroundColor The background color to use.
   *
   * @return A paint fragment. The paint fragment may be `empty` if rendering
   *         was unable to be accomplished for some reason.
   */
  static PaintFragment Record(dom::BrowsingContext* aBc,
                              const Maybe<IntRect>& aRect, float aScale,
                              nscolor aBackgroundColor,
                              CrossProcessPaintFlags aFlags);

  /// Returns whether this paint fragment contains a valid recording.
  bool IsEmpty() const;

  PaintFragment(PaintFragment&&) = default;
  PaintFragment& operator=(PaintFragment&&) = default;

 protected:
  friend struct mozilla::ipc::IPDLParamTraits<PaintFragment>;
  friend CrossProcessPaint;

  typedef mozilla::ipc::ByteBuf ByteBuf;

  PaintFragment(IntSize, ByteBuf&&, nsTHashtable<nsUint64HashKey>&&);

  IntSize mSize;
  ByteBuf mRecording;
  nsTHashtable<nsUint64HashKey> mDependencies;
};

/**
 * An object for painting a cross process document tree.
 */
class CrossProcessPaint final {
  NS_INLINE_DECL_REFCOUNTING(CrossProcessPaint);

 public:
  typedef nsRefPtrHashtable<nsUint64HashKey, RecordedDependentSurface>
      ResolvedFragmentMap;
  typedef MozPromise<ResolvedFragmentMap, nsresult, true> ResolvePromise;
  /**
   * Begin an asynchronous paint of a cross process document tree starting at
   * a WindowGlobalParent. A maybe-async paint for the root WGP will be done,
   * then async paints will be recursively queued for remote subframes. Once
   * all subframes have been recorded, the final image will be resolved, and
   * the promise will be resolved with a dom::ImageBitmap.
   *
   * @param aRoot The WindowGlobalParent to paint.
   * @param aRect The rectangle relative to the viewport to use, or null to
   *   render the whole viewport.
   * @param aScale The coordinate scale to use. The size of the resolved
   *   surface will be `aRect.Size() * aScale`, with aScale clamped to
   *   at least kMinPaintScale. See the implementation for the current
   *   minimum value.
   * @param aBackgroundColor The background color to use.
   * @param aPromise The promise to resolve with a dom::ImageBitmap.
   *
   * @returns Whether the paint was able to be initiated or not.
   */
  static bool Start(dom::WindowGlobalParent* aRoot, const dom::DOMRect* aRect,
                    float aScale, nscolor aBackgroundColor,
                    CrossProcessPaintFlags aFlags, dom::Promise* aPromise);

  static RefPtr<ResolvePromise> Start(
      nsTHashtable<nsUint64HashKey>&& aDependencies);

  void ReceiveFragment(dom::WindowGlobalParent* aWGP,
                       PaintFragment&& aFragment);
  void LostFragment(dom::WindowGlobalParent* aWGP);

 private:
  typedef nsTHashMap<nsUint64HashKey, PaintFragment> ReceivedFragmentMap;

  CrossProcessPaint(float aScale, dom::TabId aRoot);
  ~CrossProcessPaint();

  void QueueDependencies(const nsTHashtable<nsUint64HashKey>& aDependencies);

  void QueuePaint(
      dom::WindowGlobalParent* aWGP, const Maybe<IntRect>& aRect,
      nscolor aBackgroundColor = NS_RGBA(0, 0, 0, 0),
      CrossProcessPaintFlags aFlags = CrossProcessPaintFlags::DrawView);

  /// Clear the state of this paint so that it cannot be resolved or receive
  /// any paint fragments.
  void Clear(nsresult aStatus);

  /// Returns if this paint has been cleared.
  bool IsCleared() const;

  /// Resolves the paint fragments if we have none pending and resolves the
  /// promise.
  void MaybeResolve();
  nsresult ResolveInternal(dom::TabId aTabId, ResolvedFragmentMap* aResolved);

  RefPtr<ResolvePromise> Init() {
    MOZ_ASSERT(mPromise.IsEmpty());
    return mPromise.Ensure(__func__);
  }

  MozPromiseHolder<ResolvePromise> mPromise;
  dom::TabId mRoot;
  float mScale;
  uint32_t mPendingFragments;
  ReceivedFragmentMap mReceivedFragments;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_CrossProcessPaint_h_
