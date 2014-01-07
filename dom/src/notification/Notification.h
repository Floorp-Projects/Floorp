/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_notification_h__
#define mozilla_dom_notification_h__

#include "mozilla/dom/NotificationBinding.h"

#include "nsDOMEventTargetHelper.h"
#include "nsIObserver.h"

#include "nsCycleCollectionParticipant.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {


class NotificationObserver;
class Promise;

class Notification : public nsDOMEventTargetHelper
{
  friend class NotificationTask;
  friend class NotificationPermissionRequest;
  friend class NotificationObserver;
  friend class NotificationStorageCallback;

public:
  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(show)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)

  static already_AddRefed<Notification> Constructor(const GlobalObject& aGlobal,
                                                    const nsAString& aTitle,
                                                    const NotificationOptions& aOption,
                                                    ErrorResult& aRv);
  void GetID(nsAString& aRetval) {
    aRetval = mID;
  }

  void GetTitle(nsAString& aRetval)
  {
    aRetval = mTitle;
  }

  NotificationDirection Dir()
  {
    return mDir;
  }

  void GetLang(nsAString& aRetval)
  {
    aRetval = mLang;
  }

  void GetBody(nsAString& aRetval)
  {
    aRetval = mBody;
  }

  void GetTag(nsAString& aRetval)
  {
    aRetval = mTag;
  }

  void GetIcon(nsAString& aRetval)
  {
    aRetval = mIconUrl;
  }

  static void RequestPermission(const GlobalObject& aGlobal,
                                const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
                                ErrorResult& aRv);

  static NotificationPermission GetPermission(const GlobalObject& aGlobal,
                                              ErrorResult& aRv);

  static already_AddRefed<Promise> Get(const GlobalObject& aGlobal,
                                       const GetNotificationOptions& aFilter,
                                       ErrorResult& aRv);

  void Close();

  static bool PrefEnabled();

  nsPIDOMWindow* GetParentObject()
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
protected:
  Notification(const nsAString& aID, const nsAString& aTitle, const nsAString& aBody,
               NotificationDirection aDir, const nsAString& aLang,
               const nsAString& aTag, const nsAString& aIconUrl,
	       nsPIDOMWindow* aWindow);

  static already_AddRefed<Notification> CreateInternal(nsPIDOMWindow* aWindow,
                                                       const nsAString& aID,
                                                       const nsAString& aTitle,
                                                       const NotificationOptions& aOptions);

  void ShowInternal();
  void CloseInternal();

  static NotificationPermission GetPermissionInternal(nsISupports* aGlobal,
                                                      ErrorResult& rv);

  static const nsString DirectionToString(NotificationDirection aDirection)
  {
    switch (aDirection) {
    case NotificationDirection::Ltr:
      return NS_LITERAL_STRING("ltr");
    case NotificationDirection::Rtl:
      return NS_LITERAL_STRING("rtl");
    default:
      return NS_LITERAL_STRING("auto");
    }
  }

  static const NotificationDirection StringToDirection(const nsAString& aDirection)
  {
    if (aDirection.EqualsLiteral("ltr")) {
      return NotificationDirection::Ltr;
    }
    if (aDirection.EqualsLiteral("rtl")) {
      return NotificationDirection::Rtl;
    }
    return NotificationDirection::Auto;
  }

  static nsresult GetOrigin(nsPIDOMWindow* aWindow, nsString& aOrigin);

  nsresult GetAlertName(nsString& aAlertName);

  nsString mID;
  nsString mTitle;
  nsString mBody;
  NotificationDirection mDir;
  nsString mLang;
  nsString mTag;
  nsString mIconUrl;

  bool mIsClosed;

  static uint32_t sCount;

private:
  nsIPrincipal* GetPrincipal();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_notification_h__

