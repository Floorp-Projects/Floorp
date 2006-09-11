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

#import "mozAccessibleWrapper.h"
#import "mozAccessible.h"

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
  if (!mNativeWrapper)
    // create our native object using the class type specified in GetNativeType()
    mNativeWrapper = new AccessibleWrapper (this, GetNativeType());
  
  // need to pass the call up, so we're cached (which nsAccessNode::Init() takes care of).
  return nsAccessible::Init();
}

NS_IMETHODIMP
nsAccessibleWrap::GetNativeInterface (void **aOutInterface) 
{
  NS_ASSERTION (mNativeWrapper, "No native wrapper for this accessible!");
  if (mNativeWrapper)
    *aOutInterface = (void**)mNativeWrapper->getNativeObject();
  return NS_OK;
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
  return [mozAccessible class];
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