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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
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

// This defines a common base class for nsITheme implementations, to reduce
// code duplication.

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsMargin.h"

class nsIFrame;
class nsIPresShell;
class nsPresContext;

class nsNativeTheme
{
 protected:
  nsNativeTheme();

  // Returns the content state (hover, focus, etc), see nsIEventStateManager.h
  PRInt32 GetContentState(nsIFrame* aFrame, PRUint8 aWidgetType);

  // Returns whether the widget is already styled by content
  // Normally called from ThemeSupportsWidget to turn off native theming
  // for elements that are already styled.
  PRBool IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                        PRUint8 aWidgetType);                                              

  // Accessors to widget-specific state information

  // all widgets:
  PRBool IsDisabled(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, mDisabledAtom);
  }

  // button:
  PRBool IsDefaultButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, mDefaultAtom);
  }

  // checkbox:
  PRBool IsChecked(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_FALSE);
  }

  // radiobutton:
  PRBool IsSelected(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_TRUE);
  }
  
  PRBool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, mFocusedAtom);
  }

  // tab:
  PRBool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, mSelectedAtom);
  }

  // treeheadercell:
  PRBool IsSortedColumn(nsIFrame* aFrame) {
    nsAutoString sortdir;
    if (GetAttr(aFrame, mSortDirectionAtom, sortdir))
      return !sortdir.IsEmpty();
    return PR_FALSE;
  }

  PRBool IsSortReversed(nsIFrame* aFrame) {
    nsAutoString sortdir;
    if (GetAttr(aFrame, mSortDirectionAtom, sortdir))
      return sortdir.EqualsLiteral("descending");
    return PR_FALSE;
  }

  // tab:
  PRBool IsBottomTab(nsIFrame* aFrame) {
    nsAutoString classStr;
    if (GetAttr(aFrame, mClassAtom, classStr))
      return classStr.Find("tab-bottom") != kNotFound;
    return PR_FALSE;
  }

  // progressbar:
  PRBool IsIndeterminateProgress(nsIFrame* aFrame) {
    nsAutoString mode;
    if (GetAttr(aFrame, mModeAtom, mode))
      return mode.EqualsLiteral("undetermined");
    return PR_FALSE;
  }

  PRInt32 GetProgressValue(nsIFrame* aFrame) {
    return CheckIntAttr(aFrame, mValueAtom);
  }

  // textfield:
  PRBool IsReadOnly(nsIFrame* aFrame) {
      return CheckBooleanAttr(aFrame, mReadOnlyAtom);
  }

  // These are used by nsNativeThemeGtk
  nsIPresShell *GetPresShell(nsIFrame* aFrame);
  PRInt32 CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom);
  PRBool CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom);

private:
  PRBool GetAttr(nsIFrame* aFrame, nsIAtom* aAtom, nsAString& attrValue);
  PRBool GetCheckedOrSelected(nsIFrame* aFrame, PRBool aCheckSelected);

protected:
  // these are available to subclasses because they are useful in
  // implementing WidgetStateChanged()
  nsCOMPtr<nsIAtom> mDisabledAtom;
  nsCOMPtr<nsIAtom> mCheckedAtom;
  nsCOMPtr<nsIAtom> mSelectedAtom;
  nsCOMPtr<nsIAtom> mReadOnlyAtom;
  nsCOMPtr<nsIAtom> mFirstTabAtom;
  nsCOMPtr<nsIAtom> mFocusedAtom;

  // these should be set to appropriate platform values by the subclass, to
  // match the values in platform-forms.css.  These defaults match forms.css
  static nsMargin sButtonBorderSize;
  static nsMargin sButtonDisabledBorderSize;
  static nsMargin sTextfieldBorderSize;
  static PRBool   sTextfieldBGTransparent;
  static nsMargin sListboxBorderSize;
  static PRBool   sListboxBGTransparent;

private:
  nsCOMPtr<nsIAtom> mDefaultAtom;
  nsCOMPtr<nsIAtom> mValueAtom;
  nsCOMPtr<nsIAtom> mModeAtom;
  nsCOMPtr<nsIAtom> mClassAtom;
  nsCOMPtr<nsIAtom> mSortDirectionAtom;
};
