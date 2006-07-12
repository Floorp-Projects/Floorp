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
 * The Initial Developers of the Original Code are
 * Sun Microsystems and IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ginn Chen (ginn.chen@sun.com)
 *   Aaron Leventhal (aleventh@us.ibm.com)
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

#ifndef _nsHyperTextAccessible_H_
#define _nsHyperTextAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleEditableText.h"
#include "nsAccessibleEventData.h"
#include "nsIEditActionListener.h"
#include "nsIEditor.h"
#include "nsFrameSelection.h"
#include "nsISelectionController.h"

enum EGetTextType { eGetBefore=-1, eGetAt=0, eGetAfter=1 };

// This character marks where in the text returned via nsIAccessibleText(),
// that embedded object characters exist
const PRUnichar kEmbeddedObjectChar = 0xfffc;
const PRUnichar kForcedNewLineChar = '\n';

/**
  * Special Accessible that knows how contain both text and embedded objects
  */
class nsHyperTextAccessible : public nsAccessibleWrap,
                              public nsIAccessibleText,
                              public nsIAccessibleHyperText,
                              public nsIAccessibleEditableText,
                              public nsIEditActionListener
{
public:
  nsHyperTextAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXT
  NS_DECL_NSIACCESSIBLEHYPERTEXT
  NS_DECL_NSIACCESSIBLEEDITABLETEXT
  NS_DECL_NSIEDITACTIONLISTENER

  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetExtState(PRUint32 *aState);

protected:
  PRBool IsHyperText();

  nsresult GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                         nsAString & aText);
  PRInt32 GetRelativeOffset(nsIPresShell *aPresShell, nsIFrame *aFromFrame, PRInt32 aFromOffset,
                            nsSelectionAmount amount, nsDirection direction, PRBool aNeedsStart);
  nsIFrame* GetPosAndText(PRInt32& aStartOffset, PRInt32& aEndOffset, nsAString *aText = nsnull, nsIFrame **aEndFrame = nsnull);

  // Editor helpers, subclasses of nsHyperTextAccessible may have editor
  virtual void SetEditor(nsIEditor *aEditor) { return; }
  virtual already_AddRefed<nsIEditor> GetEditor() { return nsnull; }

  // Selection helpers
  nsresult GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel);
  nsresult SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos);

  // Event helpers
  nsresult FireTextChangeEvent(AtkTextChange *aTextData);

  // Static helpers
  nsresult DOMPointToOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32 *aResult);
};
#endif  // _nsHyperTextAccessible_H_
