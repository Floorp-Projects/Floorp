/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULFrameElement_h__
#define XULFrameElement_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "js/TypeDecls.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsXULElement.h"

class nsIWebNavigation;
class nsFrameLoader;

namespace mozilla {
namespace dom {

class XULFrameElement final : public nsXULElement,
                              public nsIFrameLoaderOwner
{
public:
  explicit XULFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsXULElement(aNodeInfo)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULFrameElement, nsXULElement)

  // XULFrameElement.webidl
  nsIDocShell* GetDocShell();
  already_AddRefed<nsIWebNavigation> GetWebNavigation();
  already_AddRefed<nsPIDOMWindowOuter> GetContentWindow();
  nsIDocument* GetContentDocument();

  // nsIFrameLoaderOwner / MozFrameLoaderOwner
  NS_IMETHOD_(already_AddRefed<nsFrameLoader>) GetFrameLoader() override
  {
    return do_AddRef(mFrameLoader);
  }

  NS_IMETHOD_(void) InternalSetFrameLoader(nsFrameLoader* aFrameLoader) override
  {
    mFrameLoader = aFrameLoader;
  }

  void PresetOpenerWindow(mozIDOMWindowProxy* aWindow, ErrorResult& aRv)
  {
    mOpener = do_QueryInterface(aWindow);
  }

  void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);
  void SwapFrameLoaders(XULFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);
  void SwapFrameLoaders(nsIFrameLoaderOwner* aOtherLoaderOwner,
                        mozilla::ErrorResult& rv);

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;
  virtual void DestroyContent() override;


  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

protected:
  virtual ~XULFrameElement()
  {
  }

  RefPtr<nsFrameLoader> mFrameLoader;
  nsCOMPtr<nsPIDOMWindowOuter> mOpener;

  JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  void LoadSrc();
};

} // namespace dom
} // namespace mozilla

#endif // XULFrameElement_h
