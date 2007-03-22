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

NS_IMETHODIMP
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
  PRUint32 role = Role(this);
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
nsAccessibleWrap::InvalidateChildren ()
{
  if (mNativeWrapper) {
    mozAccessible *object = mNativeWrapper->getNativeObject();
    [object invalidateChildren];
  }
  return nsAccessible::InvalidateChildren();
}

PRInt32
nsAccessibleWrap::GetUnignoredChildCount(PRBool aDeepCount)
{
  // if we're flat, we have no children.
  if (IsFlat())
    return 0;
  
  PRInt32 childCount = 0;
  GetChildCount(&childCount);
  
  nsCOMPtr<nsIAccessible> curAcc;
  
  while (NextChild(curAcc)) {
    nsAccessibleWrap *childWrap = NS_STATIC_CAST(nsAccessibleWrap*, (nsIAccessible*)curAcc.get());
    
    // if the current child is not ignored, count it.
    if (!childWrap->IsIgnored())
      ++childCount;
      
    // if it's flat, we don't care to inspect its children.
    if (childWrap->IsFlat())
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
  if (IsFlat())
    return;
  
  while (NextChild(curAcc)) {
    nsAccessibleWrap *childWrap = NS_STATIC_CAST(nsAccessibleWrap*, (nsIAccessible*)curAcc.get());
    if (childWrap->IsIgnored()) {
      // element is ignored, so try adding its children as substitutes, if it has any.
      if (!childWrap->IsFlat()) {
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
  nsAccessibleWrap *parentWrap = NS_STATIC_CAST(nsAccessibleWrap*, (nsIAccessible*)parent.get());
  if (!parentWrap)
    return nsnull;
    
  // recursively return the parent, until we find one that is not ignored.
  if (parentWrap->IsIgnored())
    return parentWrap->GetUnignoredParent();
  
  nsIAccessible *outValue = nsnull;
  NS_IF_ADDREF(outValue = parent.get());
  
  return outValue;
}
