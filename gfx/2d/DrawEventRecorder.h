/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWEVENTRECORDER_H_
#define MOZILLA_GFX_DRAWEVENTRECORDER_H_

#include "2D.h"
#include "RecordedEvent.h"
#include "RecordingTypes.h"

#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <vector>

#include "mozilla/DataMutex.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "nsTHashSet.h"

namespace mozilla {
namespace gfx {

class PathRecording;

class DrawEventRecorderPrivate : public DrawEventRecorder {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderPrivate, override)

  DrawEventRecorderPrivate();
  virtual ~DrawEventRecorderPrivate() = default;
  bool Finish() override {
    ClearResources();
    return true;
  }
  virtual void FlushItem(IntRect) {}
  void DetachResources() {
    std::unordered_set<ScaledFont*> fonts;
    fonts.swap(mStoredFonts);
    std::for_each(fonts.begin(), fonts.end(), [this](auto font) {
      font->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
    });

    // SourceSurfaces can be deleted off the main thread, so we use
    // ThreadSafeWeakPtrs to allow for this. RemoveUserData is thread safe.
    std::unordered_map<void*, ThreadSafeWeakPtr<SourceSurface>> surfaces;
    surfaces.swap(mStoredSurfaces);
    std::for_each(surfaces.begin(), surfaces.end(), [this](auto surfacePair) {
      RefPtr<SourceSurface> strongRef(surfacePair.second);
      if (strongRef) {
        strongRef->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
      }
    });

    // Now that we've detached we can't get any more pending deletions, so
    // processing now should mean we include all clean up operations.
    ProcessPendingDeletions();
  }

  void ClearResources() {
    mStoredObjects.clear();
    mStoredFontData.clear();
    mScaledFonts.clear();
  }

  template <class S>
  void WriteHeader(S& aStream) {
    WriteElement(aStream, kMagicInt);
    WriteElement(aStream, kMajorRevision);
    WriteElement(aStream, kMinorRevision);
  }

  virtual void RecordEvent(const RecordedEvent& aEvent) = 0;

  void AddStoredObject(const ReferencePtr aObject) {
    ProcessPendingDeletions();
    mStoredObjects.insert(aObject);
  }

  /**
   * This is a combination of HasStoredObject and AddStoredObject, so that we
   * only have to call ProcessPendingDeletions once, which involves locking.
   * @param aObject the object to store if not already stored
   * @return true if the object was not already stored, false if it was
   */
  bool TryAddStoredObject(const ReferencePtr aObject) {
    ProcessPendingDeletions();
    if (mStoredObjects.find(aObject) != mStoredObjects.end()) {
      return false;
    }

    mStoredObjects.insert(aObject);
    return true;
  }

  void AddPendingDeletion(std::function<void()>&& aPendingDeletion) {
    auto lockedPendingDeletions = mPendingDeletions.Lock();
    lockedPendingDeletions->emplace_back(std::move(aPendingDeletion));
  }

  void RemoveStoredObject(const ReferencePtr aObject) {
    mStoredObjects.erase(aObject);
  }

  /**
   * @param aUnscaledFont the UnscaledFont to increment the reference count for
   * @return the previous reference count
   */
  int32_t IncrementUnscaledFontRefCount(const ReferencePtr aUnscaledFont) {
    int32_t& count = mUnscaledFontRefs[aUnscaledFont];
    return count++;
  }

  /**
   * Decrements the reference count for aUnscaledFont and, if count is now zero,
   * records its destruction.
   * @param aUnscaledFont the UnscaledFont to decrement the reference count for
   */
  void DecrementUnscaledFontRefCount(const ReferencePtr aUnscaledFont);

  void AddScaledFont(ScaledFont* aFont) {
    if (mStoredFonts.insert(aFont).second && WantsExternalFonts()) {
      mScaledFonts.push_back(aFont);
    }
  }

  void RemoveScaledFont(ScaledFont* aFont) { mStoredFonts.erase(aFont); }

  void AddSourceSurface(SourceSurface* aSurface) {
    mStoredSurfaces.emplace(aSurface, aSurface);
  }

  void RemoveSourceSurface(SourceSurface* aSurface) {
    mStoredSurfaces.erase(aSurface);
  }

#if defined(DEBUG)
  // Only used within debug assertions.
  bool HasStoredObject(const ReferencePtr aObject) {
    ProcessPendingDeletions();
    return mStoredObjects.find(aObject) != mStoredObjects.end();
  }
#endif

  void AddStoredFontData(const uint64_t aFontDataKey) {
    mStoredFontData.insert(aFontDataKey);
  }

