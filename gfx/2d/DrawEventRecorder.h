/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWEVENTRECORDER_H_
#define MOZILLA_GFX_DRAWEVENTRECORDER_H_

#include "2D.h"
#include "RecordedEvent.h"
#include <ostream>
#include <fstream>

#if defined(_MSC_VER)
#include <unordered_set>
#else
#include <set>
#endif

namespace mozilla {
namespace gfx {

class PathRecording;

class DrawEventRecorderPrivate : public DrawEventRecorder
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderPrivate)
  explicit DrawEventRecorderPrivate(std::ostream *aStream);
  virtual ~DrawEventRecorderPrivate() { }
  virtual void Finish() {
    // The iteration is a bit awkward here because our iterator will
    // be invalidated by the removal
    for (auto font = mStoredFonts.begin(); font != mStoredFonts.end(); ) {
      auto oldFont = font++;
      (*oldFont)->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
    }
    for (auto surface = mStoredSurfaces.begin(); surface != mStoredSurfaces.end(); ) {
      auto oldSurface = surface++;
      (*oldSurface)->RemoveUserData(reinterpret_cast<UserDataKey*>(this));
    }

  }

  void WriteHeader();

  void RecordEvent(const RecordedEvent &aEvent);
  void WritePath(const PathRecording *aPath);

  void AddStoredObject(const ReferencePtr aObject) {
    mStoredObjects.insert(aObject);
  }

  void RemoveStoredObject(const ReferencePtr aObject) {
    mStoredObjects.erase(aObject);
  }

  void AddScaledFont(ScaledFont* aFont) {
    mStoredFonts.insert(aFont);
  }

  void RemoveScaledFont(ScaledFont* aFont) {
    mStoredFonts.erase(aFont);
  }

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

protected:
  std::ostream *mOutputStream;

  virtual void Flush() = 0;

#if defined(_MSC_VER)
  typedef std::unordered_set<const void*> ObjectSet;
  typedef std::unordered_set<uint64_t> Uint64Set;
  typedef std::unordered_set<ScaledFont*> FontSet;
  typedef std::unordered_set<SourceSurface*> SurfaceSet;
#else
  typedef std::set<const void*> ObjectSet;
  typedef std::set<uint64_t> Uint64Set;
  typedef std::set<ScaledFont*> FontSet;
  typedef std::set<SourceSurface*> SurfaceSet;
#endif

  ObjectSet mStoredObjects;
  Uint64Set mStoredFontData;
  FontSet mStoredFonts;
  SurfaceSet mStoredSurfaces;
};

class DrawEventRecorderFile : public DrawEventRecorderPrivate
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderFile)
  explicit DrawEventRecorderFile(const char *aFilename);
  ~DrawEventRecorderFile();

  /**
   * Returns whether a recording file is currently open.
   */
  bool IsOpen();

  /**
   * Opens new file with the provided name. The recorder does NOT forget which
   * objects it has recorded. This can be used with Close, so that a recording
   * can be processed in chunks. The file must not already be open.
   */
  void OpenNew(const char *aFilename);

  /**
   * Closes the file so that it can be processed. The recorder does NOT forget
   * which objects it has recorded. This can be used with OpenNew, so that a
   * recording can be processed in chunks. The file must be open.
   */
  void Close();

private:
  virtual void Flush();

  std::ofstream mOutputFile;
};

// WARNING: This should not be used in its existing state because
// it is likely to OOM because of large continguous allocations.
class DrawEventRecorderMemory final : public DrawEventRecorderPrivate
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderMemory)

  /**
   * Constructs a DrawEventRecorder that stores the recording in memory.
   */
  DrawEventRecorderMemory();

  /**
   * @return the current size of the recording (in chars).
   */
  size_t RecordingSize();

  /**
   * Copies at most aBufferLen chars of the recording into aBuffer.
   *
   * @param aBuffer buffer to receive the recording chars
   * @param aBufferLen length of aBuffer
   * @return true if copied successfully
   */
  bool CopyRecording(char* aBuffer, size_t aBufferLen);

  /**
   * Wipes the internal recording buffer, but the recorder does NOT forget which
   * objects it has recorded. This can be used so that a recording can be copied
   * and processed in chunks, releasing memory as it goes.
   */
  void WipeRecording();

  /**
   * Gets a readable reference of the underlying stream, reset to the beginning.
   */
  std::istream& GetInputStream() {
    mMemoryStream.seekg(0);
    return mMemoryStream;
  }

private:
  ~DrawEventRecorderMemory() {};

  void Flush() final;

  std::stringstream mMemoryStream;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWEVENTRECORDER_H_ */
