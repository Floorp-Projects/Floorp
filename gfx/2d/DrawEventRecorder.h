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

  void WriteHeader();

  void RecordEvent(const RecordedEvent &aEvent);
  void WritePath(const PathRecording *aPath);

  void AddStoredObject(const ReferencePtr aObject) {
    mStoredObjects.insert(aObject);
  }

  void RemoveStoredObject(const ReferencePtr aObject) {
    mStoredObjects.erase(aObject);
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
#else
  typedef std::set<const void*> ObjectSet;
  typedef std::set<uint64_t> Uint64Set;
#endif

  ObjectSet mStoredObjects;
  Uint64Set mStoredFontData;
};

class DrawEventRecorderFile : public DrawEventRecorderPrivate
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DrawEventRecorderFile)
  explicit DrawEventRecorderFile(const char *aFilename);
  ~DrawEventRecorderFile();

private:
  virtual void Flush();

  std::ofstream mOutputFile;
};

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

private:
  ~DrawEventRecorderMemory() {};

  void Flush() final;

  std::stringstream mMemoryStream;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWEVENTRECORDER_H_ */
