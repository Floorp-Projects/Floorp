/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* BarProps are the collection of little properties of DOM windows whose
   only property of their own is "visible".  They describe the window
   chrome which can be made visible or not through JavaScript by setting
   the appropriate property (window.menubar.visible)
*/

#ifndef mozilla_dom_BarProps_h
#define mozilla_dom_BarProps_h

#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowsingContext.h"

class nsGlobalWindowInner;
class nsIWebBrowserChrome;

namespace mozilla {

class ErrorResult;

namespace dom {

// Script "BarProp" object
class BarProp : public nsISupports, public nsWrapperCache {
 public:
  explicit BarProp(nsGlobalWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(BarProp)

  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  virtual bool GetVisible(CallerType aCallerType, ErrorResult& aRv) = 0;
  virtual void SetVisible(bool aVisible, CallerType aCallerType,
                          ErrorResult& aRv) = 0;

 protected:
  virtual ~BarProp();

  bool GetVisibleByIsPopup();
  bool GetVisibleByFlag(uint32_t aChromeFlag, CallerType aCallerType,
                        ErrorResult& aRv);
  void SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag,
                        CallerType aCallerType, ErrorResult& aRv);

  already_AddRefed<nsIWebBrowserChrome> GetBrowserChrome();

  BrowsingContext* GetBrowsingContext();

  RefPtr<nsGlobalWindowInner> mDOMWindow;
};

// Script "menubar" object
class MenubarProp final : public BarProp {
 public:
  explicit MenubarProp(nsGlobalWindowInner* aWindow);
  virtual ~MenubarProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

// Script "toolbar" object
class ToolbarProp final : public BarProp {
 public:
  explicit ToolbarProp(nsGlobalWindowInner* aWindow);
  virtual ~ToolbarProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

// Script "locationbar" object
class LocationbarProp final : public BarProp {
 public:
  explicit LocationbarProp(nsGlobalWindowInner* aWindow);
  virtual ~LocationbarProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

// Script "personalbar" object
class PersonalbarProp final : public BarProp {
 public:
  explicit PersonalbarProp(nsGlobalWindowInner* aWindow);
  virtual ~PersonalbarProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

// Script "statusbar" object
class StatusbarProp final : public BarProp {
 public:
  explicit StatusbarProp(nsGlobalWindowInner* aWindow);
  virtual ~StatusbarProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

// Script "scrollbars" object
class ScrollbarsProp final : public BarProp {
 public:
  explicit ScrollbarsProp(nsGlobalWindowInner* aWindow);
  virtual ~ScrollbarsProp();

  bool GetVisible(CallerType aCallerType, ErrorResult& aRv) override;
  void SetVisible(bool aVisible, CallerType aCallerType,
                  ErrorResult& aRv) override;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_BarProps_h */
