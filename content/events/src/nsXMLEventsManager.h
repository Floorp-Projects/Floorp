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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

/*
 * The implementation of the XML Events Basic profile
 */

class nsXMLEventsManager;
class nsXMLEventsListener : public nsIDOMEventListener {
public:
  static PRBool InitXMLEventsListener(nsIDocument * aDocument, 
                                      nsXMLEventsManager * aManager, 
                                      nsIContent * aContent);
  nsXMLEventsListener(nsXMLEventsManager * aManager,
                      nsIContent * aElement,
                      nsIContent* aObserver,
                      nsIContent * aHandler,
                      const nsAString& aEvent,
                      PRBool aPhase,
                      PRBool aStopPropagation,
                      PRBool aCancelDefault,
                      const nsAString& aTarget);
  ~nsXMLEventsListener();
  void Unregister();
  //Removes this event listener from observer and adds the element back to the
  //list of incomplete XML Events declarations in XMLEventsManager
  void SetIncomplete();
  PRBool ObserverEquals(nsIContent * aTarget);
  PRBool HandlerEquals(nsIContent * aTarget);
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
private:
  nsXMLEventsManager * mManager;
  nsCOMPtr<nsIContent> mElement;
  nsCOMPtr<nsIContent> mObserver;
  nsCOMPtr<nsIContent> mHandler;
  nsString mEvent;
  nsCOMPtr<nsIAtom> mTarget;
  PRPackedBool mPhase;
  PRPackedBool mStopPropagation;
  PRPackedBool mCancelDefault;
  
};

class nsXMLEventsManager : public nsStubDocumentObserver {
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
  //Returns PR_TRUE if a listener was removed.
  PRBool RemoveListener(nsIContent * aXMLElement);
private:
  void AddListeners(nsIDocument* aDocument);
  nsInterfaceHashtable<nsISupportsHashKey,nsXMLEventsListener> mListeners;
  nsCOMArray<nsIContent> mIncomplete;
};

#endif /* nsXMLEventsManager_h___ */
