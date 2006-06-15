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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#ifndef _nsAccessibleText_H_
#define _nsAccessibleText_H_

#include "nsAccessibleEventData.h"
#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleEditableText.h"
#include "nsIAccessibleText.h"
#include "nsIEditActionListener.h"
#include "nsIEditor.h"
#include "nsISelectionController.h"
#include "nsITextControlFrame.h"
#include "nsTextAccessibleWrap.h"
#include "nsTextAccessible.h"

enum EGetTextType { eGetBefore=-1, eGetAt=0, eGetAfter=1 };

class nsAccessibleText : public nsIAccessibleText
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLETEXT

  nsAccessibleText(nsIDOMNode *aNode);
  virtual ~nsAccessibleText() {};

  static PRBool gSuppressedNotifySelectionChanged;

protected:
  nsCOMPtr<nsIDOMNode> mTextNode;

  virtual nsresult GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel);
  nsresult GetTextHelperCore(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                             PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                             nsISelectionController *aSelCon, nsISelection *aDomSel,
                             nsISupports *aClosure, nsAString & aText);
  nsresult GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                         nsISupports *aClosure, nsAString & aText);

  static nsresult DOMPointToOffset(nsISupports *aClosure, nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32 *aResult);
  static nsresult OffsetToDOMPoint(nsISupports *aClosure, PRInt32 aOffset, nsIDOMNode** aResult, PRInt32* aPosition);
  static nsresult GetCurrentOffset(nsISupports *aClosure, nsISelection *aDomSel, PRInt32 *aOffset);

  friend class nsAccessibleHyperText;
};

class nsAccessibleEditableText : public nsAccessibleText,
                                 public nsIAccessibleEditableText,
                                 public nsIEditActionListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEEDITABLETEXT
  NS_DECL_NSIEDITACTIONLISTENER

  nsAccessibleEditableText(nsIDOMNode *aNode);
  virtual ~nsAccessibleEditableText() {};

  NS_IMETHOD GetCaretOffset(PRInt32 *aCaretOffset);
  NS_IMETHOD SetCaretOffset(PRInt32 aCaretOffset);
  NS_IMETHOD GetCharacterCount(PRInt32 *aCharacterCount);
  NS_IMETHOD GetText(PRInt32 startOffset, PRInt32 endOffset, nsAString & aText);
  NS_IMETHOD GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                 PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText);
  NS_IMETHOD GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                             PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText);
  NS_IMETHOD GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText);

  void ShutdownEditor();

  static PRBool IsSingleLineTextControl(nsIDOMNode *aDomNode);

protected:
  virtual nsresult GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel);

  void SetEditor(nsIEditor *aEditor);
  nsITextControlFrame* GetTextFrame();
  nsresult GetSelectionRange(PRInt32 *aStartPos, PRInt32 *aEndPos);
  nsresult SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos);
  nsresult FireTextChangeEvent(AtkTextChange *aTextData);

  // To solve the naming conflict with nsDocAccessible::mEditor, we have to name it
  // "mPlainEditor", though it's not necessary to be a "plaintext" editor.
  nsCOMPtr<nsIEditor>  mPlainEditor;
};

class nsTextAccessibleWrap : public nsTextAccessible,
                             public nsAccessibleText
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsTextAccessibleWrap(nsIDOMNode* aDomNode, nsIWeakReference* aShell);
};

#endif

