/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PostTraversalTask_h
#define mozilla_PostTraversalTask_h

#include "nscore.h"

/* a task to be performed immediately after a Servo traversal */

namespace mozilla {
class ServoStyleSet;
namespace dom {
class FontFace;
class FontFaceSet;
class FontFaceSetImpl;
}  // namespace dom
namespace fontlist {
struct Family;
}  // namespace fontlist
}  // namespace mozilla
class gfxUserFontEntry;

namespace mozilla {

/**
 * A PostTraversalTask is a task to be performed immediately after a Servo
 * traversal.  There are just a few tasks we need to perform, so we use this
 * class rather than Runnables, to avoid virtual calls and some allocations.
 *
 * A PostTraversalTask is only safe to run immediately after the Servo
 * traversal, since it can hold raw pointers to DOM objects.
 */
class PostTraversalTask {
 public:
  static PostTraversalTask ResolveFontFaceLoadedPromise(
      dom::FontFace* aFontFace) {
    auto task = PostTraversalTask(Type::ResolveFontFaceLoadedPromise);
    task.mTarget = aFontFace;
    return task;
  }

  static PostTraversalTask RejectFontFaceLoadedPromise(dom::FontFace* aFontFace,
                                                       nsresult aResult) {
    auto task = PostTraversalTask(Type::ResolveFontFaceLoadedPromise);
    task.mTarget = aFontFace;
    task.mResult = aResult;
    return task;
  }

  static PostTraversalTask DispatchLoadingEventAndReplaceReadyPromise(
      dom::FontFaceSet* aFontFaceSet) {
    auto task =
        PostTraversalTask(Type::DispatchLoadingEventAndReplaceReadyPromise);
    task.mTarget = aFontFaceSet;
    return task;
  }

  static PostTraversalTask DispatchFontFaceSetCheckLoadingFinishedAfterDelay(
      dom::FontFaceSetImpl* aFontFaceSet) {
    auto task = PostTraversalTask(
        Type::DispatchFontFaceSetCheckLoadingFinishedAfterDelay);
    task.mTarget = aFontFaceSet;
    return task;
  }

  static PostTraversalTask LoadFontEntry(gfxUserFontEntry* aFontEntry) {
    auto task = PostTraversalTask(Type::LoadFontEntry);
    task.mTarget = aFontEntry;
    return task;
  }

  static PostTraversalTask InitializeFamily(fontlist::Family* aFamily) {
    auto task = PostTraversalTask(Type::InitializeFamily);
    task.mTarget = aFamily;
    return task;
  }

  static PostTraversalTask FontInfoUpdate(ServoStyleSet* aSet) {
    auto task = PostTraversalTask(Type::FontInfoUpdate);
    task.mTarget = aSet;
    return task;
  }

  void Run();

 private:
  // For any new raw pointer type that we need to store in a PostTraversalTask,
  // please add an assertion that class' destructor that we are not in a Servo
  // traversal, to protect against the possibility of having dangling pointers.
  enum class Type {
    // mTarget (FontFace*)
    ResolveFontFaceLoadedPromise,

    // mTarget (FontFace*)
    // mResult
    RejectFontFaceLoadedPromise,

    // mTarget (FontFaceSet*)
    DispatchLoadingEventAndReplaceReadyPromise,

    // mTarget (FontFaceSetImpl*)
    DispatchFontFaceSetCheckLoadingFinishedAfterDelay,

    // mTarget (gfxUserFontEntry*)
    LoadFontEntry,

    // mTarget (fontlist::Family*)
    InitializeFamily,

    // mTarget (ServoStyleSet*)
    FontInfoUpdate,
  };

  explicit PostTraversalTask(Type aType)
      : mType(aType), mTarget(nullptr), mResult(NS_OK) {}

  Type mType;
  void* mTarget;
  nsresult mResult;
};

}  // namespace mozilla

#endif  // mozilla_PostTraversalTask_h
