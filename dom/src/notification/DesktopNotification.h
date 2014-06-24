/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DesktopNotification_h
#define mozilla_dom_DesktopNotification_h

#include "nsIPrincipal.h"
#include "nsIAlertsService.h"
#include "nsIContentPermissionPrompt.h"

#include "nsIObserver.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMWindow.h"
#include "nsIScriptObjectPrincipal.h"

#include "nsIDOMEvent.h"
#include "nsIDocument.h"

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"


namespace mozilla {
namespace dom {

class AlertServiceObserver;
class DesktopNotification;

/*
 * DesktopNotificationCenter
 * Object hangs off of the navigator object and hands out DesktopNotification objects
 */
class DesktopNotificationCenter MOZ_FINAL : public nsISupports,
                                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DesktopNotificationCenter)

  DesktopNotificationCenter(nsPIDOMWindow *aWindow)
  {
    MOZ_ASSERT(aWindow);
    mOwner = aWindow;

    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
    MOZ_ASSERT(sop);

    mPrincipal = sop->GetPrincipal();
    MOZ_ASSERT(mPrincipal);

    SetIsDOMBinding();
  }

  void Shutdown() {
    mOwner = nullptr;
  }

  nsPIDOMWindow* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  already_AddRefed<DesktopNotification>
  CreateNotification(const nsAString& title,
                     const nsAString& description,
                     const nsAString& iconURL);

private:
  virtual ~DesktopNotificationCenter()
  {
  }

  nsCOMPtr<nsPIDOMWindow> mOwner;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

class DesktopNotificationRequest;

class DesktopNotification MOZ_FINAL : public DOMEventTargetHelper
{
  friend class DesktopNotificationRequest;

public:

  DesktopNotification(const nsAString& aTitle,
                      const nsAString& aDescription,
                      const nsAString& aIconURL,
                      nsPIDOMWindow *aWindow,
                      nsIPrincipal* principal);

  virtual ~DesktopNotification();

  void Init();

  /*
   * PostDesktopNotification
   * Uses alert service to display a notification
   */
  nsresult PostDesktopNotification();

  nsresult SetAllow(bool aAllow);

  /*
   * Creates and dispatches a dom event of type aName
   */
  void DispatchNotificationEvent(const nsString& aName);

  void HandleAlertServiceNotification(const char *aTopic);

  // WebIDL

  nsPIDOMWindow* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void Show(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(close)

protected:

  nsString mTitle;
  nsString mDescription;
  nsString mIconURL;

  nsRefPtr<AlertServiceObserver> mObserver;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mAllow;
  bool mShowHasBeenCalled;

  static uint32_t sCount;
};

class AlertServiceObserver: public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS

    AlertServiceObserver(DesktopNotification* notification)
    : mNotification(notification) {}

  void Disconnect() { mNotification = nullptr; }

  NS_IMETHODIMP
  Observe(nsISupports *aSubject,
          const char *aTopic,
          const char16_t *aData)
  {

    // forward to parent
    if (mNotification) {
#ifdef MOZ_B2G
    if (NS_FAILED(mNotification->CheckInnerWindowCorrectness()))
      return NS_ERROR_NOT_AVAILABLE;
#endif
      mNotification->HandleAlertServiceNotification(aTopic);
    }
    return NS_OK;
  };

 private:
  virtual ~AlertServiceObserver() {}

  DesktopNotification* mNotification;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DesktopNotification_h */
