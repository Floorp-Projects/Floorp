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

#ifndef _nsHTMLTextAccessible_H_
#define _nsHTMLTextAccessible_H_

#include "nsTextAccessibleWrap.h"

class nsIWeakReference;

class nsHTMLTextAccessible : public nsTextAccessibleWrap
{
public:
  nsHTMLTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell, nsIFrame *aFrame);
  nsIFrame* GetFrame();
  NS_IMETHOD GetName(nsAString& _retval);
  NS_IMETHOD GetState(PRUint32 *aState);
private:
  nsIFrame *mFrame; // Only valid if node is not shut down (mWeakShell != null)
};

class nsHTMLHRAccessible : public nsLeafAccessible
{
public:
  nsHTMLHRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *aRole); 
  NS_IMETHOD GetState(PRUint32 *aState); 
};

class nsHTMLLabelAccessible : public nsTextAccessible 
{
public:
  nsHTMLLabelAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetName(nsAString& _retval);
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetState(PRUint32 *_retval); 
  NS_IMETHOD GetFirstChild(nsIAccessible **aFirstChild);
  NS_IMETHOD GetLastChild(nsIAccessible **aLastChild);
  NS_IMETHOD GetChildCount(PRInt32 *aAccChildCount);
};

class nsHTMLListAccessible : public nsAccessibleWrap
{
public:
  nsHTMLListAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
    nsAccessibleWrap(aDOMNode, aShell) { }
  NS_IMETHOD GetRole(PRUint32 *aRole) { *aRole = ROLE_LIST; return NS_OK; }
  NS_IMETHOD GetState(PRUint32 *aState) { nsAccessibleWrap::GetState(aState); *aState &= ~(STATE_FOCUSABLE | STATE_READONLY); return NS_OK; }
};

class nsHTMLLIAccessible : public nsAccessibleWrap
{
public:
  nsHTMLLIAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell, 
                     nsIFrame *aBulletFrame, const nsAString& aBulletText);
  NS_IMETHOD GetRole(PRUint32 *aRole) {* aRole = ROLE_LISTITEM; return NS_OK; }
  NS_IMETHOD GetState(PRUint32 *aState) { nsAccessibleWrap::GetState(aState); *aState &= ~(STATE_FOCUSABLE | STATE_READONLY); return NS_OK; }
  NS_IMETHOD GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  void CacheChildren(PRBool aWalkAnonContent);  // Include bullet accessible
protected:
  nsCOMPtr<nsIAccessible> mBulletAccessible;
};

class nsHTMLListBulletAccessible : public nsHTMLTextAccessible
{
public:
  nsHTMLListBulletAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell,
                             nsIFrame *aFrame, const nsAString& aBulletText);
  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetRole(PRUint32 *aRole) { *aRole = ROLE_STATICTEXT; return NS_OK; }
  NS_IMETHOD GetState(PRUint32 *aState) { nsHTMLTextAccessible::GetState(aState); *aState &= ~(STATE_FOCUSABLE | STATE_READONLY); return NS_OK; }
  // Don't cache via unique ID -- bullet accessible shares the same dom node as this LI accessible.
  // Also, don't cache via mParent/SetParent(), prevent circular reference since li holds onto us.
  NS_IMETHOD SetParent(nsIAccessible *aParentAccessible) { mParent = nsnull; return NS_OK; }
protected:
  // XXX Ideally we'd get the bullet text directly from the bullet frame via
  // nsBulletFrame::GetListItemText(), but we'd need an interface
  // for getting text from contentless anonymous frames.
  // Perhaps something like nsIAnonymousFrame::GetText() ?
  // However, in practice storing the bullet text here should not be a
  // problem if we invalidate the right parts of the accessibility cache
  // when mutation events occur. We'll also need to watch for mere style
  // changes -- perhaps we need to look for reflows and their reasons.
  nsString mBulletText;
};

#endif  
