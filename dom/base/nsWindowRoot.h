/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowRoot_h__
#define nsWindowRoot_h__

class nsPIDOMWindow;
class nsIDOMEventListener;
class nsIDOMEvent;

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
} // namespace mozilla

#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "nsIDOMEventTarget.h"
#include "nsPIWindowRoot.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"

class nsWindowRoot : public nsPIWindowRoot
{
public:
  nsWindowRoot(nsPIDOMWindow* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET

  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const MOZ_OVERRIDE;
  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() MOZ_OVERRIDE;

  using mozilla::dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                mozilla::dom::EventListener* aListener,
                                bool aUseCapture,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  // nsPIWindowRoot

  virtual nsPIDOMWindow* GetWindow() MOZ_OVERRIDE;

  virtual nsresult GetControllers(nsIControllers** aResult) MOZ_OVERRIDE;
  virtual nsresult GetControllerForCommand(const char * aCommand,
                                           nsIController** _retval) MOZ_OVERRIDE;

  virtual nsIDOMNode* GetPopupNode() MOZ_OVERRIDE;
  virtual void SetPopupNode(nsIDOMNode* aNode) MOZ_OVERRIDE;

  virtual void SetParentTarget(mozilla::dom::EventTarget* aTarget) MOZ_OVERRIDE
  {
    mParent = aTarget;
  }
  virtual mozilla::dom::EventTarget* GetParentTarget() MOZ_OVERRIDE { return mParent; }
  virtual nsIDOMWindow* GetOwnerGlobal() MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsWindowRoot,
                                                         nsIDOMEventTarget)

protected:
  virtual ~nsWindowRoot();

  // Members
  nsCOMPtr<nsPIDOMWindow> mWindow;
  // We own the manager, which owns event listeners attached to us.
  nsRefPtr<mozilla::EventListenerManager> mListenerManager; // [Strong]
  nsCOMPtr<nsIDOMNode> mPopupNode; // [OWNER]

  nsCOMPtr<mozilla::dom::EventTarget> mParent;
};

extern already_AddRefed<mozilla::dom::EventTarget>
NS_NewWindowRoot(nsPIDOMWindow* aWindow);

#endif
