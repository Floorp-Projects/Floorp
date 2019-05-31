/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BoxObject_h__
#define mozilla_dom_BoxObject_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsPoint.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsRect.h"

class nsIFrame;

namespace mozilla {

class PresShell;

namespace dom {

class Element;

class BoxObject : public nsPIBoxObject, public nsWrapperCache {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BoxObject)
  NS_DECL_NSIBOXOBJECT

 public:
  BoxObject();

  // nsPIBoxObject
  virtual nsresult Init(Element* aElement) override;
  virtual void Clear() override;
  virtual void ClearCachedValues() override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult GetOffsetRect(nsIntRect& aRect);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult GetScreenPosition(nsIntPoint& aPoint);

  // Given a parent frame and a child frame, find the frame whose
  // next sibling is the given child frame and return its element
  static Element* GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame);

  // WebIDL (wraps old impls)
  Element* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  Element* GetElement() const;

  int32_t X();
  int32_t Y();
  int32_t GetScreenX(ErrorResult& aRv);
  int32_t GetScreenY(ErrorResult& aRv);
  int32_t Width();
  int32_t Height();

  already_AddRefed<nsISupports> GetPropertyAsSupports(
      const nsAString& propertyName);
  void SetPropertyAsSupports(const nsAString& propertyName, nsISupports* value);
  void GetProperty(const nsAString& propertyName, nsString& aRetVal,
                   ErrorResult& aRv);
  void SetProperty(const nsAString& propertyName,
                   const nsAString& propertyValue);
  void RemoveProperty(const nsAString& propertyName);

  Element* GetParentBox();
  Element* GetFirstChild();
  Element* GetLastChild();
  Element* GetNextSibling();
  Element* GetPreviousSibling();

 protected:
  virtual ~BoxObject();

  nsIFrame* GetFrame() const;
  MOZ_CAN_RUN_SCRIPT nsIFrame* GetFrameWithFlushPendingNotifications();
  PresShell* GetPresShell() const;
  MOZ_CAN_RUN_SCRIPT PresShell* GetPresShellWithFlushPendingNotifications();

  nsAutoPtr<nsInterfaceHashtable<nsStringHashKey, nsISupports>> mPropertyTable;

  Element* mContent;  // [WEAK]
};

}  // namespace dom
}  // namespace mozilla

#endif
