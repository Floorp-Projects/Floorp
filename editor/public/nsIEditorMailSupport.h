/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIEditorMailSupport_h__
#define nsIEditorMailSupport_h__


#define NS_IEDITORMAILSUPPORT_IID                  \
{ /* {fdf23301-4a94-11d3-9ce4-9960496c41bc} */     \
0xfdf23301, 0x4a94, 0x11d3,                        \
{ 0x9c, 0xe4, 0x99, 0x60, 0x49, 0x6c, 0x41, 0xbc } }

class nsString;
class nsISupportsArray;


class nsIEditorMailSupport : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITORMAILSUPPORT_IID; return iid; }


  /** paste the text in the OS clipboard at the cursor position
    * as a quotation (whose representation is dependant on the editor type),
    * replacing the selected text (if any)
    * @param aCitation  The "mid" URL of the source message
    */
  NS_IMETHOD PasteAsQuotation(PRInt32 aSelectionType)=0;

  /** insert a string as quoted text,
    * as a quotation (whose representation is dependant on the editor type),
    * replacing the selected text (if any)
    * @param aQuotedText  The actual text to be quoted
    * @param aCitation    The "mid" URL of the source message
    */
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText,
                               nsIDOMNode** aNodeInserted)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation,
                                   PRInt32 aSelectionType)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText,
                                    const nsString& aCitation,
                                    PRBool aInsertHTML,
                                    const nsString& aCharset,
                                    nsIDOMNode** aNodeInserted)=0;

  /**
   * Document me!
   * 
   */
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList)=0;

};


#endif // nsIEditorMailSupport_h__
