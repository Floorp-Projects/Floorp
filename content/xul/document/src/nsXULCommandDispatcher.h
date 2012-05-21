/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*

   This is the focus manager for XUL documents.

*/

#ifndef nsXULCommandDispatcher_h__
#define nsXULCommandDispatcher_h__

#include "nsCOMPtr.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsWeakReference.h"
#include "nsIDOMNode.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMElement;
class nsPIWindowRoot;

class nsXULCommandDispatcher : public nsIDOMXULCommandDispatcher,
                               public nsSupportsWeakReference
{
public:
    nsXULCommandDispatcher(nsIDocument* aDocument);
    virtual ~nsXULCommandDispatcher();

    // nsISupports
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULCommandDispatcher,
                                             nsIDOMXULCommandDispatcher)

    // nsIDOMXULCommandDispatcher interface
    NS_DECL_NSIDOMXULCOMMANDDISPATCHER

    void Disconnect();
protected:
    already_AddRefed<nsPIWindowRoot> GetWindowRoot();

    nsIContent* GetRootFocusedContentAndWindow(nsPIDOMWindow** aWindow);

    nsCOMPtr<nsIDocument> mDocument;

    class Updater {
    public:
      Updater(nsIDOMElement* aElement,
              const nsAString& aEvents,
              const nsAString& aTargets)
          : mElement(aElement),
            mEvents(aEvents),
            mTargets(aTargets),
            mNext(nsnull)
      {}

      nsCOMPtr<nsIDOMElement> mElement;
      nsString                mEvents;
      nsString                mTargets;
      Updater*                mNext;
    };

    Updater* mUpdaters;

    bool Matches(const nsString& aList, 
                   const nsAString& aElement);
};

#endif // nsXULCommandDispatcher_h__
