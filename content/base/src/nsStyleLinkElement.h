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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */
#ifndef nsStyleLinkElement_h___
#define nsStyleLinkElement_h___

#include "nsCOMPtr.h"
#include "nsIDOMLinkStyle.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIStyleSheet.h"
#include "nsIParser.h"

class nsIDocument;
class nsStringArray;

class nsStyleLinkElement : public nsIDOMLinkStyle,
                           public nsIStyleSheetLinkingElement
{
public:
  nsStyleLinkElement();
  virtual ~nsStyleLinkElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) = 0;

  // nsIDOMLinkStyle
  NS_DECL_NSIDOMLINKSTYLE

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet);
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet);
  NS_IMETHOD InitStyleLinkElement(nsIParser *aParser, PRBool aDontLoadStyle);
  NS_IMETHOD UpdateStyleSheet(PRBool aNotify, nsIDocument *aOldDocument = nsnull, PRInt32 aDocIndex = -1);
  NS_IMETHOD SetEnableUpdates(PRBool aEnableUpdates);
  NS_IMETHOD GetCharset(nsAString& aCharset);

  static void ParseLinkTypes(const nsAReadableString& aTypes, nsStringArray& aResult);
  static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams);

protected:
  virtual void GetStyleSheetInfo(nsAWritableString& aUrl,
                                 nsAWritableString& aTitle,
                                 nsAWritableString& aType,
                                 nsAWritableString& aMedia,
                                 PRBool* aIsAlternate) = 0;


  nsCOMPtr<nsIStyleSheet> mStyleSheet;
  nsCOMPtr<nsIParser> mParser;
  PRPackedBool mDontLoadStyle;
  PRPackedBool mUpdatesEnabled;
};

#endif /* nsStyleLinkElement_h___ */

