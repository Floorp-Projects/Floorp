/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMMutationEvent.h"
#include "nsXMLEventsManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIDOMEventListener.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

bool nsXMLEventsListener::InitXMLEventsListener(nsIDocument * aDocument,
                                                  nsXMLEventsManager * aManager,
                                                  nsIContent * aContent)
{
  nsresult rv;
  PRInt32 nameSpaceID;
  if (aContent->GetDocument() != aDocument)
    return false;
  if (aContent->NodeInfo()->Equals(nsGkAtoms::listener,
                                   kNameSpaceID_XMLEvents))
    nameSpaceID = kNameSpaceID_None;
  else
    nameSpaceID = kNameSpaceID_XMLEvents;
  nsAutoString eventType;
  aContent->GetAttr(nameSpaceID, nsGkAtoms::event, eventType);
  if (eventType.IsEmpty())
    return false;
  nsAutoString handlerURIStr;
  bool hasHandlerURI = false;
  nsIContent *handler = nsnull;
  nsAutoString observerID;
  nsAutoString targetIdref;
  
  if (aContent->GetAttr(nameSpaceID, nsGkAtoms::handler, handlerURIStr)) {
    hasHandlerURI = true;
    nsCAutoString handlerRef;
    nsCOMPtr<nsIURI> handlerURI;
    bool equals = false;
    nsIURI *docURI = aDocument->GetDocumentURI();
    nsIURI *baseURI = aDocument->GetDocBaseURI();
    rv = NS_NewURI( getter_AddRefs(handlerURI), handlerURIStr, nsnull, baseURI);
    if (NS_SUCCEEDED(rv)) {
      handlerURI->GetRef(handlerRef);
      // We support only XML Events Basic.
      rv = docURI->EqualsExceptRef(handlerURI, &equals);
      if (NS_SUCCEEDED(rv) && equals) {
        handler = aDocument->GetElementById(NS_ConvertUTF8toUTF16(handlerRef));
      }
    }
  }
  else
    handler = aContent;
  if (!handler)
    return false;

  aContent->GetAttr(nameSpaceID, nsGkAtoms::target, targetIdref);

  bool hasObserver = 
    aContent->GetAttr(nameSpaceID, nsGkAtoms::observer, observerID);

  bool capture =
    aContent->AttrValueIs(nameSpaceID, nsGkAtoms::phase,
                          nsGkAtoms::capture, eCaseMatters);

  bool stopPropagation = 
    aContent->AttrValueIs(nameSpaceID, nsGkAtoms::propagate,
                          nsGkAtoms::stop, eCaseMatters);

  bool cancelDefault = 
    aContent->AttrValueIs(nameSpaceID, nsGkAtoms::defaultAction,
                          nsGkAtoms::cancel, eCaseMatters);

  nsIContent *observer = nsnull;
  if (!hasObserver) {
    if (!hasHandlerURI) //Parent should be the observer
      observer = aContent->GetParent();
    else //We have the handler, so this is the observer
      observer = aContent;
  }
  else if (!observerID.IsEmpty()) {
    observer = aDocument->GetElementById(observerID);
  }
  nsCOMPtr<nsIDOMEventTarget> eventObserver(do_QueryInterface(observer));
  if (eventObserver) {
    nsXMLEventsListener * eli = new nsXMLEventsListener(aManager,
                                                        aContent,
                                                        observer,
                                                        handler,
                                                        eventType,
                                                        capture,
                                                        stopPropagation,
                                                        cancelDefault,
                                                        targetIdref);
    if (eli) {
      nsresult rv = eventObserver->AddEventListener(eventType, eli, capture);
      if (NS_SUCCEEDED(rv)) {
        aManager->RemoveXMLEventsContent(aContent);
        aManager->RemoveListener(aContent);
        aManager->AddListener(aContent, eli);
        return true;
      }
      else
        delete eli;
    }
  }
  return false;
}

