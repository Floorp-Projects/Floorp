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

#include "nsXSLContentSink.h"


static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);

nsresult
NS_NewXSLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURL* aURL,
                     nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXSLContentSink* it;
  NS_NEWXPCOM(it, nsXSLContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL, aWebShell);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIXMLContentSinkIID, (void **)aResult);
}

nsXSLContentSink::nsXSLContentSink()
{
  NS_INIT_REFCNT();
}

nsXSLContentSink::~nsXSLContentSink()
{

}

/*
nsresult
nsXSLContentSink::Init(nsIDocument* aDoc,
                       nsIURL* aURL,
                       nsIWebShell* aContainer)
{
  // We'll use nsXMLContentSink::Init() for now...
}
*/


// nsIContentSink
NS_IMETHODIMP 
nsXSLContentSink::WillBuildModel(void)
{   
  return NS_OK;
}

NS_IMETHODIMP 
nsXSLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{  
  return NS_OK;
}

/*
NS_IMETHODIMP 
nsXSLContentSink::WillInterrupt(void)
{
  // We'll use nsXMLContentSink::SetParser() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::WillResume(void)
{
  // We'll use nsXMLContentSink::SetParser() for now...
}
*/

/*
NS_IMETHODIMP
nsXSLContentSink::SetParser(nsIParser* aParser)
{
  // We'll use nsXMLContentSink::SetParser() for now...
}
*/

NS_IMETHODIMP 
nsXSLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  result = nsXMLContentSink::OpenContainer(aNode);
  return result;
}

NS_IMETHODIMP 
nsXSLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  result = nsXMLContentSink::CloseContainer(aNode);
  return result;
}

NS_IMETHODIMP 
nsXSLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  result = nsXMLContentSink::AddLeaf(aNode);
  return result;
}

NS_IMETHODIMP 
nsXSLContentSink::AddComment(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  result = nsXMLContentSink::AddComment(aNode);
  return result;
}
 
NS_IMETHODIMP 
nsXSLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXSLContentSink::NotifyError(nsresult aErrorResult)
{
  printf("nsXSLContentSink::NotifyError\n");
  return NS_OK;
}


// nsIXMLContentSink

/*
NS_IMETHODIMP 
nsXSLContentSink::AddXMLDecl(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddXMLDecl() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddDocTypeDecl() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::AddCharacterData(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddCharacterData() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::AddUnparsedEntity(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddUnparsedEntity() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::AddNotation(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddNotation() for now...
}
*/

/*
NS_IMETHODIMP 
nsXSLContentSink::AddEntityReference(const nsIParserNode& aNode)
{
  // We'll use nsXMLContentSink::AddEntityReference() for now...
}
*/

