/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParserNode.h"
#include "nsIParser.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
class nsIDocument;

// TODO
// - add in base tag support
// - get links from other sources:
//      - LINK tag
//      - STYLE SRC
//      - IMG SRC
//      - LAYER SRC

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIRobotSinkIID, NS_IROBOTSINK_IID);

class RobotSink : public nsIRobotSink {
public:
  RobotSink();
  ~RobotSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTMLContentSink
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD CloseTopmostContainer();
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD WillBuildModel(void) { return NS_OK; }
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel) { return NS_OK; }
  NS_IMETHOD WillInterrupt(void) { return NS_OK; }
  NS_IMETHOD WillResume(void) { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }

  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition){ return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition){ return NS_OK; }

  // nsIRobotSink
  NS_IMETHOD Init(nsIURL* aDocumentURL);
  NS_IMETHOD AddObserver(nsIRobotSinkObserver* aObserver);
  NS_IMETHOD RemoveObserver(nsIRobotSinkObserver* aObserver);

  void ProcessLink(const nsString& aLink);

protected:
  nsIURL* mDocumentURL;
  nsVoidArray mObservers;
};

nsresult NS_NewRobotSink(nsIRobotSink** aInstancePtrResult)
{
  RobotSink* it = new RobotSink();
  return it->QueryInterface(kIRobotSinkIID, (void**) aInstancePtrResult);
}

RobotSink::RobotSink()
{
}

RobotSink::~RobotSink()
{
  NS_IF_RELEASE(mDocumentURL);
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; i++) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    NS_RELEASE(cop);
  }
}  

NS_IMPL_ADDREF(RobotSink);

NS_IMPL_RELEASE(RobotSink);

NS_IMETHODIMP RobotSink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRobotSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP RobotSink::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenHead(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseHead(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenBody(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseBody(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenForm(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseForm(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenMap(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseMap(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenFrameset(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseFrameset(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::OpenContainer(const nsIParserNode& aNode)
{
  nsAutoString tmp(aNode.GetText());
  tmp.ToLowerCase();
  if (tmp.Equals("a")) {
    nsAutoString k, v;
    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; i++) {
      // Get upper-cased key
      const nsString& key = aNode.GetKeyAt(i);
      k.Truncate();
      k.Append(key);
      k.ToLowerCase();
      if (k.Equals("href")) {
        // Get value and remove mandatory quotes
        v.Truncate();
        v.Append(aNode.GetValueAt(i));
        PRUnichar first = v.First();
        if ((first == '"') || (first == '\'')) {
          if (v.Last() == first) {
            v.Cut(0, 1);
            PRInt32 pos = v.Length() - 1;
            if (pos >= 0) {
              v.Cut(pos, 1);
            }
          } else {
            // Mismatched quotes - leave them in
          }
        }
        ProcessLink(v);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseContainer(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::CloseTopmostContainer()
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddLeaf(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP RobotSink::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

/**
 * This gets called by the parsing system when we find a comment
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
NS_IMETHODIMP RobotSink::AddComment(const nsIParserNode& aNode) {
  nsresult result= NS_OK;
  return result;
}

/**
 * This gets called by the parsing system when we find a PI
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
NS_IMETHODIMP RobotSink::AddProcessingInstruction(const nsIParserNode& aNode) {
  nsresult result= NS_OK;
  return result;
}

NS_IMETHODIMP RobotSink::Init(nsIURL* aDocumentURL)
{
  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aDocumentURL;
  NS_IF_ADDREF(aDocumentURL);
  return NS_OK;
}

NS_IMETHODIMP RobotSink::AddObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.AppendElement(aObserver)) {
    NS_ADDREF(aObserver);
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP RobotSink::RemoveObserver(nsIRobotSinkObserver* aObserver)
{
  if (mObservers.RemoveElement(aObserver)) {
    NS_RELEASE(aObserver);
    return NS_OK;
  }
  //XXX return NS_ERROR_NOT_FOUND;
  return NS_OK;
}

void RobotSink::ProcessLink(const nsString& aLink)
{
  nsAutoString absURLSpec(aLink);

  // Make link absolute
  // XXX base tag handling
  nsIURL* docURL = mDocumentURL;
  if (nsnull != docURL) {
    nsIURL* absurl;
    nsresult rv = NS_NewURL(&absurl, aLink, docURL);
    if (NS_OK == rv) {
      absURLSpec.Truncate();
      PRUnichar* str;
      absurl->ToString(&str);
      absURLSpec = str;
      NS_RELEASE(absurl);
      delete str;
    }
  }

  // Now give link to robot observers
  PRInt32 i, n = mObservers.Count();
  for (i = 0; i < n; i++) {
    nsIRobotSinkObserver* cop = (nsIRobotSinkObserver*)mObservers.ElementAt(i);
    cop->ProcessLink(absURLSpec);
  }
}


NS_IMETHODIMP
RobotSink::DoFragment(PRBool aFlag) 
{
  return NS_OK; 
}

