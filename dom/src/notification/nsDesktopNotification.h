/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DesktopNotification.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@dougt.org>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDesktopNotification_h
#define nsDesktopNotification_h

#include "PCOMContentPermissionRequestChild.h"

#include "nsDOMClassInfoID.h"
#include "nsIPrincipal.h"
#include "nsIJSContextStack.h"

#include "nsIAlertsService.h"

#include "nsIDOMDesktopNotification.h"
#include "nsIDOMEventTarget.h"
#include "nsIContentPermissionPrompt.h"

#include "nsIObserver.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMWindow.h"
#include "nsThreadUtils.h"

#include "nsDOMEventTargetHelper.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDocument.h"

class AlertServiceObserver;

/*
 * nsDesktopNotificationCenter
 * Object hangs off of the navigator object and hands out nsDOMDesktopNotification objects
 */
class nsDesktopNotificationCenter : public nsIDOMDesktopNotificationCenter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDESKTOPNOTIFICATIONCENTER

  nsDesktopNotificationCenter(nsPIDOMWindow *aWindow)
  {
    mOwner = aWindow;

    // Grab the uri of the document
    nsCOMPtr<nsIDOMDocument> domdoc;
    mOwner->GetDocument(getter_AddRefs(domdoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    doc->NodePrincipal()->GetURI(getter_AddRefs(mURI));
  }

  virtual ~nsDesktopNotificationCenter()
  {
  }

  void Shutdown() {
    mOwner = nsnull;
  }

private:
  nsCOMPtr<nsPIDOMWindow> mOwner;
  nsCOMPtr<nsIURI> mURI;
};


class nsDOMDesktopNotification : public nsDOMEventTargetHelper,
                                 public nsIDOMDesktopNotification
{
  friend class nsDesktopNotificationRequest;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDesktopNotification,nsDOMEventTargetHelper)
  NS_DECL_NSIDOMDESKTOPNOTIFICATION

  nsDOMDesktopNotification(const nsAString & title,
                           const nsAString & description,
                           const nsAString & iconURL,
                           nsPIDOMWindow *aWindow,
                           nsIURI* uri);

  virtual ~nsDOMDesktopNotification();

  /*
   * PostDesktopNotification
   * Uses alert service to display a notification
   */
  void PostDesktopNotification();

  void SetAllow(bool aAllow);

  /*
   * Creates and dispatches a dom event of type aName
   */
  void DispatchNotificationEvent(const nsString& aName);

  void HandleAlertServiceNotification(const char *aTopic);

protected:

  nsString mTitle;
  nsString mDescription;
  nsString mIconURL;

  nsRefPtr<nsDOMEventListenerWrapper> mOnClickCallback;
  nsRefPtr<nsDOMEventListenerWrapper> mOnCloseCallback;

  nsRefPtr<AlertServiceObserver> mObserver;
  nsCOMPtr<nsIURI> mURI;
  bool mAllow;
  bool mShowHasBeenCalled;
};

/*
 * Simple Request
 */
class nsDesktopNotificationRequest : public nsIContentPermissionRequest,
                                     public nsRunnable, 
                                     public PCOMContentPermissionRequestChild

{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  nsDesktopNotificationRequest(nsDOMDesktopNotification* notification)
    : mDesktopNotification(notification) {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIContentPermissionPrompt> prompt =
      do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
    if (prompt) {
      prompt->Prompt(this);
    }
    return NS_OK;
  }

  ~nsDesktopNotificationRequest()
  {
  }

 bool Recv__delete__(const bool& allow)
 {
   if (allow)
     (void) Allow();
   else
     (void) Cancel();
   return true;
 }
 void IPDLRelease() { Release(); }

  nsRefPtr<nsDOMDesktopNotification> mDesktopNotification;
};

class AlertServiceObserver: public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
    
    AlertServiceObserver(nsDOMDesktopNotification* notification)
    : mNotification(notification) {}
  
  virtual ~AlertServiceObserver() {}

  void Disconnect() { mNotification = nsnull; }

  NS_IMETHODIMP
  Observe(nsISupports *aSubject,
          const char *aTopic,
          const PRUnichar *aData)
  {
    // forward to parent
    if (mNotification)
      mNotification->HandleAlertServiceNotification(aTopic);
    return NS_OK;
  };
  
 private:
  nsDOMDesktopNotification* mNotification;
};

#endif /* nsDesktopNotification_h */
