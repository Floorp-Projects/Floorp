/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContextStateTracker.h"
#include "GLContext.h"
#include "GeckoProfiler.h"
#include "ProfilerMarkerPayload.h"

namespace mozilla {

void
ContextStateTrackerOGL::PushOGLSection(GLContext* aGL, const char* aSectionName)
{
  if (!profiler_feature_active(ProfilerFeature::GPU)) {
    return;
  }

  if (!aGL->IsSupported(gl::GLFeature::query_objects)) {
    return;
  }

  if (mSectionStack.Length() > 0) {
    // We need to end the query since we're starting a new section and restore it
    // when this section is finished.
    aGL->fEndQuery(LOCAL_GL_TIME_ELAPSED);
    Top().mCpuTimeEnd = TimeStamp::Now();
  }

  ContextState newSection(aSectionName);

  GLuint queryObject;
  aGL->fGenQueries(1, &queryObject);
  newSection.mStartQueryHandle = queryObject;
  newSection.mCpuTimeStart = TimeStamp::Now();

  aGL->fBeginQuery(LOCAL_GL_TIME_ELAPSED_EXT, queryObject);

  mSectionStack.AppendElement(newSection);
}

void
ContextStateTrackerOGL::PopOGLSection(GLContext* aGL, const char* aSectionName)
{
  // We might have ignored a section start if we started profiling
  // in the middle section. If so we will ignore this unmatched end.
  if (mSectionStack.Length() == 0) {
    return;
  }

  int i = mSectionStack.Length() - 1;
  MOZ_ASSERT(strcmp(mSectionStack[i].mSectionName, aSectionName) == 0);
  aGL->fEndQuery(LOCAL_GL_TIME_ELAPSED);
  mSectionStack[i].mCpuTimeEnd = TimeStamp::Now();
  mCompletedSections.AppendElement(mSectionStack[i]);
  mSectionStack.RemoveElementAt(i);

  if (i - 1 >= 0) {
    const char* sectionToRestore = Top().mSectionName;

    // We need to restore the outer section
    // Well do this by completing this section and adding a new
    // one with the same name
    mCompletedSections.AppendElement(Top());
    mSectionStack.RemoveElementAt(i - 1);

    ContextState newSection(sectionToRestore);

    GLuint queryObject;
    aGL->fGenQueries(1, &queryObject);
    newSection.mStartQueryHandle = queryObject;
    newSection.mCpuTimeStart = TimeStamp::Now();

    aGL->fBeginQuery(LOCAL_GL_TIME_ELAPSED_EXT, queryObject);

    mSectionStack.AppendElement(newSection);
  }

  Flush(aGL);
}

void
ContextStateTrackerOGL::Flush(GLContext* aGL)
{
  TimeStamp now = TimeStamp::Now();

  while (mCompletedSections.Length() != 0) {
    // On mac we see QUERY_RESULT_AVAILABLE cause a GL flush if we query it
    // too early. For profiling we rather have the last 200ms of data missing
    // while causing let's measurement distortions.
    if (mCompletedSections[0].mCpuTimeEnd + TimeDuration::FromMilliseconds(200) > now) {
      break;
    }

    GLuint handle = mCompletedSections[0].mStartQueryHandle;

    // We've waiting 200ms, content rendering at > 20 FPS will be ready. We
    // shouldn't see any flushes now.
    GLuint returned = 0;
    aGL->fGetQueryObjectuiv(handle, LOCAL_GL_QUERY_RESULT_AVAILABLE, &returned);

    if (!returned) {
      break;
    }

    GLuint gpuTime = 0;
    aGL->fGetQueryObjectuiv(handle, LOCAL_GL_QUERY_RESULT, &gpuTime);

    aGL->fDeleteQueries(1, &handle);

    if (profiler_is_active()) {
      profiler_add_marker(
        "gpu_timer_query",
        MakeUnique<GPUMarkerPayload>(mCompletedSections[0].mCpuTimeStart,
                                     mCompletedSections[0].mCpuTimeEnd,
                                     0, gpuTime));
    }

    mCompletedSections.RemoveElementAt(0);
  }
}

void
ContextStateTrackerOGL::DestroyOGL(GLContext* aGL)
{
  while (mCompletedSections.Length() != 0) {
    GLuint handle = (GLuint)mCompletedSections[0].mStartQueryHandle;
    aGL->fDeleteQueries(1, &handle);
    mCompletedSections.RemoveElementAt(0);
  }
}

} // namespace mozilla