nsXMLEventsListener::nsXMLEventsListener(nsXMLEventsManager * aManager,
                                         nsIContent * aElement,
                                         nsIContent * aObserver,
                                         nsIContent * aHandler,
                                         const nsAString& aEvent,
                                         bool aPhase,
                                         bool aStopPropagation,
                                         bool aCancelDefault,
                                         const nsAString& aTarget)
 : mManager(aManager),
   mElement(aElement),
   mObserver(aObserver),
   mHandler(aHandler),
   mEvent(aEvent),
   mPhase(aPhase),
   mStopPropagation(aStopPropagation),
   mCancelDefault(aCancelDefault)
{
  if (!aTarget.IsEmpty())
    mTarget = do_GetAtom(aTarget);
}

nsXMLEventsListener::~nsXMLEventsListener()
{
}

void nsXMLEventsListener::Unregister()
{
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mObserver);
  if (target) {
    target->RemoveEventListener(mEvent, this, mPhase);
  }
  mObserver = nsnull;
  mHandler = nsnull;
}

void nsXMLEventsListener::SetIncomplete()
{
  Unregister();
  mManager->AddXMLEventsContent(mElement);
  mElement = nsnull;
}

bool nsXMLEventsListener::ObserverEquals(nsIContent * aTarget)
{
  return aTarget == mObserver;
}

bool nsXMLEventsListener::HandlerEquals(nsIContent * aTarget)
{
  return aTarget == mHandler;
}

NS_IMPL_ISUPPORTS1(nsXMLEventsListener, nsIDOMEventListener)
NS_IMETHODIMP
nsXMLEventsListener::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!aEvent) 
    return NS_ERROR_INVALID_ARG;
  bool targetMatched = true;
  nsCOMPtr<nsIDOMEvent> event(aEvent);
  if (mTarget) {
    targetMatched = false;
    nsCOMPtr<nsIDOMEventTarget> target;
    aEvent->GetTarget(getter_AddRefs(target));
    nsCOMPtr<nsIContent> targetEl(do_QueryInterface(target));
    if (targetEl && targetEl->GetID() == mTarget) 
        targetMatched = true;
  }
  if (!targetMatched)
    return NS_OK;
  nsCOMPtr<nsIDOMEventListener> handler(do_QueryInterface(mHandler));
  if (handler) {
    nsresult rv = handler->HandleEvent(event);
    if (NS_SUCCEEDED(rv)) {
      if (mStopPropagation)
        event->StopPropagation();
      if (mCancelDefault)
        event->PreventDefault();
    }
    return rv;
  }
  return NS_OK;
}


//XMLEventsManager / DocumentObserver

static PLDHashOperator EnumAndUnregisterListener(nsISupports * aContent,
                                                 nsCOMPtr<nsXMLEventsListener> & aListener,
                                                 void * aData)
{
  if (aListener)
    aListener->Unregister();
  return PL_DHASH_NEXT;
}

static PLDHashOperator EnumAndSetIncomplete(nsISupports * aContent,
                                            nsCOMPtr<nsXMLEventsListener> & aListener,
                                            void * aData)
{
  if (aListener && aData) {
    nsCOMPtr<nsIContent> content = static_cast<nsIContent *>(aData);
    if (content) { 
      if (aListener->ObserverEquals(content) || aListener->HandlerEquals(content)) {
        aListener->SetIncomplete();
        return PL_DHASH_REMOVE;
      }
    }
  }
  return PL_DHASH_NEXT;
}

nsXMLEventsManager::nsXMLEventsManager()
{
  mListeners.Init();
}
nsXMLEventsManager::~nsXMLEventsManager()
{
}

NS_IMPL_ISUPPORTS2(nsXMLEventsManager, nsIDocumentObserver, nsIMutationObserver)

void nsXMLEventsManager::AddXMLEventsContent(nsIContent * aContent)
{
  mIncomplete.RemoveObject(aContent);
  mIncomplete.AppendObject(aContent);
}

