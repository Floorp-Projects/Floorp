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

#include "nsStyleLinkElement.h"

#include "nsIContent.h"
#include "nsICSSLoader.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsINameSpaceManager.h"
#include "nsUnicharUtils.h"

#include "nsNetUtil.h"
#include "nsVoidArray.h"

nsStyleLinkElement::nsStyleLinkElement() :
    mDontLoadStyle(PR_FALSE),
    mUpdatesEnabled(PR_TRUE)
{
}

nsStyleLinkElement::~nsStyleLinkElement()
{
}

NS_IMETHODIMP 
nsStyleLinkElement::SetStyleSheet(nsIStyleSheet* aStyleSheet)
{
  mStyleSheet = aStyleSheet;

  return NS_OK;
}

NS_IMETHODIMP 
nsStyleLinkElement::GetStyleSheet(nsIStyleSheet*& aStyleSheet)
{
  aStyleSheet = mStyleSheet;
  NS_IF_ADDREF(aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP 
nsStyleLinkElement::InitStyleLinkElement(nsIParser* aParser,
                                         PRBool aDontLoadStyle)
{
  mParser = aParser;
  mDontLoadStyle = aDontLoadStyle;

  return NS_OK;
}

NS_IMETHODIMP
nsStyleLinkElement::GetSheet(nsIDOMStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);
  *aSheet = nsnull;

  if (mStyleSheet) {
    CallQueryInterface(mStyleSheet, aSheet);
  }

  // Always return NS_OK to avoid throwing JS exceptions if mStyleSheet 
  // is not a nsIDOMStyleSheet
  return NS_OK;
}

NS_IMETHODIMP
nsStyleLinkElement::SetEnableUpdates(PRBool aEnableUpdates)
{
  mUpdatesEnabled = aEnableUpdates;

  return NS_OK;
}

void nsStyleLinkElement::ParseLinkTypes(const nsAReadableString& aTypes,
                                        nsStringArray& aResult)
{
  nsReadingIterator<PRUnichar> current;
  nsReadingIterator<PRUnichar> done;

  aTypes.BeginReading(current);
  aTypes.EndReading(done);
  if (current == done)
    return;

  nsReadingIterator<PRUnichar> start;
  PRBool inString = !nsCRT::IsAsciiSpace(*current);
  nsAutoString subString;

  aTypes.BeginReading(start);
  while (current != done) {
    if (nsCRT::IsAsciiSpace(*current)) {
      if (inString) {
        subString = Substring(start, current);
        ToLowerCase(subString);
        aResult.AppendString(subString);
        inString = PR_FALSE;
      }
    }
    else {
      if (!inString) {
        start = current;
        inString = PR_TRUE;
      }
    }
    ++current;
  }
  if (inString) {
    subString = Substring(start, current);
    ToLowerCase(subString);
    aResult.AppendString(subString);
  }
}

void nsStyleLinkElement::SplitMimeType(const nsString& aValue, nsString& aType,
                                       nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
}

#ifdef ALLOW_ASYNCH_STYLE_SHEETS
const PRBool kBlockByDefault=PR_FALSE;
#else
const PRBool kBlockByDefault=PR_TRUE;
#endif

NS_IMETHODIMP
nsStyleLinkElement::UpdateStyleSheet(PRBool aNotify, nsIDocument *aOldDocument,
                                     PRInt32 aDocIndex)
{
  if (mDontLoadStyle || !mUpdatesEnabled) {
    return NS_OK;
  }

  // Keep a strong ref to the parser so it's still around when we pass it
  // to the CSS loader. Release strong ref in mParser so we don't hang on
  // to the parser once we start the load or if we fail to load the
  // stylesheet.
  nsCOMPtr<nsIParser> parser = mParser;
  mParser = nsnull;

  nsCOMPtr<nsIContent> thisContent;
  QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(thisContent));

  NS_ENSURE_TRUE(thisContent, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc;
  thisContent->GetDocument(*getter_AddRefs(doc));

  if (aNotify && mStyleSheet && !doc && aOldDocument) {
    // We're removing the link element from the document, unload the
    // stylesheet.
    aOldDocument->RemoveStyleSheet(mStyleSheet);
    mStyleSheet = nsnull;

    return NS_OK;
  }

  if (!doc) {
    return NS_OK;
  }

  nsAutoString url, title, type, media;
  PRBool isAlternate;

  GetStyleSheetInfo(url, title, type, media, &isAlternate);

  nsCOMPtr<nsIDOMStyleSheet> styleSheet(do_QueryInterface(mStyleSheet));

  if (mStyleSheet && url.IsEmpty()) {
    // Inline stylesheets have the document's URL as their URL internally
    nsCOMPtr<nsIURI> docURL, styleSheetURL;
    mStyleSheet->GetURL(*getter_AddRefs(styleSheetURL));
    doc->GetBaseURL(*getter_AddRefs(docURL));
    if (docURL && styleSheetURL) {
      PRBool inlineStyle;
      docURL->Equals(styleSheetURL, &inlineStyle);
      if (inlineStyle) {
        return NS_OK;
      }
    }
  }
  else if (styleSheet) {
    nsAutoString oldHref;

    styleSheet->GetHref(oldHref);
    if (oldHref.Equals(url)) {
      return NS_OK;
    }
  }

  if (mStyleSheet) {
    doc->RemoveStyleSheet(mStyleSheet);
    mStyleSheet = nsnull;
  }

  if (url.IsEmpty()) {
    return NS_OK; // If href is empty then just bail
  }

  if (!type.EqualsIgnoreCase("text/css")) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);

  if (NS_FAILED(rv)) {
    return NS_OK; // The URL is bad, move along, don't propogate the error (for now)
  }

  nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
  nsCOMPtr<nsICSSLoader> loader;

  if (htmlContainer) {
    htmlContainer->GetCSSLoader(*getter_AddRefs(loader));
  }

  if (!loader) {
    return NS_OK;
  }

  PRBool blockParser = kBlockByDefault;
  if (isAlternate) {
    blockParser = PR_FALSE;
  }

  /* NOTE: no longer honoring the important keyword to indicate blocking
           as it is proprietary and unnecessary since all non-alternate 
           will block the parser now  -mja
    if (-1 != linkTypes.IndexOf("important")) {
      blockParser = PR_TRUE;
    }
  */

  // The way we determine the stylesheet's position in the cascade is by looking
  // at the first of the previous siblings that are style linking elements, and
  // insert just after that one. I'm not sure this is correct for every case for
  // XML documents (it seems to be all right for HTML). The sink should disable
  // this search by directly specifying a position.
  PRInt32 insertionPoint;

  if (aDocIndex > -1) {
    insertionPoint = aDocIndex;
  }
  else {
    // We're not getting them in document order, look for where to insert.
    nsCOMPtr<nsIDOMNode> parentNode;
    PRUint16 nodeType = 0;
    nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(thisContent));
    nsCOMPtr<nsIStyleSheet> prevSheet;

    thisNode->GetParentNode(getter_AddRefs(parentNode));
    if (parentNode)
      parentNode->GetNodeType(&nodeType);
    if (nodeType == nsIDOMNode::DOCUMENT_NODE) {
      nsCOMPtr<nsIDocument> parent(do_QueryInterface(parentNode));
      if (parent) {
        PRInt32 index;
        nsCOMPtr<nsIContent> prevNode;
        nsCOMPtr<nsIStyleSheetLinkingElement> previousLink;

        parent->IndexOf(thisContent, index);
        while (index-- > 0) {
          parent->ChildAt(index, *getter_AddRefs(prevNode));
          previousLink = do_QueryInterface(prevNode);
          if (previousLink) {
            previousLink->GetStyleSheet(*getter_AddRefs(prevSheet));
            if (prevSheet)
              // Found the first previous sibling that is a style linking element.
              break;
          }
        }
      }
    }
    else {
      nsCOMPtr<nsIContent> parent(do_QueryInterface(parentNode));
      if (parent) {
        PRInt32 index;
        nsCOMPtr<nsIContent> prevNode;
        nsCOMPtr<nsIStyleSheetLinkingElement> previousLink;

        parent->IndexOf(thisContent, index);
        while (index-- > 0) {
          parent->ChildAt(index, *getter_AddRefs(prevNode));
          previousLink = do_QueryInterface(prevNode);
          if (previousLink) {
            previousLink->GetStyleSheet(*getter_AddRefs(prevSheet));
            if (prevSheet)
              // Found the first previous sibling that is a style linking element.
              break;
          }
        }
      }
    }
    if (prevSheet) {
      PRInt32 sheetIndex = 0;
      doc->GetIndexOfStyleSheet(prevSheet, &sheetIndex);
      insertionPoint = sheetIndex + 1;
    }
    else {
      insertionPoint = 0;
    }
  }

  PRBool doneLoading;
  rv = loader->LoadStyleLink(thisContent, uri, title, media,
                             kNameSpaceID_Unknown,
                             insertionPoint, 
                             ((blockParser) ? parser.get() : nsnull),
                             doneLoading, 
                             nsnull);

  if (NS_SUCCEEDED(rv) && blockParser && !doneLoading) {
    rv = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return rv;
}

