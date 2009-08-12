/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Håkan Waara <hwaara@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAccessibleWrap.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleText.h"
#include "nsObjCExceptions.h"

#import "nsRoleMap.h"

#import "mozAccessibleWrapper.h"
#import "mozAccessible.h"
#import "mozActionElements.h"
#import "mozTextAccessible.h"

nsAccessibleWrap::nsAccessibleWrap(nsIDOMNode* aNode, nsIWeakReference *aShell): 
  nsAccessible(aNode, aShell),
  mNativeWrapper(nsnull)
{
}

nsAccessibleWrap::~nsAccessibleWrap()
{
  if (mNativeWrapper) {
    delete mNativeWrapper;
    mNativeWrapper = nsnull;
  }
}

nsresult
nsAccessibleWrap::Init () 
{
  // need to pass the call up, so we're cached (which nsAccessNode::Init() takes care of).
  nsresult rv = nsAccessible::Init();
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mNativeWrapper && !AncestorIsFlat()) {
    // Create our native object using the class type specified in GetNativeType().
    mNativeWrapper = new AccessibleWrapper (this, GetNativeType());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleWrap::GetNativeInterface (void **aOutInterface) 
{
  if (mNativeWrapper) {
    *aOutInterface = (void**)mNativeWrapper->getNativeObject();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// get the native NSWindow we reside in.
void
nsAccessibleWrap::GetNativeWindow (void **aOutNativeWindow)
{
  *aOutNativeWindow = nsnull;
  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  docAccessible->GetWindowHandle (aOutNativeWindow);
}

// overridden in subclasses to create the right kind of object. by default we create a generic
// 'mozAccessible' node.
objc_class*
nsAccessibleWrap::GetNativeType () 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  PRUint32 role = nsAccUtils::Role(this);
  switch (role) {
    case nsIAccessibleRole::ROLE_PUSHBUTTON:
    case nsIAccessibleRole::ROLE_SPLITBUTTON:
    case nsIAccessibleRole::ROLE_TOGGLE_BUTTON:
    {
      // if this button may show a popup, let's make it of the popupbutton type.
      if (HasPopup())
        return [mozPopupButtonAccessible class];
        
      // regular button
      return [mozButtonAccessible class];
    }
    
    case nsIAccessibleRole::ROLE_CHECKBUTTON:
      return [mozCheckboxAccessible class];
      
    case nsIAccessibleRole::ROLE_AUTOCOMPLETE:
      return [mozComboboxAccessible class];
      
    case nsIAccessibleRole::ROLE_ENTRY:
    case nsIAccessibleRole::ROLE_STATICTEXT:
    case nsIAccessibleRole::ROLE_HEADING:
    case nsIAccessibleRole::ROLE_LABEL:
    case nsIAccessibleRole::ROLE_CAPTION:
    case nsIAccessibleRole::ROLE_ACCEL_LABEL:
    case nsIAccessibleRole::ROLE_TEXT_LEAF:
      // normal textfield (static or editable)
      return [mozTextAccessible class]; 
      
    case nsIAccessibleRole::ROLE_COMBOBOX:
      return [mozPopupButtonAccessible class];
      
    default:
      return [mozAccessible class];
  }
  
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// this method is very important. it is fired when an accessible object "dies". after this point
// the object might still be around (because some 3rd party still has a ref to it), but it is
// in fact 'dead'.
nsresult
nsAccessibleWrap::Shutdown ()
{
  if (mNativeWrapper) {
    delete mNativeWrapper;
    mNativeWrapper = nsnull;
  }
  
  return nsAccessible::Shutdown();
}

nsresult
nsAccessibleWrap::FireAccessibleEvent(nsIAccessibleEvent *aEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(aEvent);

  nsresult rv = nsAccessible::FireAccessibleEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return FirePlatformEvent(aEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsAccessibleWrap::FirePlatformEvent(nsIAccessibleEvent *aEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PRUint32 eventType;
  nsresult rv = aEvent->GetEventType(&eventType);
  NS_ENSURE_SUCCESS(rv, rv);

  // ignore everything but focus-changed and value-changed events for now.
  if (eventType != nsIAccessibleEvent::EVENT_FOCUS &&
      eventType != nsIAccessibleEvent::EVENT_VALUE_CHANGE)
    return NS_OK;

  nsCOMPtr<nsIAccessible> accessible;
  rv = aEvent->GetAccessible(getter_AddRefs(accessible));
  NS_ENSURE_STATE(accessible);

  mozAccessible *nativeAcc = nil;
  accessible->GetNativeInterface((void**)&nativeAcc);
  if (!nativeAcc)
    return NS_ERROR_FAILURE;

  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      [nativeAcc didReceiveFocus];
      break;
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
      [nativeAcc valueDidChange];
      break;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
nsAccessibleWrap::InvalidateChildren()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mNativeWrapper) {
    mozAccessible *object = mNativeWrapper->getNativeObject();
    [object invalidateChildren];
  }
  nsAccessible::InvalidateChildren();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

PRInt32
nsAccessibleWrap::GetUnignoredChildCount(PRBool aDeepCount)
{
  // if we're flat, we have no children.
  if (nsAccUtils::MustPrune(this))
    return 0;
  
  PRInt32 childCount = 0;
  GetChildCount(&childCount);
  
  nsCOMPtr<nsIAccessible> curAcc;
  
  while (NextChild(curAcc)) {
    nsAccessibleWrap *childWrap = static_cast<nsAccessibleWrap*>(curAcc.get());
    
    // if the current child is not ignored, count it.
    if (!childWrap->IsIgnored())
      ++childCount;
      
    // if it's flat, we don't care to inspect its children.
    if (nsAccUtils::MustPrune(childWrap))
      continue;
    
    if (aDeepCount) {
      // recursively count the unignored children of our children since it's a deep count.
      childCount += childWrap->GetUnignoredChildCount(PR_TRUE);
    } else {
      // no deep counting, but if the child is ignored, we want to substitute it for its
      // children.
      if (childWrap->IsIgnored()) 
        childCount += childWrap->GetUnignoredChildCount(PR_FALSE);
    }
  } 
  
  return childCount;
}

// if we for some reason have no native accessible, we should be skipped over (and traversed)
// when fetching all unignored children, etc.  when counting unignored children, we will not be counted.
PRBool 
nsAccessibleWrap::IsIgnored() 
{
  return (mNativeWrapper == nsnull) || mNativeWrapper->isIgnored();
}

void
nsAccessibleWrap::GetUnignoredChildren(nsTArray<nsRefPtr<nsAccessibleWrap> > &aChildrenArray)
{
  nsCOMPtr<nsIAccessible> curAcc;
  
  // we're flat; there are no children.
  if (nsAccUtils::MustPrune(this))
    return;
  
  while (NextChild(curAcc)) {
    nsAccessibleWrap *childWrap = static_cast<nsAccessibleWrap*>(curAcc.get());
    if (childWrap->IsIgnored()) {
      // element is ignored, so try adding its children as substitutes, if it has any.
      if (!nsAccUtils::MustPrune(childWrap)) {
        nsTArray<nsRefPtr<nsAccessibleWrap> > children;
        childWrap->GetUnignoredChildren(children);
        if (!children.IsEmpty()) {
          // add the found unignored descendants to the array.
          aChildrenArray.AppendElements(children);
        }
      }
    } else
      // simply add the element, since it's not ignored.
      aChildrenArray.AppendElement(childWrap);
  }
}

already_AddRefed<nsIAccessible>
nsAccessibleWrap::GetUnignoredParent()
{
  nsCOMPtr<nsIAccessible> parent(GetParent());
  nsAccessibleWrap *parentWrap = static_cast<nsAccessibleWrap*>(parent.get());
  if (!parentWrap)
    return nsnull;
    
  // recursively return the parent, until we find one that is not ignored.
  if (parentWrap->IsIgnored())
    return parentWrap->GetUnignoredParent();
  
  nsIAccessible *outValue = nsnull;
  NS_IF_ADDREF(outValue = parent.get());
  
  return outValue;
}
