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
#include "nsHTMLDocument.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIImageMap.h"

static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

NS_LAYOUT nsresult
NS_NewHTMLDocument(nsIDocument** aInstancePtrResult)
{
  nsHTMLDocument* doc = new nsHTMLDocument();
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsHTMLDocument::nsHTMLDocument()
  : nsDocument(),
    mAttrStyleSheet(nsnull)
{
  nsHTMLAtoms::AddrefAtoms();
}

nsHTMLDocument::~nsHTMLDocument()
{
  NS_IF_RELEASE(mAttrStyleSheet);
  nsHTMLAtoms::ReleaseAtoms();
}

NS_IMETHODIMP nsHTMLDocument::QueryInterface(REFNSIID aIID,
                                             void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIHTMLDocumentIID)) {
    AddRef();
    *aInstancePtr = (void**) &mIHTMLDocument;
    return NS_OK;
  }
  return nsDocument::QueryInterface(aIID, aInstancePtr);
}

void nsHTMLDocument::LoadURL(nsIURL* aURL)
{
  NS_IF_RELEASE(mAttrStyleSheet);
  NS_IF_RELEASE(mDocumentURL);
  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }

  mDocumentURL = aURL;
  NS_ADDREF(aURL);

  nsIParser* parser;
  nsresult rv = NS_NewHTMLParser(&parser);
  if (NS_OK == rv) {
    nsIHTMLContentSink* sink;
    rv = NS_NewHTMLContentSink(&sink, this, aURL);
    if (NS_OK == rv) {
      if (NS_OK == NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL)) {
        AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
      }

      parser->SetContentSink(sink);
      parser->Parse(aURL);
      NS_RELEASE(sink);
    }
    NS_RELEASE(parser);
  }
  //XXX return NS_OK;
}

static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENTOBSERVER_IID);

NS_IMETHODIMP nsHTMLDocument::SetTitle(const nsString& aTitle)
{
  if (nsnull == mDocumentTitle) {
    mDocumentTitle = new nsString(aTitle);
  }
  else {
    *mDocumentTitle = aTitle;
  }

  // Pass on title to observers
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->SetTitle(*mDocumentTitle);
  }

  // Pass on to any interested containers
  n = mPresShells.Count();
  for (i = 0; i < n; i++) {
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(i);
    nsIPresContext* cx = shell->GetPresContext();
    nsISupports* container;
    if (NS_OK == cx->GetContainer(&container)) {
      if (nsnull != container) {
        nsIDocumentObserver* docob;
        if (NS_OK == container->QueryInterface(kIDocumentObserverIID,
                                               (void**) &docob)) {
          docob->SetTitle(aTitle);
          NS_RELEASE(docob);
        }
        NS_RELEASE(container);
      }
    }
    NS_RELEASE(cx);
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLDocument::AddImageMap(nsIImageMap* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mImageMaps.AppendElement(aMap)) {
    NS_ADDREF(aMap);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsHTMLDocument::GetImageMap(const nsString& aMapName,
                                          nsIImageMap** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsAutoString name;
  PRInt32 i, n = mImageMaps.Count();
  for (i = 0; i < n; i++) {
    nsIImageMap* map = (nsIImageMap*) mImageMaps.ElementAt(i);
    if (NS_OK == map->GetName(name)) {
      if (name.EqualsIgnoreCase(aMapName)) {
        *aResult = map;
        NS_ADDREF(map);
        return NS_OK;
      }
    }
  }

  return 1;/* XXX NS_NOT_FOUND */
}

void nsHTMLDocument::AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet)
{
  if ((nsnull != mAttrStyleSheet) && (aSheet != mAttrStyleSheet)) {
    aSet->InsertDocStyleSheetBefore(aSheet, mAttrStyleSheet);
  }
  else {
    aSet->AppendDocStyleSheet(aSheet);
  }
}

//----------------------------------------------------------------------

// Aggregation class to give nsHTMLDocument the nsIHTMLDocument interface

#define GET_OUTER() \
 ((nsHTMLDocument*) ((char*)this - nsHTMLDocument::GetOuterOffset()))

nsHTMLDocument::AggIHTMLDocument::AggIHTMLDocument() {
  NS_INIT_REFCNT();
}

nsHTMLDocument::AggIHTMLDocument::~AggIHTMLDocument() { }

NS_IMETHODIMP_(nsrefcnt) nsHTMLDocument::AggIHTMLDocument::AddRef() {
  return GET_OUTER()->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) nsHTMLDocument::AggIHTMLDocument::Release() {
  return GET_OUTER()->Release();
}

NS_IMETHODIMP nsHTMLDocument::AggIHTMLDocument::QueryInterface(REFNSIID aIID,
                                                               void** aInstancePtr)
{
  return GET_OUTER()->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP nsHTMLDocument::AggIHTMLDocument::SetTitle(const nsString& aTitle) {
  return GET_OUTER()->SetTitle(aTitle);
}

NS_IMETHODIMP nsHTMLDocument::AggIHTMLDocument::AddImageMap(nsIImageMap* aMap) {
  return GET_OUTER()->AddImageMap(aMap);
}

NS_IMETHODIMP nsHTMLDocument::AggIHTMLDocument::GetImageMap(const nsString& aMapName,
                                            nsIImageMap** aResult) {
  return GET_OUTER()->GetImageMap(aMapName, aResult);
}

//----------------------------------------------------------------------

#ifdef NS_DEBUG

NS_LAYOUT void
NS_HackAppendContent(nsIDocument* aDoc)
{
  ((nsHTMLDocument*)aDoc)->HackAppendContent();
}

#include "nsIHTMLContent.h"
#include "nsIAtom.h"

static const char* kBigParagraph =
"This is some text. It will be word wrapped because our container will "
"word wrap us. It will also be baseline aligned because our container "
"will do that too. ";

#define NUM_BIG_TEXTS 114

nsresult nsHTMLDocument::HackAppendSimpleSpan(nsIContent* aContainer,
                                              const char* aTag,
                                              const char* aText)
{
  nsIHTMLContent* span;
  nsIHTMLContent* text;
  nsIAtom* atom = NS_NewAtom(aTag);
  nsresult rv = NS_NewHTMLContainer(&span, atom);
  if (NS_OK == rv) {
    nsAutoString tmp;
    for (PRIntn i = 0; i < NUM_BIG_TEXTS; i++) {
      tmp.Append(aText);
    }
    rv = NS_NewHTMLText(&text, tmp.GetUnicode(), tmp.Length());
    if (NS_OK == rv) {
      span->AppendChild(text);
      NS_RELEASE(text);
    }
    aContainer->AppendChild(span);
    NS_RELEASE(span);
  }
  NS_RELEASE(atom);
  return rv;
}

void nsHTMLDocument::HackAppendContent()
{
  nsIContent* body = mRootContent->ChildAt(0);
  HackAppendSimpleSpan(body, "P", kBigParagraph);
  NS_RELEASE(body);
}
#endif


