/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsMarkupDocument.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsHTMLValue.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"

// XXX TODO:

// 1. hookup the original stream to the image library directly so that we
//    don't have to load the image twice.

// 2. have a synthetic document url that we load that has patterns in it
//    that we replace with the image url (so that we can customize the
//    the presentation of the image): we could add script code to set the
//    width/height to provide scale buttons, we could show the image
//    attributes; etc.; should we have a seperate style sheet?

// 3. override the save-as methods so that we don't write out our synthetic
//    html

static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

class nsImageDocument : public nsMarkupDocument {
public:
  nsImageDocument();
  virtual ~nsImageDocument();

  NS_IMETHOD StartDocumentLoad(nsIURL* aUrl, 
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener** aDocListener);

  nsresult CreateSyntheticDocument();
  void StartLayout();

  nsIHTMLStyleSheet* mAttrStyleSheet;
};

class ImageListener : public nsIStreamListener {
public:
  ImageListener(nsImageDocument* aDoc);
  ~ImageListener();

  NS_DECL_ISUPPORTS
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus,
                           const nsString& aMsg);
  NS_IMETHOD GetBindInfo(nsIURL* aURL);
  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream* aStream,
                             PRInt32 aCount);

  nsImageDocument* mDocument;
};

//----------------------------------------------------------------------

ImageListener::ImageListener(nsImageDocument* aDoc)
{
  mDocument = aDoc;
  NS_ADDREF(aDoc);
  mRefCnt = 1;
}

ImageListener::~ImageListener()
{
  NS_RELEASE(mDocument);
}

NS_IMPL_ISUPPORTS(ImageListener, kIStreamListenerIID)

NS_IMETHODIMP
ImageListener::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  return NS_OK;
}

NS_IMETHODIMP
ImageListener::OnProgress(nsIURL* aURL, PRInt32 aProgress,
                          PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
ImageListener::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
ImageListener::OnStopBinding(nsIURL* aURL, PRInt32 aStatus,
                             const nsString& aMsg)
{
  mDocument->StartLayout();
  mDocument->CreateSyntheticDocument();
  return NS_OK;
}

NS_IMETHODIMP
ImageListener::GetBindInfo(nsIURL* aURL)
{
  return NS_OK;
}

NS_IMETHODIMP
ImageListener::OnDataAvailable(nsIURL* aURL, nsIInputStream* aStream,
                               PRInt32 aCount)
{
  for (;;) {
    PRInt32 len;
    char buf[1000];
    nsresult rv = aStream->Read(buf, 0, sizeof(buf), &len);
    if ((NS_OK != rv) || (0 == len)) {
      break;
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewImageDocument(nsIDocument** aInstancePtrResult)
{
  nsImageDocument* doc = new nsImageDocument();
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsImageDocument::nsImageDocument()
{
}

nsImageDocument::~nsImageDocument()
{
}

NS_IMETHODIMP
nsImageDocument::StartDocumentLoad(nsIURL* aURL, 
                                   nsIContentViewerContainer* aContainer,
                                   nsIStreamListener** aDocListener)
{
  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aURL;
  NS_IF_ADDREF(aURL);

  nsIHTMLCSSStyleSheet* styleAttrSheet;
  if (NS_OK == NS_NewHTMLCSSStyleSheet(&styleAttrSheet, aURL)) {
    AddStyleSheet(styleAttrSheet); // tell the world about our new style sheet
    NS_RELEASE(styleAttrSheet);
  }
  if (NS_OK == NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL)) {
    AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
  }

  *aDocListener = new ImageListener(this);

  return NS_OK;
}

nsresult
nsImageDocument::CreateSyntheticDocument()
{
  // XXX Set title to "GIF image widthxheight pixels - Netscape"

  // Wire up an image load request to the document
//  mImageRequest = aGroup->GetImage(cp, this, aBackgroundColor, 0, 0, 0);

  // Synthesize an html document that refers to the image
  nsresult rv;
  nsIHTMLContent* root;
  rv = NS_NewHTMLHtmlElement(&root, nsHTMLAtoms::html);
  if (NS_OK != rv) {
    return rv;
  }
  root->SetDocument(this);
  SetRootContent(root);

  nsIHTMLContent* body;
  rv = NS_NewHTMLBodyElement(&body, nsHTMLAtoms::body);
  if (NS_OK != rv) {
    return rv;
  }
  body->SetDocument(this);

  nsIHTMLContent* center;
  nsIAtom* centerAtom = NS_NewAtom("P");
  rv = NS_NewHTMLParagraphElement(&center, centerAtom);
  NS_RELEASE(centerAtom);
  if (NS_OK != rv) {
    return rv;
  }
  center->SetDocument(this);

  nsIHTMLContent* image;
  nsIAtom* imgAtom = NS_NewAtom("IMG");
  rv = NS_NewHTMLImageElement(&image, imgAtom);
  NS_RELEASE(imgAtom);
  if (NS_OK != rv) {
    return rv;
  }
  image->SetDocument(this);
  // XXX?
  nsAutoString src;
  mDocumentURL->ToString(src);
  nsHTMLValue val(src);
  image->SetAttribute(nsHTMLAtoms::src, val, PR_FALSE);
  image->SetAttribute(nsHTMLAtoms::alt, val, PR_FALSE);

  root->AppendChildTo(body, PR_FALSE);
  center->AppendChildTo(image, PR_FALSE);
  body->AppendChildTo(center, PR_FALSE);

  NS_RELEASE(image);
  NS_RELEASE(center);
  NS_RELEASE(body);
  NS_RELEASE(root);

  StartLayout();

  return NS_OK;
}

void
nsImageDocument::StartLayout()
{
  PRInt32 i, ns = GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = GetShellAt(i);
    if (nsnull != shell) {
      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsIPresContext* cx = shell->GetPresContext();
      nsRect r;
      cx->GetVisibleArea(r);
      shell->ResizeReflow(r.width, r.height);
      NS_RELEASE(cx);

      // Now trigger a refresh
      // XXX It's unfortunate that this has to be here
      nsIViewManager* vm = shell->GetViewManager();
      if (nsnull != vm) {
        vm->EnableRefresh();
        NS_RELEASE(vm);
      }

      NS_RELEASE(shell);
    }
  }
}
