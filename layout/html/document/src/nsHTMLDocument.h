/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsIHTMLDocument.h"

class nsIHTMLStyleSheet;
class nsIViewDocument;
class nsIWebWidget;

class nsHTMLDocument : public nsDocument {
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD StartDocumentLoad(nsIURL* aUrl, 
                               nsIWebWidget* aWebWidget,
                               nsIStreamListener** aDocListener);

  NS_IMETHOD SetTitle(const nsString& aTitle);

  NS_IMETHOD AddImageMap(nsIImageMap* aMap);

  NS_IMETHOD GetImageMap(const nsString& aMapName, nsIImageMap** aResult);

  static PRInt32 GetOuterOffset() {
    return offsetof(nsHTMLDocument,mIHTMLDocument);
  }

  NS_IMETHOD CreateElement(nsString &aTagName, 
                           nsIDOMAttributeList *aAttributes, 
                           nsIDOMElement **aElement);
  NS_IMETHOD CreateTextNode(nsString &aData, nsIDOMText** aTextNode);

protected:
  virtual void AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet);

  class AggIHTMLDocument : public nsIHTMLDocument {
  public:
    AggIHTMLDocument();
    ~AggIHTMLDocument();

    NS_DECL_ISUPPORTS

    NS_IMETHOD SetTitle(const nsString& aTitle);

    NS_IMETHOD AddImageMap(nsIImageMap* aMap);

    NS_IMETHOD GetImageMap(const nsString& aMapName, nsIImageMap** aResult);
  };

  AggIHTMLDocument mIHTMLDocument;
  nsIHTMLStyleSheet* mAttrStyleSheet;
  nsVoidArray mImageMaps;
};

#endif /* nsHTMLDocument_h___ */
