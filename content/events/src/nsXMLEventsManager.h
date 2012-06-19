/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLEventsManager_h___
#define nsXMLEventsManager_h___

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsCOMArray.h"
#include "nsIDOMEventListener.h"
#include "nsInterfaceHashtable.h"
#include "nsIAtom.h"
#include "nsStubDocumentObserver.h"
#include "mozilla/Attributes.h"

/*
 * The implementation of the XML Events Basic profile
 */

class nsXMLEventsManager;
class nsXMLEventsListener MOZ_FINAL : public nsIDOMEventListener {
public:
  static bool InitXMLEventsListener(nsIDocument * aDocument, 
                                      nsXMLEventsManager * aManager, 
                                      nsIContent * aContent);
  nsXMLEventsListener(nsXMLEventsManager * aManager,
                      nsIContent * aElement,
                      nsIContent* aObserver,
                      nsIContent * aHandler,
                      const nsAString& aEvent,
                      bool aPhase,
                      bool aStopPropagation,
                      bool aCancelDefault,
                      const nsAString& aTarget);
  ~nsXMLEventsListener();
  void Unregister();
  //Removes this event listener from observer and adds the element back to the
  //list of incomplete XML Events declarations in XMLEventsManager
  void SetIncomplete();
  bool ObserverEquals(nsIContent * aTarget);
  bool HandlerEquals(nsIContent * aTarget);
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
private:
  nsXMLEventsManager * mManager;
  nsCOMPtr<nsIContent> mElement;
  nsCOMPtr<nsIContent> mObserver;
  nsCOMPtr<nsIContent> mHandler;
  nsString mEvent;
  nsCOMPtr<nsIAtom> mTarget;
  bool mPhase;
  bool mStopPropagation;
  bool mCancelDefault;
  
};

class nsXMLEventsManager MOZ_FINAL : public nsStubDocumentObserver {
public:
  nsXMLEventsManager();
  ~nsXMLEventsManager();
  NS_DECL_ISUPPORTS

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  void AddXMLEventsContent(nsIContent * aContent);
  void RemoveXMLEventsContent(nsIContent * aContent);
  void AddListener(nsIContent * aContent, nsXMLEventsListener * aListener);
  //Returns true if a listener was removed.
  bool RemoveListener(nsIContent * aXMLElement);
private:
  void AddListeners(nsIDocument* aDocument);
  nsInterfaceHashtable<nsISupportsHashKey,nsXMLEventsListener> mListeners;
  nsCOMArray<nsIContent> mIncomplete;
};

#endif /* nsXMLEventsManager_h___ */
