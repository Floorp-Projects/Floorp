/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin <bolian.yin@sun.com>
 *   Ginn Chen <ginn.chen@sun.com>
 *   Alexander Surkov <surkov.alexander@gmail.com>
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
 
#include "nsApplicationAccessible.h"

#include "Relation.h"
#include "States.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"

#include "nsIComponentManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWindowMediator.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"

using namespace mozilla::a11y;

nsApplicationAccessible::nsApplicationAccessible() :
  nsAccessibleWrap(nsnull, nsnull)
{
  mFlags |= eApplicationAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsApplicationAccessible, nsAccessible,
                             nsIAccessibleApplication)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

NS_IMETHODIMP
nsApplicationAccessible::GetParent(nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetName(nsAString& aName)
{
  aName.Truncate();

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();

  NS_ASSERTION(bundleService, "String bundle service must be present!");
  NS_ENSURE_STATE(bundleService);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                            getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString appName;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(appName));
  if (NS_FAILED(rv) || appName.IsEmpty()) {
    NS_WARNING("brandShortName not found, using default app name");
    appName.AssignLiteral("Gecko based application");
  }

  aName.Assign(appName);
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetValue(nsAString &aValue)
{
  aValue.Truncate();
  return NS_OK;
}

void
nsApplicationAccessible::Description(nsString &aDescription)
{
  aDescription.Truncate();
}

PRUint64
nsApplicationAccessible::State()
{
  return IsDefunct() ? states::DEFUNCT : 0;
}

NS_IMETHODIMP
nsApplicationAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GroupPosition(PRInt32 *aGroupLevel,
                                       PRInt32 *aSimilarItemsInGroup,
                                       PRInt32 *aPositionInGroup)
{
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  *aGroupLevel = 0;
  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  *aSimilarItemsInGroup = 0;
  NS_ENSURE_ARG_POINTER(aPositionInGroup);
  *aPositionInGroup = 0;
  return NS_OK;
}

nsAccessible*
nsApplicationAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                      EWhichChildAtPoint aWhichChild)
{
  return nsnull;
}

nsAccessible*
nsApplicationAccessible::FocusedChild()
{
  nsAccessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && focus->Parent() == this)
    return focus;

  return nsnull;
}

Relation
nsApplicationAccessible::RelationByType(PRUint32 aRelationType)
{
  return Relation();
}

NS_IMETHODIMP
nsApplicationAccessible::GetBounds(PRInt32 *aX, PRInt32 *aY,
                                   PRInt32 *aWidth, PRInt32 *aHeight)
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
nsApplicationAccessible::SetSelected(PRBool aIsSelected)
{
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::TakeSelection()
{
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::TakeFocus()
{
  return NS_OK;
}

PRUint8
nsApplicationAccessible::ActionCount()
{
  return 0;
}

NS_IMETHODIMP
nsApplicationAccessible::GetActionName(PRUint8 aIndex, nsAString &aName)
{
  aName.Truncate();
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsApplicationAccessible::GetActionDescription(PRUint8 aIndex,
                                              nsAString &aDescription)
{
  aDescription.Truncate();
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsApplicationAccessible::DoAction(PRUint8 aIndex)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleApplication

NS_IMETHODIMP
nsApplicationAccessible::GetAppName(nsAString& aName)
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
nsApplicationAccessible::GetAppVersion(nsAString& aVersion)
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
nsApplicationAccessible::GetPlatformName(nsAString& aName)
{
  aName.AssignLiteral("Gecko");
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetPlatformVersion(nsAString& aVersion)
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

bool
nsApplicationAccessible::IsDefunct() const
{
  return nsAccessibilityService::IsShutdown();
}

PRBool
nsApplicationAccessible::Init()
{
  mAppInfo = do_GetService("@mozilla.org/xre/app-info;1");
  return PR_TRUE;
}

void
nsApplicationAccessible::Shutdown()
{
  mAppInfo = nsnull;
}

bool
nsApplicationAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible public methods

void
nsApplicationAccessible::ApplyARIAState(PRUint64* aState)
{
}

PRUint32
nsApplicationAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_APP_ROOT;
}

PRUint64
nsApplicationAccessible::NativeState()
{
  return 0;
}

void
nsApplicationAccessible::InvalidateChildren()
{
  // Do nothing because application children are kept updated by AppendChild()
  // and RemoveChild() method calls.
}

KeyBinding
nsApplicationAccessible::AccessKey() const
{
  return KeyBinding();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected methods

void
nsApplicationAccessible::CacheChildren()
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
  nsresult rv = windowMediator->GetEnumerator(nsnull,
                                              getter_AddRefs(windowEnumerator));
  if (NS_FAILED(rv))
    return;

  PRBool hasMore = PR_FALSE;
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

nsAccessible*
nsApplicationAccessible::GetSiblingAtOffset(PRInt32 aOffset,
                                            nsresult* aError) const
{
  if (aError)
    *aError = NS_OK; // fail peacefully

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessNode and nsAccessNode

NS_IMETHODIMP
nsApplicationAccessible::GetDOMNode(nsIDOMNode **aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetDocument(nsIAccessibleDocument **aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetRootDocument(nsIAccessibleDocument **aRootDocument)
{
  NS_ENSURE_ARG_POINTER(aRootDocument);
  *aRootDocument = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetInnerHTML(nsAString &aInnerHTML)
{
  aInnerHTML.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::ScrollTo(PRUint32 aScrollType)
{
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::ScrollToPoint(PRUint32 aCoordinateType,
                                       PRInt32 aX, PRInt32 aY)
{
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetComputedStyleValue(const nsAString &aPseudoElt,
                                               const nsAString &aPropertyName,
                                               nsAString &aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetComputedStyleCSSValue(const nsAString &aPseudoElt,
                                                  const nsAString &aPropertyName,
                                                  nsIDOMCSSPrimitiveValue **aCSSPrimitiveValue)
{
  NS_ENSURE_ARG_POINTER(aCSSPrimitiveValue);
  *aCSSPrimitiveValue = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationAccessible::GetLanguage(nsAString &aLanguage)
{
  aLanguage.Truncate();
  return NS_OK;
}

