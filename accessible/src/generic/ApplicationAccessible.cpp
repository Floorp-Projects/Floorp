/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "nsIComponentManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWindowMediator.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"
#include "nsIStringBundle.h"

using namespace mozilla::a11y;

ApplicationAccessible::ApplicationAccessible() :
  AccessibleWrap(nullptr, nullptr)
{
  mFlags |= eApplicationAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(ApplicationAccessible, Accessible,
                             nsIAccessibleApplication)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

NS_IMETHODIMP
ApplicationAccessible::GetParent(nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetNextSibling(nsIAccessible** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetPreviousSibling(nsIAccessible** aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nullptr;
  return NS_OK;
}

ENameValueFlag
ApplicationAccessible::Name(nsString& aName)
{
  aName.Truncate();

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();

  NS_ASSERTION(bundleService, "String bundle service must be present!");
  if (!bundleService)
    return eNameOK;

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                            getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return eNameOK;

  nsXPIDLString appName;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(appName));
  if (NS_FAILED(rv) || appName.IsEmpty()) {
    NS_WARNING("brandShortName not found, using default app name");
    appName.AssignLiteral("Gecko based application");
  }

  aName.Assign(appName);
  return eNameOK;
}

void
ApplicationAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();
}

void
ApplicationAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
}

PRUint64
ApplicationAccessible::State()
{
  return IsDefunct() ? states::DEFUNCT : 0;
}

NS_IMETHODIMP
ApplicationAccessible::GetAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;
  return NS_OK;
}

GroupPos
ApplicationAccessible::GroupPosition()
{
  return GroupPos();
}

Accessible*
ApplicationAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                    EWhichChildAtPoint aWhichChild)
{
  return nullptr;
}

Accessible*
ApplicationAccessible::FocusedChild()
{
  Accessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && focus->Parent() == this)
    return focus;

  return nullptr;
}

Relation
ApplicationAccessible::RelationByType(PRUint32 aRelationType)
{
  return Relation();
}

NS_IMETHODIMP
ApplicationAccessible::GetBounds(PRInt32* aX, PRInt32* aY,
                                 PRInt32* aWidth, PRInt32* aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::SetSelected(bool aIsSelected)
{
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::TakeSelection()
{
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::TakeFocus()
{
  return NS_OK;
}

PRUint8
ApplicationAccessible::ActionCount()
{
  return 0;
}

NS_IMETHODIMP
ApplicationAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
ApplicationAccessible::GetActionDescription(PRUint8 aIndex,
                                            nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
ApplicationAccessible::DoAction(PRUint8 aIndex)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleApplication

NS_IMETHODIMP
ApplicationAccessible::GetAppName(nsAString& aName)
{
  aName.Truncate();

  if (!mAppInfo)
    return NS_ERROR_FAILURE;

  nsCAutoString cname;
  nsresult rv = mAppInfo->GetName(cname);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF8toUTF16(cname, aName);
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetAppVersion(nsAString& aVersion)
{
  aVersion.Truncate();

  if (!mAppInfo)
    return NS_ERROR_FAILURE;

  nsCAutoString cversion;
  nsresult rv = mAppInfo->GetVersion(cversion);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF8toUTF16(cversion, aVersion);
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetPlatformName(nsAString& aName)
{
  aName.AssignLiteral("Gecko");
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetPlatformVersion(nsAString& aVersion)
{
  aVersion.Truncate();

  if (!mAppInfo)
    return NS_ERROR_FAILURE;

  nsCAutoString cversion;
  nsresult rv = mAppInfo->GetPlatformVersion(cversion);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF8toUTF16(cversion, aVersion);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public methods

void
ApplicationAccessible::Init()
{
  mAppInfo = do_GetService("@mozilla.org/xre/app-info;1");
}

void
ApplicationAccessible::Shutdown()
{
  mAppInfo = nullptr;
}

bool
ApplicationAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible public methods

void
ApplicationAccessible::ApplyARIAState(PRUint64* aState) const
{
}

role
ApplicationAccessible::NativeRole()
{
  return roles::APP_ROOT;
}

PRUint64
ApplicationAccessible::NativeState()
{
  return 0;
}

void
ApplicationAccessible::InvalidateChildren()
{
  // Do nothing because application children are kept updated by AppendChild()
  // and RemoveChild() method calls.
}

KeyBinding
ApplicationAccessible::AccessKey() const
{
  return KeyBinding();
}

////////////////////////////////////////////////////////////////////////////////
// Accessible protected methods

void
ApplicationAccessible::CacheChildren()
{
  // CacheChildren is called only once for application accessible when its
  // children are requested because empty InvalidateChldren() prevents its
  // repeated calls.

  // Basically children are kept updated by Append/RemoveChild method calls.
  // However if there are open windows before accessibility was started
  // then we need to make sure root accessibles for open windows are created so
  // that all root accessibles are stored in application accessible children
  // array.

  nsCOMPtr<nsIWindowMediator> windowMediator =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  nsresult rv = windowMediator->GetEnumerator(nullptr,
                                              getter_AddRefs(windowEnumerator));
  if (NS_FAILED(rv))
    return;

  bool hasMore = false;
  windowEnumerator->HasMoreElements(&hasMore);
  while (hasMore) {
    nsCOMPtr<nsISupports> window;
    windowEnumerator->GetNext(getter_AddRefs(window));
    nsCOMPtr<nsIDOMWindow> DOMWindow = do_QueryInterface(window);
    if (DOMWindow) {
      nsCOMPtr<nsIDOMDocument> DOMDocument;
      DOMWindow->GetDocument(getter_AddRefs(DOMDocument));
      if (DOMDocument) {
        nsCOMPtr<nsIDocument> docNode(do_QueryInterface(DOMDocument));
        GetAccService()->GetDocAccessible(docNode); // ensure creation
      }
    }
    windowEnumerator->HasMoreElements(&hasMore);
  }
}

Accessible*
ApplicationAccessible::GetSiblingAtOffset(PRInt32 aOffset,
                                          nsresult* aError) const
{
  if (aError)
    *aError = NS_OK; // fail peacefully

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

NS_IMETHODIMP
ApplicationAccessible::GetDOMNode(nsIDOMNode** aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetRootDocument(nsIAccessibleDocument** aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);
  *aRootDocument = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::ScrollTo(PRUint32 aScrollType)
{
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::ScrollToPoint(PRUint32 aCoordinateType,
                                     PRInt32 aX, PRInt32 aY)
{
  return NS_OK;
}

NS_IMETHODIMP
ApplicationAccessible::GetLanguage(nsAString& aLanguage)
{
  aLanguage.Truncate();
  return NS_OK;
}

