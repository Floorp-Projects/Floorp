/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *   Ali Juma <ajuma@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_layers_CompositorParent_h
#define mozilla_layers_CompositorParent_h

// Enable this pref to turn on compositor performance warning.
// This will print warnings if the compositor isn't meeting
// its responsiveness objectives:
//    1) Compose a frame within 15ms of receiving a ScheduleCompositeCall
//    2) Unless a frame was composited within the throttle threshold in
//       which the deadline will be 15ms + throttle threshold
#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/PLayersParent.h"
#include "base/thread.h"
#include "ShadowLayersManager.h"

class nsIWidget;

namespace base {
class Thread;
}

namespace mozilla {
namespace layers {

class LayerManager;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(nsIntPoint aTranslation = nsIntPoint(0, 0), float aXScale = 1, float aYScale = 1)
    : mTranslation(aTranslation)
    , mXScale(aXScale)
    , mYScale(aYScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::ScalingMatrix(mXScale, mYScale, 1) *
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0);
  }

  nsIntPoint mTranslation;
  float mXScale;
  float mYScale;
};

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorParent)
public:
  CompositorParent(nsIWidget* aWidget, base::Thread* aCompositorThread);
  virtual ~CompositorParent();

  virtual bool RecvStop() MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(bool isFirstPaint) MOZ_OVERRIDE;
  void Destroy();

  LayerManager* GetLayerManager() { return mLayerManager; }

  void SetTransformation(float aScale, nsIntPoint aScrollOffset);
  void AsyncRender();

  // Can be called from any thread
  void ScheduleRenderOnCompositorThread();
  void SchedulePauseOnCompositorThread();
  void ScheduleResumeOnCompositorThread();

protected:
  virtual PLayersParent* AllocPLayers(const LayersBackend &backendType);
  virtual bool DeallocPLayers(PLayersParent* aLayers);

private:
  void PauseComposition();
  void ResumeComposition();

  void Composite();
  void ScheduleComposition();
  void TransformShadowTree();

  // Platform specific functions
#ifdef MOZ_WIDGET_ANDROID
  /**
   * Does a breadth-first search to find the first layer in the tree with a
   * displayport set.
   */
  Layer* GetPrimaryScrollableLayer();
#endif

  nsRefPtr<LayerManager> mLayerManager;
  base::Thread* mCompositorThread;
  nsIWidget* mWidget;
  CancelableTask *mCurrentCompositeTask;
  TimeStamp mLastCompose;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeTime;
#endif

  bool mPaused;
  float mXScale;
  float mYScale;
  nsIntPoint mScrollOffset;
  nsIntSize mContentSize;

  // When this flag is set, the next composition will be the first for a
  // particular document (i.e. the document displayed on the screen will change).
  // This happens when loading a new page or switching tabs. We notify the
  // front-end (e.g. Java on Android) about this so that it take the new page
  // size and zoom into account when providing us with the next view transform.
  bool mIsFirstPaint;

  // This flag is set during a layers update, so that the first composition
  // after a layers update has it set. It is cleared after that first composition.
  bool mLayersUpdated;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
