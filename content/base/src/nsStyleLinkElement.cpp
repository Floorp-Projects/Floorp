/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIUnicharInputStream.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

nsStyleLinkElement::nsStyleLinkElement()
  : mDontLoadStyle(PR_FALSE)
  , mUpdatesEnabled(PR_TRUE)
  , mLineNumber(1)
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

/* virtual */ void
nsStyleLinkElement::SetLineNumber(PRUint32 aLineNumber)
{
  mLineNumber = aLineNumber;
}

void nsStyleLinkElement::ParseLinkTypes(const nsAString& aTypes,
                                        nsStringArray& aResult)
{
  nsAString::const_iterator start, done;
  aTypes.BeginReading(start);
  aTypes.EndReading(done);
  if (start == done)
    return;

  nsAString::const_iterator current(start);
  PRBool inString = !nsCRT::IsAsciiSpace(*current);
  nsAutoString subString;

  while (current != done) {
    if (nsCRT::IsAsciiSpace(*current)) {
      if (inString) {
        ToLowerCase(Substring(start, current), subString);
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
    ToLowerCase(Substring(start, current), subString);
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
    aOldDocument->BeginUpdate(UPDATE_STYLE);
    aOldDocument->RemoveStyleSheet(mStyleSheet);
    aOldDocument->EndUpdate(UPDATE_STYLE);
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

  nsCOMPtr<nsIDocument> doc = thisContent->GetDocument();

  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  PRBool isInline;
  GetStyleSheetURL(&isInline, getter_AddRefs(uri));

  if (mStyleSheet && !isInline && uri) {
    nsCOMPtr<nsIURI> oldURI;

    mStyleSheet->GetBaseURI(getter_AddRefs(oldURI));
    if (oldURI) {
      PRBool equal;
      nsresult rv = oldURI->Equals(uri, &equal);
      if (NS_SUCCEEDED(rv) && equal) {
        return NS_OK; // We already loaded this stylesheet
      }
    }
  }

  if (mStyleSheet) {
    doc->BeginUpdate(UPDATE_STYLE);
    doc->RemoveStyleSheet(mStyleSheet);
    doc->EndUpdate(UPDATE_STYLE);
    mStyleSheet = nsnull;
  }

  if (!uri && !isInline) {
    return NS_OK; // If href is empty and this is not inline style then just bail
  }

  nsAutoString title, type, media;
  PRBool isAlternate;

  GetStyleSheetInfo(title, type, media, &isAlternate);

  if (!type.LowerCaseEqualsLiteral("text/css")) {
    return NS_OK;
  }

  nsICSSLoader* loader = doc->GetCSSLoader();

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
  nsresult rv = NS_OK;
  if (isInline) {
    PRUint32 count = thisContent->GetChildCount();

    nsString *content = new nsString();
    NS_ENSURE_TRUE(content, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 i;
    for (i = 0; i < count; ++i) {
      nsIContent *node = thisContent->GetChildAt(i);
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
    rv = loader->LoadInlineStyle(thisContent, uin, mLineNumber, title, media,
                                 ((blockParser) ? parser.get() : nsnull),
                                 doneLoading, aObserver);
  }
  else {
    rv = loader->LoadStyleLink(thisContent, uri, title, media,
                               ((blockParser) ? parser.get() : nsnull),
                               doneLoading, aObserver);
  }

  if (NS_SUCCEEDED(rv) && blockParser && !doneLoading) {
    rv = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return rv;
}

