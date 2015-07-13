/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
class nsIPresShell;

namespace mozilla {
namespace dom {

class Element;

class BoxObject : public nsPIBoxObject,
                  public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BoxObject)
  NS_DECL_NSIBOXOBJECT

public:
  BoxObject();

  // nsPIBoxObject
  virtual nsresult Init(nsIContent* aContent) override;
  virtual void Clear() override;
  virtual void ClearCachedValues() override;

  nsIFrame* GetFrame(bool aFlushLayout);
  nsIPresShell* GetPresShell(bool aFlushLayout);
  nsresult GetOffsetRect(nsIntRect& aRect);
  nsresult GetScreenPosition(nsIntPoint& aPoint);

  // Given a parent frame and a child frame, find the frame whose
  // next sibling is the given child frame and return its element
  static nsresult GetPreviousSibling(nsIFrame* aParentFrame, nsIFrame* aFrame,
                                     nsIDOMElement** aResult);

  // WebIDL (wraps old impls)
  nsIContent* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  Element* GetElement() const;

  int32_t X();
  int32_t Y();
  int32_t GetScreenX(ErrorResult& aRv);
  int32_t GetScreenY(ErrorResult& aRv);
  int32_t Width();
  int32_t Height();

  already_AddRefed<nsISupports> GetPropertyAsSupports(const nsAString& propertyName);
  void SetPropertyAsSupports(const nsAString& propertyName, nsISupports* value);
  void GetProperty(const nsAString& propertyName, nsString& aRetVal, ErrorResult& aRv);
  void SetProperty(const nsAString& propertyName, const nsAString& propertyValue);
  void RemoveProperty(const nsAString& propertyName);

  already_AddRefed<Element> GetParentBox();
  already_AddRefed<Element> GetFirstChild();
  already_AddRefed<Element> GetLastChild();
  already_AddRefed<Element> GetNextSibling();
  already_AddRefed<Element> GetPreviousSibling();

protected:
  virtual ~BoxObject();

  nsAutoPtr<nsInterfaceHashtable<nsStringHashKey,nsISupports> > mPropertyTable; //[OWNER]

  nsIContent* mContent; // [WEAK]
};

} // namespace dom
} // namespace mozilla

#endif
