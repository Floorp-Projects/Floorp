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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#ifndef _nsXULFormControlAccessible_H_
#define _nsXULFormControlAccessible_H_

// NOTE: alphabetically ordered
#include "nsAccessibleWrap.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIDOMXULTextboxElement.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsHTMLFormControlAccessibleWrap.h"
#endif

class nsXULButtonAccessible : public nsAccessibleWrap
// Don't inherit from nsFormControlAccessible - it doesn't allow children and a button can have a dropmarker child
{
public:
  nsXULButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& aResult);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetChildCount(PRInt32 *_retval);
};

class nsXULCheckboxAccessible : public nsFormControlAccessible
{
public:
  nsXULCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetState(PRUint32 *_retval); 
};

class nsXULDropmarkerAccessible : public nsFormControlAccessible
{
public:
  nsXULDropmarkerAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD DoAction(PRUint8 index);

private:
  PRBool DropmarkerOpen(PRBool aToggleOpen);
};

class nsXULGroupboxAccessible : public nsAccessibleWrap
{
public:
  nsXULGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetName(nsAString& _retval);
};

class nsXULProgressMeterAccessible : public nsFormControlAccessible
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  nsXULProgressMeterAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetValue(nsAString &_retval);
};

class nsXULRadioButtonAccessible : public nsRadioButtonAccessible
{

public:
  nsXULRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetParent(nsIAccessible **_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval);
};

class nsXULRadioGroupAccessible : public nsAccessibleWrap
{
public:
  nsXULRadioGroupAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
};

class nsXULStatusBarAccessible : public nsAccessibleWrap
{
public:
  nsXULStatusBarAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
};

class nsXULToolbarAccessible : public nsAccessibleWrap
{
public:
  nsXULToolbarAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
};

class nsXULToolbarSeparatorAccessible : public nsLeafAccessible
{
public:
  nsXULToolbarSeparatorAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
};

class nsXULTextFieldAccessible : public nsHTMLTextFieldAccessibleWrap
{
public:
  nsXULTextFieldAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& aName) { return GetXULName(aName); }
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD GetState(PRUint32 *aState);
};


#endif  

