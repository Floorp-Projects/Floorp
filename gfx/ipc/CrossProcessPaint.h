/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_CrossProcessPaint_h_
#define _include_mozilla_gfx_ipc_CrossProcessPaint_h_

#include "nsISupportsImpl.h"

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ipc/ByteBuf.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashtable.h"

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace gfx {

class CrossProcessPaint;

/**
 * A fragment of a paint of a cross process document tree.
 */
class PaintFragment final {
 public:
  /// Initializes an empty PaintFragment
  PaintFragment() = default;

  /**
   * Creates a paint fragment by recording the draw commands and dependent tabs
   * for an nsIDocShell.
   *
   * @param aDocShell The document shell to record.
   * @param aRect The rectangle relative to the viewport to use.
   * @param aScale The coordinate scale to use. The size of the resolved
   *   surface will be `aRect.Size() * aScale`, with aScale clamped to
   *   at least kMinPaintScale.
   * @param aBackgroundColor The background color to use.
   *
   * @return A paint fragment. The paint fragment may be `empty` if rendering
   *         was unable to be accomplished for some reason.
   */
  static PaintFragment Record(nsIDocShell* aDocShell, const IntRect& aRect,
                              float aScale, nscolor aBackgroundColor);

  /// Returns whether this paint fragment contains a valid recording.
  bool IsEmpty() const;

  PaintFragment(PaintFragment&&) = default;
  PaintFragment& operator=(PaintFragment&&) = default;

 protected:
  friend struct IPC::ParamTraits<PaintFragment>;
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
  /**
   * Begin an asynchronous paint of a cross process document tree starting at
   * a local document shell. The local document will be painted, then async
   * paints will be queued for remote subframes. Once all subframes have been
   * recorded, the final image will be resolved, and the promise will be
   * resolved with a dom::ImageBitmap.
   *
   * @param aDocShell The document shell to paint.
   * @param aRect The rectangle relative to the viewport to use.
   * @param aScale The coordinate scale to use. The size of the resolved
   *   surface will be `aRect.Size() * aScale`, with aScale clamped to
   *   at least kMinPaintScale. See the implementation for the current
   *   minimum value.
   * @param aBackgroundColor The background color to use.
   * @param aPromise The promise to resolve with a dom::ImageBitmap.
   */
  static void StartLocal(nsIDocShell* aRoot, const IntRect& aRect, float aScale,
                         nscolor aBackgroundColor, dom::Promise* aPromise);

  /**
   * Begin an asynchronous paint of a cross process document tree starting at
   * a remote tab. An async paint for the remote tab will be queued, then async
   * paints will be recursively queued for remote subframes. Once all subframes
   * have been recorded, the final image will be resolved, and the promise will
   * be resolved with a dom::ImageBitmap.
   *
   * @param aDocShell The document shell to paint.
   * @param aRect The rectangle relative to the viewport to use.
   * @param aScale The coordinate scale to use. The size of the resolved
   *   surface will be `aRect.Size() * aScale`, with aScale clamped to
   *   at least kMinPaintScale. See the implementation for the current
   *   minimum value.
   * @param aBackgroundColor The background color to use.
   * @param aPromise The promise to resolve with a dom::ImageBitmap.
   */
  static void StartRemote(dom::TabId aRoot, const IntRect& aRect, float aScale,
                          nscolor aBackgroundColor, dom::Promise* aPromise);

  void ReceiveFragment(dom::TabId aId, PaintFragment&& aFragment);
  void LostFragment(dom::TabId aId);

 private:
  typedef nsRefPtrHashtable<nsUint64HashKey, SourceSurface> ResolvedSurfaceMap;
  typedef nsDataHashtable<nsUint64HashKey, PaintFragment> ReceivedFragmentMap;

  CrossProcessPaint(dom::Promise* aPromise, float aScale,
                    nscolor aBackgroundColor, dom::TabId aRootId);
  ~CrossProcessPaint();

  void QueueRootPaint(dom::TabId aId, const IntRect& aRect, float aScale,
                      nscolor aBackgroundColor);
  void QueueSubPaint(dom::TabId aId);

  /// Clear the state of this paint so that it cannot be resolved or receive
  /// any paint fragments.
  void Clear();

  /// Returns if this paint has been cleared.
  bool IsCleared() const;

  /// Resolves the paint fragments if we have none pending and resolves the
  /// promise.
  void MaybeResolve();
  bool ResolveInternal(dom::TabId aId, ResolvedSurfaceMap* aResolved);

  RefPtr<dom::Promise> mPromise;
  dom::TabId mRootId;
  float mScale;
  nscolor mBackgroundColor;
  uint32_t mPendingFragments;
  ReceivedFragmentMap mReceivedFragments;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_CrossProcessPaint_h_
