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

#include "nsXSLContentSink.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsITransformMediator.h"
#include "nsIParser.h"

nsresult
NS_NewXSLContentSink(nsIXMLContentSink** aResult,
                     nsITransformMediator* aTM,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
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
  nsresult rv = it->Init(aTM, aDoc, aURL, aWebShell);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(NS_GET_IID(nsIXMLContentSink), (void **)aResult);
}

nsXSLContentSink::nsXSLContentSink()
{
  // Empty
}

nsXSLContentSink::~nsXSLContentSink()
{
  // Empty
}


nsresult
nsXSLContentSink::Init(nsITransformMediator* aTM,
                       nsIDocument* aDoc,
                       nsIURI* aURL,
                       nsIWebShell* aContainer)
{
  nsresult rv;
  rv = nsXMLContentSink::Init(aDoc, aURL, aContainer);
  mXSLTransformMediator = aTM;

  return rv;
}

// nsIContentSink
NS_IMETHODIMP 
nsXSLContentSink::WillBuildModel(void)
{   
  return NS_OK;
}

NS_IMETHODIMP 
nsXSLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{  
  nsCOMPtr<nsIDOMNode> styleDoc;
  nsresult rv;

  mDocument->SetRootContent(mDocElement);
  styleDoc = do_QueryInterface(mDocument, &rv);
  if (NS_SUCCEEDED(rv) && mXSLTransformMediator) {
    // Pass the style content model to the tranform mediator.
    mXSLTransformMediator->SetStyleSheetContentModel(styleDoc);
  }
  
  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

  return NS_OK;
}
