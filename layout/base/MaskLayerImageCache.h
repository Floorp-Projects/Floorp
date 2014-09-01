/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MASKLAYERIMAGECACHE_H_
#define MASKLAYERIMAGECACHE_H_

#include "DisplayItemClip.h"
#include "nsPresContext.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {

namespace layers {
class ImageContainer;
}

/**
 * Keeps a record of image containers for mask layers, containers are mapped
 * from the rounded rects used to create them.
 * The cache stores MaskLayerImageEntries indexed by MaskLayerImageKeys.
 * Each MaskLayerImageEntry owns a heap-allocated MaskLayerImageKey
 * (heap-allocated so that a mask layer's userdata can keep a pointer to the
 * key for its image, in spite of the hashtable moving its entries around).
 * The key consists of the rounded rects used to create the mask,
 * an nsRefPtr to the ImageContainer containing the image, and a count
 * of the number of layers currently using this ImageContainer.
 * When the key's layer count is zero, the cache
 * may remove the entry, which deletes the key object.
 */
class MaskLayerImageCache
{
  typedef mozilla::layers::ImageContainer ImageContainer;
public:
  MaskLayerImageCache();
  ~MaskLayerImageCache();

  /**
   * Representation of a rounded rectangle in device pixel coordinates, in
   * contrast to DisplayItemClip::RoundedRect, which uses app units.
   * In particular, our internal representation uses a gfxRect, rather than
   * an nsRect, so this class is easier to use with transforms.
   */
  struct PixelRoundedRect
  {
    PixelRoundedRect(const DisplayItemClip::RoundedRect& aRRect,
                     nsPresContext* aPresContext)
      : mRect(aPresContext->AppUnitsToGfxUnits(aRRect.mRect.x),
              aPresContext->AppUnitsToGfxUnits(aRRect.mRect.y),
              aPresContext->AppUnitsToGfxUnits(aRRect.mRect.width),
              aPresContext->AppUnitsToGfxUnits(aRRect.mRect.height))
    {
      MOZ_COUNT_CTOR(PixelRoundedRect);
      NS_FOR_CSS_HALF_CORNERS(corner) {
        mRadii[corner] = aPresContext->AppUnitsToGfxUnits(aRRect.mRadii[corner]);
      }
    }
    PixelRoundedRect(const PixelRoundedRect& aPRR)
      : mRect(aPRR.mRect)
    {
      MOZ_COUNT_CTOR(PixelRoundedRect);
      NS_FOR_CSS_HALF_CORNERS(corner) {
        mRadii[corner] = aPRR.mRadii[corner];
      }
    }

    ~PixelRoundedRect()
    {
      MOZ_COUNT_DTOR(PixelRoundedRect);
    }

    // Applies the scale and translate components of aTransform.
    // It is an error to pass a matrix which does more than just scale
    // and translate.
    void ScaleAndTranslate(const gfx::Matrix& aTransform)
    {
      NS_ASSERTION(aTransform._12 == 0 && aTransform._21 == 0,
                   "Transform has a component other than scale and translate");

      mRect = aTransform.TransformBounds(mRect);

      for (size_t i = 0; i < ArrayLength(mRadii); i += 2) {
        mRadii[i] *= aTransform._11;
        mRadii[i + 1] *= aTransform._22;
      }
    }

    bool operator==(const PixelRoundedRect& aOther) const {
      if (!mRect.IsEqualInterior(aOther.mRect)) {
        return false;
      }

      NS_FOR_CSS_HALF_CORNERS(corner) {
        if (mRadii[corner] != aOther.mRadii[corner]) {
          return false;
        }
      }
      return true;
    }
    bool operator!=(const PixelRoundedRect& aOther) const {
      return !(*this == aOther);
    }

    // Create a hash for this object.
    PLDHashNumber Hash() const
    {
      PLDHashNumber hash = HashBytes(&mRect.x, 4*sizeof(gfxFloat));
      hash = AddToHash(hash, HashBytes(mRadii, 8*sizeof(gfxFloat)));

      return hash;
    }