void nsXMLEventsManager::RemoveXMLEventsContent(nsIContent * aContent)
{
  mIncomplete.RemoveObject(aContent);
}

void nsXMLEventsManager::AddListener(nsIContent * aContent, 
                                     nsXMLEventsListener * aListener)
{
  mListeners.Put(aContent, aListener);
}

bool nsXMLEventsManager::RemoveListener(nsIContent * aContent)
{
  nsCOMPtr<nsXMLEventsListener> listener;
  mListeners.Get(aContent, getter_AddRefs(listener));
  if (listener) {
    listener->Unregister();
    mListeners.Remove(aContent);
    return true;
  }
  return false;
}

void nsXMLEventsManager::AddListeners(nsIDocument* aDocument)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  nsIContent *cur;
  for (int i = 0; i < mIncomplete.Count(); ++i) {
    cur = mIncomplete[i];
    //If this succeeds, the object will be removed from mIncomplete
    if (nsXMLEventsListener::InitXMLEventsListener(aDocument, this, cur))
      --i;
  }
}

void 
nsXMLEventsManager::NodeWillBeDestroyed(const nsINode* aNode)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  mIncomplete.Clear();
  mListeners.Enumerate(EnumAndUnregisterListener, this);
  mListeners.Clear();
}

void 
nsXMLEventsManager::EndLoad(nsIDocument* aDocument)
{
  AddListeners(aDocument);
}

void
nsXMLEventsManager::AttributeChanged(nsIDocument* aDocument,
                                     Element* aElement,
                                     PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32 aModType)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (aNameSpaceID == kNameSpaceID_XMLEvents &&
      (aAttribute == nsGkAtoms::event ||
       aAttribute == nsGkAtoms::handler ||
       aAttribute == nsGkAtoms::target ||
       aAttribute == nsGkAtoms::observer ||
       aAttribute == nsGkAtoms::phase ||
       aAttribute == nsGkAtoms::propagate)) {
    RemoveListener(aElement);
    AddXMLEventsContent(aElement);
    nsXMLEventsListener::InitXMLEventsListener(aDocument, this, aElement);
  }
  else {
    if (aElement->NodeInfo()->Equals(nsGkAtoms::listener,
                                     kNameSpaceID_XMLEvents)) {
      RemoveListener(aElement);
      AddXMLEventsContent(aElement);
      nsXMLEventsListener::InitXMLEventsListener(aDocument, this, aElement);
    }
    else if (aElement->GetIDAttributeName() == aAttribute) {
      if (aModType == nsIDOMMutationEvent::REMOVAL)
        mListeners.Enumerate(EnumAndSetIncomplete, aElement);
      else if (aModType == nsIDOMMutationEvent::MODIFICATION) {
        //Remove possible listener
        mListeners.Enumerate(EnumAndSetIncomplete, aElement);
        //Add new listeners
        AddListeners(aDocument);
      }
      else {
        //If we are adding the ID attribute, we must check whether we can 
        //add new listeners
        AddListeners(aDocument);
      }
    }
  }
}

void
nsXMLEventsManager::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    PRInt32 aNewIndexInContainer)
{
  AddListeners(aDocument);
}

void
nsXMLEventsManager::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer)
{
  AddListeners(aDocument);
}

void
nsXMLEventsManager::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  if (!aChild || !aChild->IsElement())
    return;
  //Note, we can't use IDs here, the observer may not always have an ID.
  //And to remember: the same observer can be referenced by many 
  //XMLEventsListeners

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  //If the content was an XML Events observer or handler
  mListeners.Enumerate(EnumAndSetIncomplete, aChild);

  //If the content was an XML Events attributes container
  if (RemoveListener(aChild)) {
    //for aContainer.appendChild(aContainer.removeChild(aChild));
    AddXMLEventsContent(aChild);
  }

  PRUint32 count = aChild->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    ContentRemoved(aDocument, aChild, aChild->GetChildAt(i), i, aChild->GetPreviousSibling());
  }
}
