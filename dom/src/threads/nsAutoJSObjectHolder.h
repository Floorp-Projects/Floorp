/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __NSAUTOJSOBJECTHOLDER_H__
#define __NSAUTOJSOBJECTHOLDER_H__

#include "jsapi.h"

/**
 * Simple class that looks and acts like a JSObject* except that it unroots
 * itself automatically if Root() is ever called. Designed to be rooted on the
 * context or runtime (but not both!). Also automatically nulls its JSObject*
 * on Unroot and asserts that Root has been called prior to assigning an object.
 */
class nsAutoJSObjectHolder
{
public:
  /**
   * Default constructor, no holding.
   */
  nsAutoJSObjectHolder()
  : mRt(NULL), mObj(NULL), mHeld(PR_FALSE) { }

  /**
   * Hold by rooting on the context's runtime in the constructor, passing the
   * result out.
   */
  nsAutoJSObjectHolder(JSContext* aCx, JSBool* aRv = NULL,
                       JSObject* aObj = NULL)
  : mRt(NULL), mObj(aObj), mHeld(JS_FALSE) {
    JSBool rv = Hold(aCx);
    if (aRv) {
      *aRv = rv;
    }
  }

  /**
   * Hold by rooting on the runtime in the constructor, passing the result out.
   */
  nsAutoJSObjectHolder(JSRuntime* aRt, JSBool* aRv = NULL,
                       JSObject* aObj = NULL)
  : mRt(aRt), mObj(aObj), mHeld(JS_FALSE) {
    JSBool rv = Hold(aRt);
    if (aRv) {
      *aRv = rv;
    }
  }

  /**
   * Always release on destruction.
   */
  ~nsAutoJSObjectHolder() {
    Release();
  }

  /**
   * Hold by rooting on the context's runtime.
   */
  JSBool Hold(JSContext* aCx) {
    return Hold(JS_GetRuntime(aCx));
  }

  /**
   * Hold by rooting on the runtime.
   */
  JSBool Hold(JSRuntime* aRt) {
    if (!mHeld) {
      mHeld = JS_AddNamedRootRT(aRt, &mObj, "nsAutoRootedJSObject");
      if (mHeld) {
        mRt = aRt;
      }
    }
    return mHeld;
  }

  /**
   * Manually release.
   */
  void Release() {
    NS_ASSERTION(!mHeld || mRt, "Bad!");
    if (mHeld) {
      mHeld = !JS_RemoveRootRT(mRt, &mObj);
      if (!mHeld) {
        mRt = NULL;
      }
      mObj = NULL;
    }
  }

  /**
   * Determine if Hold has been called.
   */
  JSBool IsHeld() {
    return mHeld;
  }

  /**
   * Pretend to be a JSObject*.
   */
  JSObject* get() const {
    return mObj;
  }

  /**
   * Pretend to be a JSObject*.
   */
  operator JSObject*() const {
    return get();
  }

  /**
   * Pretend to be a JSObject*. Assert if not held.
   */
  JSObject* operator=(JSObject* aOther) {
    NS_ASSERTION(mHeld, "Not rooted!");
    return mObj = aOther;
  }

private:
  JSRuntime* mRt;
  JSObject* mObj;
  JSBool mHeld;
};

#endif /* __NSAUTOJSOBJECTHOLDER_H__ */
