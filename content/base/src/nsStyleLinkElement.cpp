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

#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIDocument.h"
#include "nsIDOMComment.h"
#include "nsIDOMNode.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMText.h"
#include "nsIHTMLContentContainer.h"
#include "nsINameSpaceManager.h"
#include "nsIUnicharInputStream.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

nsStyleLinkElement::nsStyleLinkElement() :
    mDontLoadStyle(PR_FALSE),
    mUpdatesEnabled(PR_TRUE)
{
}

nsStyleLinkElement::~nsStyleLinkElement()
{
  nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(mStyleSheet);
  if (cssSheet) {
    cssSheet->SetOwningNode(nsnull);
  }
}

NS_IMETHODIMP 
nsStyleLinkElement::SetStyleSheet(nsIStyleSheet* aStyleSheet)
{
  nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(mStyleSheet);
  if (cssSheet) {
    cssSheet->SetOwningNode(nsnull);
  }

  mStyleSheet = aStyleSheet;
  cssSheet = do_QueryInterface(mStyleSheet);
  if (cssSheet) {
    nsCOMPtr<nsIDOMNode> node;
    CallQueryInterface(this,
                       NS_STATIC_CAST(nsIDOMNode**, getter_AddRefs(node)));
    if (node) {
      cssSheet->SetOwningNode(node);
    }
  }
    
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

NS_IMETHODIMP
nsStyleLinkElement::GetCharset(nsAString& aCharset)
{
  // descendants have to implement this themselves
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsStyleLinkElement::ParseLinkTypes(const nsAString& aTypes,
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

#ifdef ALLOW_ASYNCH_STYLE_SHEETS
const PRBool kBlockByDefault=PR_FALSE;
#else
const PRBool kBlockByDefault=PR_TRUE;
#endif

NS_IMETHODIMP
nsStyleLinkElement::UpdateStyleSheet(nsIDocument *aOldDocument,
                                     nsICSSLoaderObserver* aObserver)
{
  if (mStyleSheet && aOldDocument) {
    // We're removing the link element from the document, unload the
    // stylesheet.  We want to do this even if updates are disabled, since
    // otherwise a sheet with a stale linking element pointer will be hanging
    // around -- not good!
    aOldDocument->RemoveStyleSheet(mStyleSheet);
    mStyleSheet = nsnull;
  }

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

  if (!doc) {
    return NS_OK;
  }

  nsAutoString url;
  PRBool isInline;

  GetStyleSheetURL(&isInline, url);

  nsCOMPtr<nsIDOMStyleSheet> styleSheet(do_QueryInterface(mStyleSheet));

  if (styleSheet && !isInline) {
    nsAutoString oldHref;

    styleSheet->GetHref(oldHref);
    if (oldHref.Equals(url)) {
      return NS_OK; // We already loaded this stylesheet
    }
  }

  if (mStyleSheet) {
    doc->RemoveStyleSheet(mStyleSheet);
    mStyleSheet = nsnull;
  }

  if (url.IsEmpty() && !isInline) {
    return NS_OK; // If href is empty and this is not inline style then just bail
  }

  nsAutoString title, type, media;
  PRBool isAlternate;

  GetStyleSheetInfo(title, type, media, &isAlternate);

  if (!type.EqualsIgnoreCase("text/css")) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_OK;

  if (!isInline) {
    rv = NS_NewURI(getter_AddRefs(uri), url);

    if (NS_FAILED(rv)) {
      return NS_OK; // The URL is bad, move along, don't propagate the error (for now)
    }
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

  if (!isAlternate && !title.IsEmpty()) {  // possibly preferred sheet
    nsAutoString prefStyle;
    doc->GetHeaderData(nsHTMLAtoms::headerDefaultStyle, prefStyle);

    if (prefStyle.IsEmpty()) {
      doc->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, title);
    }
  }

  PRBool doneLoading;
  if (isInline) {
    PRInt32 count;
    thisContent->ChildCount(count);
    if (count < 0)
      return NS_OK;

    nsString *content = new nsString();
    NS_ENSURE_TRUE(content, NS_ERROR_OUT_OF_MEMORY);

    PRInt32 i;
    nsCOMPtr<nsIContent> node;
    for (i = 0; i < count; ++i) {
      thisContent->ChildAt(i, *getter_AddRefs(node));
      nsCOMPtr<nsIDOMText> tc = do_QueryInterface(node);
      // Ignore nodes that are not DOMText.
      if (!tc) {
        nsCOMPtr<nsIDOMComment> comment = do_QueryInterface(node);
        if (comment)
          // Skip a comment
          continue;
        break;
      }

      nsAutoString tcString;
      tc->GetData(tcString);
      content->Append(tcString);
    }

    nsCOMPtr<nsIUnicharInputStream> uin;
    rv = NS_NewStringUnicharInputStream(getter_AddRefs(uin), content);
    if (NS_FAILED(rv)) {
      delete content;
      return rv;
    }

    // Now that we have a url and a unicode input stream, parse the
    // style sheet.
    rv = loader->LoadInlineStyle(thisContent, uin, title, media,
                                 kNameSpaceID_Unknown,
                                 ((blockParser) ? parser.get() : nsnull),
                                 doneLoading, aObserver);
  }
  else {
    rv = loader->LoadStyleLink(thisContent, uri, title, media,
                               kNameSpaceID_Unknown,
                               ((blockParser) ? parser.get() : nsnull),
                               doneLoading, aObserver);
  }

  if (NS_SUCCEEDED(rv) && blockParser && !doneLoading) {
    rv = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return rv;
}