  bool HasStoredFontData(const uint64_t aFontDataKey) {
    return mStoredFontData.find(aFontDataKey) != mStoredFontData.end();
  }

  bool WantsExternalFonts() const { return mExternalFonts; }

  void TakeExternalSurfaces(std::vector<RefPtr<SourceSurface>>& aSurfaces) {
    aSurfaces = std::move(mExternalSurfaces);
  }

  virtual void StoreSourceSurfaceRecording(SourceSurface* aSurface,
                                           const char* aReason);

  /**
   * Used when a source surface is destroyed, aSurface is a void* instead of a
   * SourceSurface* because this is called during the SourceSurface destructor,
   * so it is partially destructed and should not be accessed.
   * @param aSurface the surface whose destruction is being recorded
   */
  void RecordSourceSurfaceDestruction(void* aSurface);

  virtual void AddDependentSurface(uint64_t aDependencyId) {
    MOZ_CRASH("GFX: AddDependentSurface");
  }

 protected:
  void StoreExternalSurfaceRecording(SourceSurface* aSurface, uint64_t aKey);

  void ProcessPendingDeletions() {
    MOZ_ASSERT(NS_IsMainThread());

    PendingDeletionsVector pendingDeletions;
    {
      auto lockedPendingDeletions = mPendingDeletions.Lock();
      pendingDeletions.swap(*lockedPendingDeletions);
    }
    for (const auto& pendingDeletion : pendingDeletions) {
      pendingDeletion();
    }
  }

  virtual void Flush() = 0;

  std::unordered_set<const void*> mStoredObjects;

  using PendingDeletionsVector = std::vector<std::function<void()>>;
  DataMutex<PendingDeletionsVector> mPendingDeletions{
      "DrawEventRecorderPrivate::mPendingDeletions"};

  // It's difficult to track the lifetimes of UnscaledFonts directly, so we
  // instead track the number of recorded ScaledFonts that hold a reference to
  // an Unscaled font and use that as a proxy to the real lifetime. An
  // UnscaledFonts lifetime could be longer than this, but we only use the
  // ScaledFonts directly and if another uses an UnscaledFont we have destroyed
  // on the translation side, it will be recreated.
  std::unordered_map<const void*, int32_t> mUnscaledFontRefs;

  std::unordered_set<uint64_t> mStoredFontData;
  std::unordered_set<ScaledFont*> mStoredFonts;
  std::vector<RefPtr<ScaledFont>> mScaledFonts;

  // SourceSurfaces can get deleted off the main thread, so we hold a map of the
  // raw pointer to a ThreadSafeWeakPtr to protect against this.
  std::unordered_map<void*, ThreadSafeWeakPtr<SourceSurface>> mStoredSurfaces;
  std::vector<RefPtr<SourceSurface>> mExternalSurfaces;
  bool mExternalFonts;
};

typedef std::function<void(MemStream& aStream,
                           std::vector<RefPtr<ScaledFont>>& aScaledFonts)>
    SerializeResourcesFn;

// WARNING: This should not be used in its existing state because
// it is likely to OOM because of large continguous allocations.
class DrawEventRecorderMemory : public DrawEventRecorderPrivate {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderMemory, override)

  /**
   * Constructs a DrawEventRecorder that stores the recording in memory.
   */
  DrawEventRecorderMemory();
  explicit DrawEventRecorderMemory(const SerializeResourcesFn& aSerialize);

  void RecordEvent(const RecordedEvent& aEvent) override;

  void AddDependentSurface(uint64_t aDependencyId) override;

  nsTHashSet<uint64_t>&& TakeDependentSurfaces();

  /**
   * @return the current size of the recording (in chars).
   */
  size_t RecordingSize();

  /**
   * Wipes the internal recording buffer, but the recorder does NOT forget which
   * objects it has recorded. This can be used so that a recording can be copied
   * and processed in chunks, releasing memory as it goes.
   */
  void WipeRecording();
  bool Finish() override;
  void FlushItem(IntRect) override;

  MemStream mOutputStream;
  /* The index stream is of the form:
   * ItemIndex { size_t dataEnd; size_t extraDataEnd; }
   * It gets concatenated to the end of mOutputStream in Finish()
   * The last size_t in the stream is offset of the begining of the
   * index.
   */
  MemStream mIndex;

 protected:
  virtual ~DrawEventRecorderMemory() = default;

 private:
  SerializeResourcesFn mSerializeCallback;
  nsTHashSet<uint64_t> mDependentSurfaces;

  void Flush() override;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_DRAWEVENTRECORDER_H_ */
