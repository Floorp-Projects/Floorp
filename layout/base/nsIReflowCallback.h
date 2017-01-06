/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIReflowCallback_h___
#define nsIReflowCallback_h___

/**
 * Reflow callback interface.
 * These are not refcounted. Objects must be removed from the presshell
 * callback list before they die.
 * Protocol: objects will either get a ReflowFinished() call when a reflow
 * has finished or a ReflowCallbackCanceled() call if the shell is destroyed,
 * whichever happens first. If the object is explicitly removed from the shell
 * (using nsIPresShell::CancelReflowCallback()) before that occurs then neither
 * of the callback methods are called.
 */
class nsIReflowCallback {
public:
  /**
   * The presshell calls this when reflow has finished. Return true if
   * you need a FlushType::Layout to happen after this.
   */
  virtual bool ReflowFinished() = 0;
  /**
   * The presshell calls this on outstanding callback requests in its
   * Destroy() method. The shell removes the request after calling
   * ReflowCallbackCanceled().
   */
  virtual void ReflowCallbackCanceled() = 0;
};

#endif /* nsIReflowCallback_h___ */
