/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsITextContent_h___
#define nsITextContent_h___

#include "nsIContent.h"
class nsString;
class nsTextFragment;

// IID for the nsITextContent interface
#define NS_ITEXT_CONTENT_IID \
 {0xa6cf9065, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * Interface for textual content. This interface is used to provide
 * an efficient access to text content.
 */
class nsITextContent : public nsIContent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ITEXT_CONTENT_IID; return iid; }

  /**
   * Get direct access (but read only) to the text in the text content.
   */
  NS_IMETHOD GetText(const nsTextFragment** aFragmentsResult) = 0;

  /**
   * Get the length of the text content.
   */
  NS_IMETHOD GetTextLength(PRInt32* aLengthResult) = 0;

  /**
   * Make a copy of the text content in aResult.
   */
  NS_IMETHOD CopyText(nsAWritableString& aResult) = 0;

  /**
   * Set the text to the given value. If aNotify is PR_TRUE then
   * the document is notified of the content change.
   */
  NS_IMETHOD SetText(const PRUnichar* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify) = 0;

  /**
   * Set the text to the given value. If aNotify is PR_TRUE then
   * the document is notified of the content change.
   */
  NS_IMETHOD SetText(const nsAReadableString& aStr,
                     PRBool aNotify) = 0;

  /**
   * Set the text to the given value. If aNotify is PR_TRUE then
   * the document is notified of the content change.
   */
  NS_IMETHOD SetText(const char* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify) = 0;

  /**
   * Query method to see if the frame is nothing but whitespace
   */
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult) = 0;

  /**
   * Clone this content node. Unlike the nsIDOMNode equivalent, this
   * method allows you to specify whether to copy the text as well.
   */
  NS_IMETHOD CloneContent(PRBool aCloneText, nsITextContent** aClone) = 0;
};

//----------------------------------------------------------------------

/* a6cf905e-15b3-11d2-932e-00805f8add32 */
#define NS_ITEXT_CONTENT_CHANGE_DATA_IID \
 {0xa6cf905e, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Simple interface for encapsulating change data for a ContentChanged
// notification.
class nsITextContentChangeData : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITEXT_CONTENT_CHANGE_DATA_IID);

  enum ChangeType {
    Insert,
    Append,
    Replace
  };

  /**
   * Get the type of change associated with the ContentChanged
   * notification.
   */
  NS_IMETHOD GetChangeType(ChangeType* aResult) = 0;

  NS_IMETHOD GetInsertData(PRInt32* aOffset,
                           PRInt32* aInsertLength) = 0;

  NS_IMETHOD GetAppendData(PRInt32* aOffset,
                           PRInt32* aAppendLength) = 0;

  NS_IMETHOD GetReplaceData(PRInt32* aOffset,
                            PRInt32* aSourceLength,
                            PRInt32* aReplaceLength) = 0;
};

// XXX These belong elsewhere
extern nsresult
NS_NewTextNode(nsIContent** aResult);

extern nsresult
NS_NewCommentNode(nsIContent** aResult);


#endif /* nsITextContent_h___ */
