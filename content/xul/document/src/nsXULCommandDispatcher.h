/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */


/*

   This is the focus manager for XUL documents.

*/

#ifndef nsXULCommandDispatcher_h__
#define nsXULCommandDispatcher_h__

#include "nsCOMPtr.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMFocusListener.h"
#include "nsIScriptObjectOwner.h"
#include "nsWeakReference.h"
#include "nsIDOMNode.h"
#include "nsString.h"

class nsIDOMElement;
class nsPIDOMWindow;
class nsIFocusController;

class nsXULCommandDispatcher : public nsIDOMXULCommandDispatcher,
                               public nsIScriptObjectOwner,
                               public nsSupportsWeakReference
{
protected:
    nsXULCommandDispatcher(nsIDocument* aDocument);
    virtual ~nsXULCommandDispatcher();

    void EnsureFocusController();

public:

    static NS_IMETHODIMP
    Create(nsIDocument* aDocument, nsIDOMXULCommandDispatcher** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIDOMXULCommandDispatcher interface
    NS_DECL_IDOMXULCOMMANDDISPATCHER

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
    nsIFocusController* mFocusController; // Weak. We always die before the focus controller does.
    nsIDocument* mDocument; // Weak.

    void* mScriptObject;

    class Updater {
    public:
      Updater(nsIDOMElement* aElement,
              const nsAReadableString& aEvents,
              const nsAReadableString& aTargets)
          : mElement(aElement),
            mEvents(aEvents),
            mTargets(aTargets),
            mNext(nsnull)
      {}

      nsIDOMElement* mElement; // [WEAK]
      nsString       mEvents;
      nsString       mTargets;
      Updater*       mNext;
    };

    Updater* mUpdaters;

    PRBool Matches(const nsString& aList, 
                   const nsAReadableString& aElement);
};

#endif // nsXULCommandDispatcher_h__
