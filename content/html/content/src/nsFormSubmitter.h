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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsFormSubmitter_h___
#define nsFormSubmitter_h___

#include "prtypes.h"
#include "nscore.h"
#include "nsError.h"

class nsIForm;
class nsIPresContext;
class nsIContent;
class nsIFormProcessor;
class nsIFileSpec;
class nsString;
class nsIPresContext;
class nsIUnicodeEncoder;
class nsIDOMNode;
class nsAString;

class nsFormSubmitter {

public:
  // JBK moved methods necessary for submit
  static nsresult OnSubmit(nsIForm* form,
                           nsIPresContext* aPresContext,
                           nsIContent* submitElement);

protected:
  static nsresult CompareNodes(nsIDOMNode* a,
                               nsIDOMNode* b,
                               PRInt32* retval);
  static nsresult ProcessAsURLEncoded(nsIForm* form,
                                      nsIPresContext* aPresContext,
                                      nsIFormProcessor* aFormProcessor,
                                      PRBool isPost,
                                      nsAString& aData,
                                      nsIContent* submitElement,
                                      PRInt32 aSubmitPosition,
                                      PRUint8 aCtrlsModAtSubmit,
                                      PRUint8 aTextDirAtSubmit);
  static nsresult ProcessAsMultipart(nsIForm* form,
                                     nsIPresContext* aPresContext,
                                     nsIFormProcessor* aFormProcessor,
                                     nsIFileSpec*& aMultipartDataFile,
                                     nsIContent* submitElement,
                                     PRInt32 aSubmitPosition,
                                     PRUint8 aCtrlsModAtSubmit,
                                     PRUint8 aTextDirAtSubmit);
  static void GetSubmitCharset(nsIForm* form,
                               nsAString& oCharset,
                               nsIPresContext*
                               aPresContext,
                               PRUint8 aCtrlsModAtSubmit);
  static nsresult GetEncoder(nsIForm* form,
                             nsIPresContext* aPresContext,
                             nsIUnicodeEncoder** encoder,
                             PRUint8 aCtrlsModAtSubmit,
                             const nsAString& mCharset);
  static nsString* URLEncode(const nsAString& aString,
                             nsIUnicodeEncoder* encoder,
                             PRUint8 aCtrlsModAtSubmit,
                             PRUint8 aTextDirAtSubmit,
                             const nsAString& mCharset);
  static char* UnicodeToNewBytes(const PRUnichar* aSrc,
                                 PRUint32 aLen,
                                 nsIUnicodeEncoder* encoder,
                                 PRUint8 aCtrlsModAtSubmit,
                                 PRUint8 aTextDirAtSubmit,
                                 const nsAString& mCharset);
  static nsresult GetPlatformEncoder(nsIUnicodeEncoder** encoder);
  static PRUint32 GetFileNameWithinPath(nsString& aPathName);
  static nsresult GetContentType(char* aPathName, char** aContentType);

  // Detection of first form to notify observers
  static PRBool gFirstFormSubmitted;
};

#endif
