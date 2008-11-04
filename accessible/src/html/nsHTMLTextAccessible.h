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
#include "nsAutoPtr.h"
#include "nsBaseWidgetAccessible.h"

class nsIWeakReference;

class nsHTMLTextAccessible : public nsTextAccessibleWrap
{
public:
  nsHTMLTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  
  // nsIAccessible
  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetRole(PRUint32 *aRole);

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

class nsHTMLHRAccessible : public nsLeafAccessible
{
public:
  nsHTMLHRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
  NS_IMETHOD GetRole(PRUint32 *aRole); 
};

class nsHTMLBRAccessible : public nsLeafAccessible
{
public:
  nsHTMLBRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

class nsHTMLLabelAccessible : public nsTextAccessible 
{
public:
  nsHTMLLabelAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *_retval); 
  NS_IMETHOD GetFirstChild(nsIAccessible **aFirstChild);
  NS_IMETHOD GetLastChild(nsIAccessible **aLastChild);
  NS_IMETHOD GetChildCount(PRInt32 *aAccChildCount);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

class nsHTMLListBulletAccessible : public nsLeafAccessible
{
public:
  nsHTMLListBulletAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell,
                             const nsAString& aBulletText);

  // nsIAccessNode
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetName(nsAString& aName);

  // Don't cache via unique ID -- bullet accessible shares the same dom node as
  // this LI accessible. Also, don't cache via mParent/SetParent(), prevent
  // circular reference since li holds onto us.
  NS_IMETHOD SetParent(nsIAccessible *aParentAccessible);
  NS_IMETHOD GetParent(nsIAccessible **aParentAccessible);

  // nsAccessNode
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // nsPIAccessible
  NS_IMETHOD AppendTextTo(nsAString& aText, PRUint32 aStartOffset, PRUint32 aLength);

protected:
  // XXX: Ideally we'd get the bullet text directly from the bullet frame via
  // nsBulletFrame::GetListItemText(), but we'd need an interface for getting
  // text from contentless anonymous frames. Perhaps something like
  // nsIAnonymousFrame::GetText() ? However, in practice storing the bullet text
  // here should not be a problem if we invalidate the right parts of
  // the accessibility cache when mutation events occur.
  nsIAccessible *mWeakParent;
  nsString mBulletText;
};

class nsHTMLListAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLListAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
    nsHyperTextAccessibleWrap(aDOMNode, aShell) { }

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole) { *aRole = nsIAccessibleRole::ROLE_LIST; return NS_OK; }

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

class nsHTMLLIAccessible : public nsLinkableAccessible
{
public:
  nsHTMLLIAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell, 
                     const nsAString& aBulletText);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole) { *aRole = nsIAccessibleRole::ROLE_LISTITEM; return NS_OK; }
  NS_IMETHOD GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);

  // nsAccessNode
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

protected:
  void CacheChildren();  // Include bullet accessible

  nsRefPtr<nsHTMLListBulletAccessible> mBulletAccessible;
};

#endif  
