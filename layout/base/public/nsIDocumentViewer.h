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
#ifndef nsIDocumentViewer_h___
#define nsIDocumentViewer_h___

#include "nsIContentViewer.h"

class nsIDocument;
class nsIPresContext;
class nsIPresShell;
class nsIStyleSheet;
class nsITransformMediator;

#define NS_IDOCUMENT_VIEWER_IID \
 { 0xa6cf9057, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * A document viewer is a kind of content viewer that uses NGLayout
 * to manage the presentation of the content.
 */
class nsIDocumentViewer : public nsIContentViewer
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_VIEWER_IID)

  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet) = 0;
  
  NS_IMETHOD GetDocument(nsIDocument*& aResult) = 0;
  
  NS_IMETHOD GetPresShell(nsIPresShell*& aResult) = 0;
  
  NS_IMETHOD GetPresContext(nsIPresContext*& aResult) = 0;

  NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                       nsIDocumentViewer*& aResult) = 0;

  NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator)=0;
};

#endif /* nsIDocumentViewer_h___ */