    gfx::Rect mRect;
    // Indices into mRadii are the NS_CORNER_* constants in nsStyleConsts.h
    gfxFloat mRadii[8];

  private:
    PixelRoundedRect() MOZ_DELETE;
  };

  /**
   * A key to identify cached image containers.
   * The const-ness of this class is with respect to its use as a key into a
   * hashtable, so anything not used to create the hash is mutable.
   * mLayerCount counts the number of mask layers which have a reference to
   * MaskLayerImageEntry::mContainer; it is maintained by MaskLayerUserData,
   * which keeps a reference to the key. There will usually be mLayerCount + 1
   * pointers to a key object (the +1 being from the hashtable entry), but this
   * invariant may be temporarily broken.
   */
  struct MaskLayerImageKey
  {
    MaskLayerImageKey();
    MaskLayerImageKey(const MaskLayerImageKey& aKey);

    ~MaskLayerImageKey();

    void AddRef() const { ++mLayerCount; }
    void Release() const
    {
      NS_ASSERTION(mLayerCount > 0, "Inconsistent layer count");
      --mLayerCount;
    }

    PLDHashNumber Hash() const
    {
      PLDHashNumber hash = 0;

      for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
        hash = AddToHash(hash, mRoundedClipRects[i].Hash());
      }

      return hash;
    }

    bool operator==(const MaskLayerImageKey& aOther) const
    {
      return mRoundedClipRects == aOther.mRoundedClipRects;
    }

    mutable uint32_t mLayerCount;
    nsTArray<PixelRoundedRect> mRoundedClipRects;
  };

  
  // Find an image container for aKey, returns nullptr if there is no suitable
  // cached image. If there is an image, then aKey is set to point at the stored
  // key for the image.
  ImageContainer* FindImageFor(const MaskLayerImageKey** aKey);

  // Add an image container with a key to the cache
  // The image container used will be set as the container in aKey and aKey
  // itself will be linked from this cache
  void PutImage(const MaskLayerImageKey* aKey,
                ImageContainer* aContainer);

  // Sweep the cache for old image containers that can be deleted
  void Sweep();

protected:

  class MaskLayerImageEntry : public PLDHashEntryHdr
  {
  public:
    typedef const MaskLayerImageKey& KeyType;
    typedef const MaskLayerImageKey* KeyTypePointer;

    explicit MaskLayerImageEntry(KeyTypePointer aKey)
      : mKey(aKey)
    {
      MOZ_COUNT_CTOR(MaskLayerImageEntry);
    }
    MaskLayerImageEntry(const MaskLayerImageEntry& aOther)
      : mKey(aOther.mKey.get())
    {
      NS_ERROR("ALLOW_MEMMOVE == true, should never be called");
    }
    ~MaskLayerImageEntry()
    {
      MOZ_COUNT_DTOR(MaskLayerImageEntry);
    }

    // KeyEquals(): does this entry match this key?
    bool KeyEquals(KeyTypePointer aKey) const
    {
      return *mKey == *aKey;
    }

    // KeyToPointer(): Convert KeyType to KeyTypePointer
    static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

    // HashKey(): calculate the hash number
    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return aKey->Hash();
    }

    // ALLOW_MEMMOVE can we move this class with memmove(), or do we have
    // to use the copy constructor?
    enum { ALLOW_MEMMOVE = true };

    bool operator==(const MaskLayerImageEntry& aOther) const
    {
      return KeyEquals(aOther.mKey);
    }

    nsAutoPtr<const MaskLayerImageKey> mKey;
    nsRefPtr<ImageContainer> mContainer;
  };

  nsTHashtable<MaskLayerImageEntry> mMaskImageContainers;

  // helper funtion for Sweep(), called for each entry in the hashtable
  static PLDHashOperator SweepFunc(MaskLayerImageEntry* aEntry, void* aUserArg);
};


}


#endif
