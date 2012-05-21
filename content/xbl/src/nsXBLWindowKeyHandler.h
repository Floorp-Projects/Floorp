/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLWindowKeyHandler_h__
#define nsXBLWindowKeyHandler_h__

#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"

class nsIAtom;
class nsIDOMElement;
class nsIDOMEventTarget;
class nsIDOMKeyEvent;
class nsIDOMEventTarget;
class nsIXBLDocumentInfo;
class nsXBLSpecialDocInfo;
class nsXBLPrototypeHandler;

class nsXBLWindowKeyHandler : public nsIDOMEventListener
{
public:
  nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventTarget* aTarget);
  virtual ~nsXBLWindowKeyHandler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  // release globals
  static NS_HIDDEN_(void) ShutDown();

protected:
  nsresult WalkHandlers(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType);

  // walk the handlers, looking for one to handle the event
  nsresult WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent,
                                nsIAtom* aEventType, 
                                nsXBLPrototypeHandler* aHandler);

  // walk the handlers for aEvent, aCharCode and aIgnoreShiftKey
  bool WalkHandlersAndExecute(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType,
                                nsXBLPrototypeHandler* aHandler,
                                PRUint32 aCharCode, bool aIgnoreShiftKey);

  // lazily load the handlers. Overridden to handle being attached
  // to a particular element rather than the document
  nsresult EnsureHandlers(bool *aIsEditor);

  // check if the given handler cares about the given key event
  bool EventMatched(nsXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                      nsIDOMKeyEvent* inEvent, PRUint32 aCharCode,
                      bool aIgnoreShiftKey);

  // are we working with editor or browser?
  bool IsEditor() ;

  // Returns the element which was passed as a parameter to the constructor,
  // unless the element has been removed from the document.
  already_AddRefed<nsIDOMElement> GetElement();
  // Using weak pointer to the DOM Element.
  nsWeakPtr              mWeakPtrForElement;
  nsIDOMEventTarget*    mTarget; // weak ref

  // these are not owning references; the prototype handlers are owned
  // by the prototype bindings which are owned by the docinfo.
  nsXBLPrototypeHandler* mHandler;     // platform bindings
  nsXBLPrototypeHandler* mUserHandler; // user-specific bindings

  // holds document info about bindings
  static nsXBLSpecialDocInfo* sXBLSpecialDocInfo;
  static PRUint32 sRefCnt;
};

nsresult
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement,
                          nsIDOMEventTarget* aTarget,
                          nsXBLWindowKeyHandler** aResult);

#endif
