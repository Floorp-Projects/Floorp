/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PostTraversalTask_h
#define mozilla_PostTraversalTask_h

/* a task to be performed immediately after a Servo traversal */

namespace mozilla {

/**
 * A PostTraversalTask is a task to be performed immediately after a Servo
 * traversal.  There are just a few tasks we need to perform, so we use this
 * class rather than Runnables, to avoid virtual calls and some allocations.
 *
 * A PostTraversalTask is only safe to run immediately after the Servo
 * traversal, since it can hold raw pointers to DOM objects.
 */
class PostTraversalTask
{
public:
  void Run();

private:
  enum class Type
  {
    Dummy,
  };

  PostTraversalTask(Type aType)
    : mType(aType)
    , mTarget(nullptr)
  {
  }

  Type mType;
  void* mTarget;
};

} // namespace mozilla

#endif // mozilla_PostTraversalTask_h
