/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsAutoPtr.h"
#include "nsPIDOMWindow.h"

class nsGlobalWindow;
class nsIWebBrowserChrome;

namespace mozilla {

class ErrorResult;

namespace dom {

// Script "BarProp" object
class BarProp : public nsISupports,
                public nsWrapperCache
{
public:
  explicit BarProp(nsGlobalWindow *aWindow);
  virtual ~BarProp();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BarProp)

  nsPIDOMWindow* GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual bool GetVisible(ErrorResult& aRv) = 0;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) = 0;

protected:
  bool GetVisibleByFlag(uint32_t aChromeFlag, ErrorResult& aRv);
  void SetVisibleByFlag(bool aVisible, uint32_t aChromeFlag, ErrorResult &aRv);

  already_AddRefed<nsIWebBrowserChrome> GetBrowserChrome();

  nsRefPtr<nsGlobalWindow> mDOMWindow;
};

// Script "menubar" object
class MenubarProp MOZ_FINAL : public BarProp
{
public:
  explicit MenubarProp(nsGlobalWindow *aWindow);
  virtual ~MenubarProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

// Script "toolbar" object
class ToolbarProp MOZ_FINAL : public BarProp
{
public:
  explicit ToolbarProp(nsGlobalWindow *aWindow);
  virtual ~ToolbarProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

// Script "locationbar" object
class LocationbarProp MOZ_FINAL : public BarProp
{
public:
  explicit LocationbarProp(nsGlobalWindow *aWindow);
  virtual ~LocationbarProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

// Script "personalbar" object
class PersonalbarProp MOZ_FINAL : public BarProp
{
public:
  explicit PersonalbarProp(nsGlobalWindow *aWindow);
  virtual ~PersonalbarProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

// Script "statusbar" object
class StatusbarProp MOZ_FINAL : public BarProp
{
public:
  explicit StatusbarProp(nsGlobalWindow *aWindow);
  virtual ~StatusbarProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

// Script "scrollbars" object
class ScrollbarsProp MOZ_FINAL : public BarProp
{
public:
  explicit ScrollbarsProp(nsGlobalWindow *aWindow);
  virtual ~ScrollbarsProp();

  virtual bool GetVisible(ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void SetVisible(bool aVisible, ErrorResult& aRv) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_BarProps_h */

