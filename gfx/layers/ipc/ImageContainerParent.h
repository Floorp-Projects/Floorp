/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECONTAINERPARENT_H
#define MOZILLA_GFX_IMAGECONTAINERPARENT_H

#include "mozilla/layers/PImageContainerParent.h"

namespace mozilla {
namespace layers {

class ImageBridgeParent;

/**
 * Handles reception of SharedImages on the compositor side.
 *
 * Received SharedImages are stored in a global map that can be accessed
 * from the compositor thread only.
 * This way, ShadowImageLayers can access SharedImages using the image's ID
 * without holding a reference to the ImageContainerParent.
 * The ID is passed to the ShadowImageLayer through a regular PCompositor 
 * transaction.
 * All ImageContainerParent's method must be called from the Compositor thread 
 * only.  
 */
class ImageContainerParent : public PImageContainerParent
{
public:

  ImageContainerParent(uint32_t aContainerID);
  ~ImageContainerParent();

  // Overriden from PImageContainerParent (see ImageContainer.ipdl)
  bool RecvPublishImage(const SharedImage& aImage) MOZ_OVERRIDE;
  // Overriden from PImageContainerParent (see DoStop)
  bool RecvStop() MOZ_OVERRIDE;
  // Overriden from PImageContainerParent (see ImageContainer.ipdl)
  bool Recv__delete__() MOZ_OVERRIDE;
  // Overriden from PImageContainerParent (see ImageContainer.ipdl)
  bool RecvFlush() MOZ_OVERRIDE;

  /**
   * Called by RecvStop and ImageBridgeChild::RecvStop.
   *
   * puts the ImageContainerChild in a state in which it cannot send async 
   * messages to the child side, in order to not race with the upcomming 
   * __delete__ message.
   */
  void DoStop();

  static SharedImage* GetSharedImage(uint64_t aID);
  /**
   * Every time a SharedImage is swaped, a version counter associated with the 
   * iamge's ID is incremented. This allows ImageLayers to compare the current 
   * image with the one that has been displayed last (in particular on OGL 
   * backend we can avoid uploading the same texture twice if composition 
   * happens more frequently than ImageBridge transfers);
   */
  static uint32_t GetSharedImageVersion(uint64_t aID);

  /**
   * Returns true if this ID exists in the global SharedImage table.
   *
   * Should be called from the Compositor thread
   */
  static bool IsExistingID(uint64_t aID);

  /**
   * Associates an image with a compositor ID, so that when the ImageContainerParent 
   * receives an image with a given ID, it can lookup the right compositor and 
   * ask it to recomposite.
   *
   * This should be called by the ShadowImageLayer each time it renders a shared
   * image, to make sure the compositor ID is updated if the image stream changes 
   * compositor (for instance when the tab containing the stream is dragged to 
   * another window).
   */
  static bool SetCompositorIDForImage(uint64_t aImageID, uint64_t aCompositorID);

  /**
   * Must be called on the compositor thread.
   */
  static uint64_t GetCompositorIDForImage(uint64_t aImageID);

  /**
   * Must be called before the first use of ImageBridge and Compositor.
   */
  static void CreateSharedImageMap();

  /**
   * Must be called after the last use of ImageBridge and Compositor.
   */
  static void DestroySharedImageMap();

protected:
  virtual PGrallocBufferParent*
  AllocPGrallocBuffer(const gfxIntSize&, const gfxContentType&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE
  { return nullptr; }

  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferParent* actor) MOZ_OVERRIDE
  { return false; }

  /**
   * Sets the image associated with t aID in the global SharedImage table and 
   * returns the previous image that was associated with aID.
   *
   * If the global SharedImage table does not yet have an entry for aID, it 
   * creates one and returns nullptr since there was no previous image for aID.
   * Must be called from the Compositor thread.
   */
  static SharedImage* SwapSharedImage(uint64_t aID, SharedImage* aImage);

  /**
   * Removes an entry from the global SharedImage table and returns the 
   * SharedImage that was stored in the table for aID or nullptr if there
   * was no entry for aID.
   * 
   * It is safe to call this function on an ID that does not exist in the table.
   * Must be called from the Compositor thread. 
   */
  static SharedImage* RemoveSharedImage(uint64_t aID);

private:
  uint64_t mID;
  bool  mStop;
};


} // namespace
} // namespace

#endif

