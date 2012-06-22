/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsScreen_h___
#define nsScreen_h___

#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/Hal.h"
#include "nsIDOMScreen.h"
#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsCOMPtr.h"
#include "nsDOMEventTargetHelper.h"
#include "mozilla/Attributes.h"

class nsIDocShell;
class nsDeviceContext;
struct nsRect;

// Script "screen" object
class nsScreen : public nsDOMEventTargetHelper
               , public nsIDOMScreen
               , public mozilla::hal::ScreenConfigurationObserver
{
public:
  static already_AddRefed<nsScreen> Create(nsPIDOMWindow* aWindow);

  void Reset();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSCREEN
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsScreen,
                                           nsDOMEventTargetHelper)

  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration);

protected:
  nsDeviceContext* GetDeviceContext();
  nsresult GetRect(nsRect& aRect);
  nsresult GetAvailRect(nsRect& aRect);

  mozilla::dom::ScreenOrientation mOrientation;

private:
  class FullScreenEventListener MOZ_FINAL : public nsIDOMEventListener
  {
  public:
    FullScreenEventListener() {};

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER
  };

  nsScreen();
  virtual ~nsScreen();

  nsRefPtr<FullScreenEventListener> mEventListener;

  NS_DECL_EVENT_HANDLER(mozorientationchange)
};

#endif /* nsScreen_h___ */
