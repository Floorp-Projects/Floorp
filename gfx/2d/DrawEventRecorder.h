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

  void RecordEvent(const RecordedEvent &aEvent);
  void WritePath(const PathRecording *aPath);

  void AddStoredPath(const ReferencePtr aPath) {
    mStoredPaths.insert(aPath);
  }

  void RemoveStoredPath(const ReferencePtr aPath) {
    mStoredPaths.erase(aPath);
  }

  bool HasStoredPath(const ReferencePtr aPath) {
    if (mStoredPaths.find(aPath) != mStoredPaths.end()) {
      return true;
    }
    return false;
  }

protected:
  std::ostream *mOutputStream;

  virtual void Flush() = 0;

#if defined(_MSC_VER)
  typedef std::unordered_set<const void*> ObjectSet;
#else
  typedef std::set<const void*> ObjectSet;
#endif

  ObjectSet mStoredPaths;
  ObjectSet mStoredScaledFonts;
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

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_DRAWEVENTRECORDER_H_ */
