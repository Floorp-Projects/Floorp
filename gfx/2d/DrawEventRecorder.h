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
#include "mozilla/FStream.h"

#include <unordered_set>
#include <unordered_map>
#include <functional>

#include "nsHashKeys.h"
#include "nsTHashtable.h"

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
    // The iteration is a bit awkward here because our iterator will
    // be invalidated by the removal
    for (auto font = mStoredFonts.begin(); font != mStoredFonts.end();) {
      auto oldFont = font++;
      (*oldFont)->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
    }
    for (auto surface = mStoredSurfaces.begin();
         surface != mStoredSurfaces.end();) {
      auto oldSurface = surface++;
      (*oldSurface)->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
    }
    mStoredFonts.clear();
    mStoredSurfaces.clear();
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
  void WritePath(const PathRecording* aPath);

  void AddStoredObject(const ReferencePtr aObject) {
    mStoredObjects.insert(aObject);
  }

  void RemoveStoredObject(const ReferencePtr aObject) {
    mStoredObjects.erase(aObject);
  }

  void AddScaledFont(ScaledFont* aFont) {
    if (mStoredFonts.insert(aFont).second && WantsExternalFonts()) {
      mScaledFonts.push_back(aFont);
    }
  }

  void RemoveScaledFont(ScaledFont* aFont) { mStoredFonts.erase(aFont); }

  void AddSourceSurface(SourceSurface* aSurface) {
    mStoredSurfaces.insert(aSurface);
  }

  void RemoveSourceSurface(SourceSurface* aSurface) {
    mStoredSurfaces.erase(aSurface);
  }

  bool HasStoredObject(const ReferencePtr aObject) {
    return mStoredObjects.find(aObject) != mStoredObjects.end();
  }

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

  virtual void AddDependentSurface(uint64_t aDependencyId) {
    MOZ_CRASH("GFX: AddDependentSurface");
  }

 protected:
  void StoreExternalSurfaceRecording(SourceSurface* aSurface, uint64_t aKey);

  virtual void Flush() = 0;

  std::unordered_set<const void*> mStoredObjects;
  std::unordered_set<uint64_t> mStoredFontData;
  std::unordered_set<ScaledFont*> mStoredFonts;
  std::vector<RefPtr<ScaledFont>> mScaledFonts;
  std::unordered_set<SourceSurface*> mStoredSurfaces;
  std::vector<RefPtr<SourceSurface>> mExternalSurfaces;
  bool mExternalFonts;
};

class DrawEventRecorderFile : public DrawEventRecorderPrivate {
  using char_type = filesystem::Path::value_type;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderFile, override)
  explicit DrawEventRecorderFile(const char_type* aFilename);
  virtual ~DrawEventRecorderFile();

  void RecordEvent(const RecordedEvent& aEvent) override;

  /**
   * Returns whether a recording file is currently open.
   */
  bool IsOpen();

  /**
   * Opens new file with the provided name. The recorder does NOT forget which
   * objects it has recorded. This can be used with Close, so that a recording
   * can be processed in chunks. The file must not already be open.
   */
  void OpenNew(const char_type* aFilename);

  /**
   * Closes the file so that it can be processed. The recorder does NOT forget
   * which objects it has recorded. This can be used with OpenNew, so that a
   * recording can be processed in chunks. The file must be open.
   */
  void Close();

 private:
  void Flush() override;

  mozilla::OFStream mOutputStream;
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

  nsTHashtable<nsUint64HashKey>&& TakeDependentSurfaces();

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
  virtual ~DrawEventRecorderMemory(){};

 private:
  SerializeResourcesFn mSerializeCallback;
  nsTHashtable<nsUint64HashKey> mDependentSurfaces;

  void Flush() override;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_DRAWEVENTRECORDER_H_ */
