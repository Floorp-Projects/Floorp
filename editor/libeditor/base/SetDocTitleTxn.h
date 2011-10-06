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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef SetDocTitleTxn_h__
#define SetDocTitleTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITransaction.h"
#include "nsCOMPtr.h"

/**
 * A transaction that changes the document's title,
 *  which is a text node under the <title> tag in a page's <head> section
 * provides default concrete behavior for all nsITransaction methods.
 */
class SetDocTitleTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aValue  the new value for document title
    */
  NS_IMETHOD Init(nsIHTMLEditor  *aEditor,
                  const nsAString *aValue);
  SetDocTitleTxn();
private:
  nsresult SetDomTitle(const nsAString& aTitle);

public:
  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();
  NS_IMETHOD GetIsTransient(bool *aIsTransient);

protected:

  /** the editor that created this transaction */
  nsIHTMLEditor*  mEditor;
  
  /** The new title string */
  nsString    mValue;

  /** The previous title string to use for undo */
  nsString    mUndoValue;

  /** Set true if we dont' really change the title during Do() */
  bool mIsTransient;
};

#endif
