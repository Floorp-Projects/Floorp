/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXSLContentSink.h"
#include "nsHTMLAtoms.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsILoadGroup.h"
#include "nsIParser.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsITransformMediator.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsEscape.h"

nsresult
NS_NewXSLContentSink(nsIXMLContentSink** aResult,
                     nsITransformMediator* aTM,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
                     nsIWebShell* aWebShell)
{
  NS_ENSURE_ARG(aResult);

  nsXSLContentSink* it;
  NS_NEWXPCOM(it, nsXSLContentSink);
  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIXMLContentSink> sink = it;
  nsresult rv = it->Init(aTM, aDoc, aURL, aWebShell);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(it, aResult);
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
  nsresult rv = nsXMLContentSink::Init(aDoc, aURL, aContainer, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mXSLTransformMediator = aTM;

  nsCOMPtr<nsIScriptLoader> loader;
  rv = mDocument->GetScriptLoader(getter_AddRefs(loader));
  NS_ENSURE_SUCCESS(rv, rv);

  loader->SetEnabled(PR_FALSE);
  loader->RemoveObserver(this);

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
  mDocument->SetRootContent(mDocElement);
  mDocument->EndLoad();

  nsCOMPtr<nsIDOMNode> styleNode;
  nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURL);
  if (url) {
      nsCAutoString ref;
      url->GetRef(ref);
      if (!ref.IsEmpty()) {
          NS_UnescapeURL(ref); // XXX this may result in non-ASCII octets
          nsCOMPtr<nsIDOMDocument> styleDoc = do_QueryInterface(mDocument);
          NS_ENSURE_TRUE(styleDoc, NS_ERROR_NO_INTERFACE);
          nsCOMPtr<nsIDOMElement> elem;
          styleDoc->GetElementById(NS_ConvertUTF8toUCS2(ref),
                                   getter_AddRefs(elem));
          styleNode = elem;
      }
      else {
          styleNode = do_QueryInterface(mDocument);
      }
  }
  else {
      styleNode = do_QueryInterface(mDocument);
  }

  if (mXSLTransformMediator) {
    // Pass the style content model to the tranform mediator.
    mXSLTransformMediator->SetStyleSheetContentModel(styleNode);
    mXSLTransformMediator = nsnull;
  }
  
  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

  return NS_OK;
}

// To stop infinite recursive processing of <?xml-stylesheet?>
NS_IMETHODIMP
nsXSLContentSink::ProcessStyleLink(nsIContent* aElement,
                                   const nsString& aHref,
                                   PRBool aAlternate,
                                   const nsString& aTitle,
                                   const nsString& aType,
                                   const nsString& aMedia)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsXSLContentSink::HandleStartElement(const PRUnichar *aName, 
                                     const PRUnichar **aAtts, 
                                     PRUint32 aAttsCount, 
                                     PRUint32 aIndex, 
                                     PRUint32 aLineNumber)
{
  nsresult rv = nsXMLContentSink::HandleStartElement(aName, aAtts,
                                                     aAttsCount, 
                                                     aIndex,
                                                     aLineNumber);

  nsCOMPtr<nsIContent> currentContent(dont_AddRef(GetCurrentContent()));

  if (currentContent &&
      currentContent->IsContentOfType(nsIContent::eHTML)) {
    nsCOMPtr<nsIAtom> tagAtom;
    currentContent->GetTag(*getter_AddRefs(tagAtom));
    if ((tagAtom == nsHTMLAtoms::link) ||
        (tagAtom == nsHTMLAtoms::style)) {
      nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
        do_QueryInterface(currentContent);
      if (ssle) {
        ssle->InitStyleLinkElement(nsnull, PR_TRUE);
      }
    }
  }
  return  rv;
}

NS_IMETHODIMP
nsXSLContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                              const PRUnichar *aData)
{
  FlushText();

  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node;

  nsresult rv = NS_NewXMLProcessingInstruction(getter_AddRefs(node),
                                               target, data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(node));
  if (ssle) {
    ssle->InitStyleLinkElement(nsnull, PR_TRUE);
  }

  rv = AddContentAsLeaf(node);
  return rv;
}

NS_IMETHODIMP
nsXSLContentSink::ReportError(const PRUnichar* aErrorText, 
                              const PRUnichar* aSourceText)
{
  // nsXMLContentSink::ReportError sets mXSLTransformMediator to nsnull
  nsCOMPtr<nsITransformMediator> mediator = mXSLTransformMediator;

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> loadGroup;
  mParser->GetChannel(getter_AddRefs(channel));
  mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  mDocument->Reset(channel, loadGroup);

  nsXMLContentSink::ReportError(aErrorText, aSourceText);

  if (mediator) {
    nsCOMPtr<nsIDOMNode> styleNode = do_QueryInterface(mDocElement);
    mediator->SetStyleInvalid(PR_TRUE);
    mediator->SetStyleSheetContentModel(styleNode);
  }
  return NS_OK;
}
