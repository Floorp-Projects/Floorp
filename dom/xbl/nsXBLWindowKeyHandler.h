/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLWindowKeyHandler_h__
#define nsXBLWindowKeyHandler_h__

#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"

class nsIAtom;
class nsIDOMElement;
class nsIDOMKeyEvent;
class nsXBLSpecialDocInfo;
class nsXBLPrototypeHandler;

namespace mozilla {
namespace dom {
class Element;
class EventTarget;
struct IgnoreModifierState;
} // namespace dom
} // namespace mozilla

class nsXBLWindowKeyHandler : public nsIDOMEventListener
{
  typedef mozilla::dom::IgnoreModifierState IgnoreModifierState;

public:
  nsXBLWindowKeyHandler(nsIDOMElement* aElement, mozilla::dom::EventTarget* aTarget);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~nsXBLWindowKeyHandler();

  nsresult WalkHandlers(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType);

  // walk the handlers, looking for one to handle the event
  bool WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent,
                            nsIAtom* aEventType,
                            nsXBLPrototypeHandler* aHandler,
                            bool aExecute,
                            bool* aOutReservedForChrome = nullptr);

  // walk the handlers for aEvent, aCharCode and aIgnoreModifierState. Execute
  // it if aExecute = true.
  bool WalkHandlersAndExecute(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType,
                              nsXBLPrototypeHandler* aHandler,
                              uint32_t aCharCode,
                              const IgnoreModifierState& aIgnoreModifierState,
                              bool aExecute,
                              bool* aOutReservedForChrome = nullptr);

  // HandleEvent function for the capturing phase.
  void HandleEventOnCapture(nsIDOMKeyEvent* aEvent);

  // Check if any handler would handle the given event. Optionally returns
  // whether the command handler for the event is marked with the "reserved"
  // attribute.
  bool HasHandlerForEvent(nsIDOMKeyEvent* aEvent,
                          bool* aOutReservedForChrome = nullptr);

  // lazily load the handlers. Overridden to handle being attached
  // to a particular element rather than the document
  nsresult EnsureHandlers();

  // check if the given handler cares about the given key event
  bool EventMatched(nsXBLPrototypeHandler* aHandler, nsIAtom* aEventType,
                    nsIDOMKeyEvent* aEvent, uint32_t aCharCode,
                    const IgnoreModifierState& aIgnoreModifierState);

  // Is an HTML editable element focused
  bool IsHTMLEditableFieldFocused();

  // Returns the element which was passed as a parameter to the constructor,
  // unless the element has been removed from the document. Optionally returns
  // whether the disabled attribute is set on the element (assuming the element
  // is non-null).
  already_AddRefed<mozilla::dom::Element> GetElement(bool* aIsDisabled = nullptr);
  // Using weak pointer to the DOM Element.
  nsWeakPtr              mWeakPtrForElement;
  mozilla::dom::EventTarget* mTarget; // weak ref

  // these are not owning references; the prototype handlers are owned
  // by the prototype bindings which are owned by the docinfo.
  nsXBLPrototypeHandler* mHandler;     // platform bindings
  nsXBLPrototypeHandler* mUserHandler; // user-specific bindings

  // holds document info about bindings
  static nsXBLSpecialDocInfo* sXBLSpecialDocInfo;
  static uint32_t sRefCnt;
};

already_AddRefed<nsXBLWindowKeyHandler>
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement,
                          mozilla::dom::EventTarget* aTarget);

#endif
