/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIJSEventListener_h__
#define nsIJSEventListener_h__

#include "nsIScriptContext.h"

class nsIScriptObjectOwner;
class nsIDOMEventListener;
class nsIAtom;

#define NS_IJSEVENTLISTENER_IID     \
{ 0xa6cf9118, 0x15b3, 0x11d2,       \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Implemented by script event listeners. Used to retrieve the
// script object corresponding to the event target.
// (Note this interface is now used to store script objects for all
// script languages, so is no longer JS specific)
class nsIJSEventListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSEVENTLISTENER_IID)

  nsIJSEventListener(nsIScriptContext *aContext, void *aScopeObject,
                     nsISupports *aTarget)
    : mContext(aContext), mScopeObject(aScopeObject), mTarget(nsnull)
  {
    // We keep a weak-ref to the event target to prevent cycles that prevent
    // GC from cleaning up our global in all cases.  However, as this is a
    // weak-ref, we must ensure it is the identity of the event target and
    // not a "tear-off" or similar that may not live as long as we expect.
    aTarget->QueryInterface(NS_GET_IID(nsISupports),
                            NS_REINTERPRET_CAST(void **, &mTarget));
    if (mTarget)
      // We keep a weak-ref, so remove the reference the QI added.
      mTarget->Release();
    else {
      NS_ERROR("Failed to get identity pointer");
    }
    // To help debug such leaks, we keep a counter of the event listeners
    // currently alive.  If you change |mTarget| to a strong-ref, this never
    // hits zero (running seamonkey.)
#ifdef NS_DEBUG
    PR_AtomicIncrement(&sNumJSEventListeners);
#endif
  }

  nsIScriptContext *GetEventContext()
  {
    return mContext;
  }

  nsISupports *GetEventTarget()
  {
    return mTarget;
  }

  void *GetEventScope()
  {
    return mScopeObject;
  }

  virtual void SetEventName(nsIAtom* aName) = 0;

#ifdef NS_DEBUG
  static PRInt32 sNumJSEventListeners;
#endif

protected:
  virtual ~nsIJSEventListener()
  {
#ifdef NS_DEBUG
    PR_AtomicDecrement(&sNumJSEventListeners);
#endif
  }
  nsCOMPtr<nsIScriptContext> mContext;
  void *mScopeObject;
  nsISupports *mTarget; // weak ref.
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSEventListener, NS_IJSEVENTLISTENER_IID)

/* factory function */
nsresult NS_NewJSEventListener(nsIScriptContext *aContext,
                               void *aScopeObject, nsISupports *aObject,
                               nsIDOMEventListener **aReturn);

#endif // nsIJSEventListener_h__
