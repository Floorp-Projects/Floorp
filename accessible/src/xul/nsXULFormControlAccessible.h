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
#include "nsFormControlAccessible.h"
#include "nsXULMenuAccessible.h"
#include "nsHyperTextAccessibleWrap.h"

/**
 * Used for XUL progressmeter element.
 */
typedef ProgressMeterAccessible<100> XULProgressMeterAccessible;

/**
 * Used for XUL button.
 *
 * @note  Don't inherit from nsFormControlAccessible - it doesn't allow children
 *         and a button can have a dropmarker child.
 */
class nsXULButtonAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Click = 0 };
  nsXULButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual nsAccessible* ContainerWidget() const;

protected:

  // nsAccessible
  virtual void CacheChildren();

  // nsXULButtonAccessible
  bool ContainsMenu();
};


/**
 * Used for XUL checkbox element.
 */
class nsXULCheckboxAccessible : public nsFormControlAccessible
{
public:
  enum { eAction_Click = 0 };
  nsXULCheckboxAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
};

/**
 * Used for XUL dropmarker element.
 */
class nsXULDropmarkerAccessible : public nsFormControlAccessible
{
public:
  enum { eAction_Click = 0 };
  nsXULDropmarkerAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

private:
  bool DropmarkerOpen(bool aToggleOpen);
};

/**
 * Used for XUL groupbox element.
 */
class nsXULGroupboxAccessible : public nsAccessibleWrap
{
public:
  nsXULGroupboxAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual Relation RelationByType(PRUint32 aRelationType);
};

/**
 * Used for XUL radio element (radio button).
 */
class nsXULRadioButtonAccessible : public nsRadioButtonAccessible
{

public:
  nsXULRadioButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual void GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                          PRInt32 *aSetSize);
  virtual PRUint64 NativeState();

  // Widgets
  virtual nsAccessible* ContainerWidget() const;
};

/**
 * Used for XUL radiogroup element.
 */
class nsXULRadioGroupAccessible : public nsXULSelectableAccessible
{
public:
  nsXULRadioGroupAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
};

/**
 * Used for XUL statusbar element.
 */
class nsXULStatusBarAccessible : public nsAccessibleWrap
{
public:
  nsXULStatusBarAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

/**
 * Used for XUL toolbarbutton element.
 */
class nsXULToolbarButtonAccessible : public nsXULButtonAccessible
{
public:
  nsXULToolbarButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual void GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                          PRInt32 *aSetSize);

  // nsXULToolbarButtonAccessible
  static bool IsSeparator(nsAccessible *aAccessible);
};

/**
 * Used for XUL toolbar element.
 */
class nsXULToolbarAccessible : public nsAccessibleWrap
{
public:
  nsXULToolbarAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetNameInternal(nsAString& aName);
};

/**
 * Used for XUL toolbarseparator element.
 */
class nsXULToolbarSeparatorAccessible : public nsLeafAccessible
{
public:
  nsXULToolbarSeparatorAccessible(nsIContent* aContent,
                                  nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();
};

/**
 * Used for XUL textbox element.
 */
class nsXULTextFieldAccessible : public nsHyperTextAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  nsXULTextFieldAccessible(nsIContent* aContent, nsIWeakReference *aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // nsIAccessibleEditableText
  NS_IMETHOD GetAssociatedEditor(nsIEditor **aEditor);

  // nsAccessible
  virtual void ApplyARIAState(PRUint64* aState);
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();
  virtual bool GetAllowsAnonChildAccessibles();

  // ActionAccessible
  virtual PRUint8 ActionCount();

protected:
  // nsAccessible
  virtual void CacheChildren();

  // nsXULTextFieldAccessible
  already_AddRefed<nsIContent> GetInputField() const;
};


#endif  

