/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTEXTSTATETRACKER_H
#define GFX_CONTEXTSTATETRACKER_H

#include "GLTypes.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"
#include <string.h>

namespace mozilla {
namespace gl {
class GLContext;
} // namespace gl

/**
 * This class tracks the state of the context for debugging and profiling.
 * Each section pushes a new stack entry and must be matched by an end section.
 * All nested section must be ended before ending a parent section.
 */
class ContextStateTracker {
public:
  ContextStateTracker() {}

private:

  bool IsProfiling() { return true; }

protected:
  typedef GLuint TimerQueryHandle;

  class ContextState {
  public:
    explicit ContextState(const char* aSectionName)
      : mSectionName(aSectionName)
    {}

    const char* mSectionName;
    mozilla::TimeStamp mCpuTimeStart;
    mozilla::TimeStamp mCpuTimeEnd;
    TimerQueryHandle mStartQueryHandle;
  };

  ContextState& Top() {
    MOZ_ASSERT(mSectionStack.Length());
    return mSectionStack[mSectionStack.Length() - 1];
  }

  nsTArray<ContextState> mCompletedSections;
  nsTArray<ContextState> mSectionStack;
};

/*
class ID3D11DeviceContext;

class ContextStateTrackerD3D11 final : public ContextStateTracker {
public:
  // TODO Implement me
  void PushD3D11Section(ID3D11DeviceContext* aCtxt, const char* aSectionName) {}
  void PopD3D11Section(ID3D11DeviceContext* aCtxt, const char* aSectionName) {}
  void DestroyD3D11(ID3D11DeviceContext* aCtxt) {}

private:
  void Flush();
};
*/

class ContextStateTrackerOGL final : public ContextStateTracker {
  typedef mozilla::gl::GLContext GLContext;
public:
  void PushOGLSection(GLContext* aGL, const char* aSectionName);
  void PopOGLSection(GLContext* aGL, const char* aSectionName);
  void DestroyOGL(GLContext* aGL);
private:
  void Flush(GLContext* aGL);
};

} // namespace mozilla

#endif

