/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsContentSink.h"
#include "nsIScriptLoader.h"
#include "nsIDocument.h"
#include "nsICSSLoader.h"
#include "nsStyleConsts.h"
#include "nsStyleLinkElement.h"
#include "nsINodeInfo.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIRefreshURI.h"
#include "nsCPrefetchService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsIContent.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIParser.h"
#include "nsContentErrors.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsIAtom.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsNetCID.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsIServiceManagerUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsContentUtils.h"
#include "nsParserUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsWeakReference.h"
#include "nsUnicharUtils.h"


#ifdef ALLOW_ASYNCH_STYLE_SHEETS
const PRBool kBlockByDefault = PR_FALSE;
#else
const PRBool kBlockByDefault = PR_TRUE;
#endif


class nsScriptLoaderObserverProxy : public nsIScriptLoaderObserver
{
public:
  nsScriptLoaderObserverProxy(nsIScriptLoaderObserver* aInner)
    : mInner(do_GetWeakReference(aInner))
  {
  }
  virtual ~nsScriptLoaderObserverProxy()
  {
  }
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER

  nsWeakPtr mInner;
};

NS_IMPL_ISUPPORTS1(nsScriptLoaderObserverProxy, nsIScriptLoaderObserver)

NS_IMETHODIMP
nsScriptLoaderObserverProxy::ScriptAvailable(nsresult aResult,
                                             nsIDOMHTMLScriptElement *aElement,
                                             PRBool aIsInline,
                                             PRBool aWasPending,
                                             nsIURI *aURI,
                                             PRInt32 aLineNo,
                                             const nsAString & aScript)
{
  nsCOMPtr<nsIScriptLoaderObserver> inner = do_QueryReferent(mInner);

  if (inner) {
    return inner->ScriptAvailable(aResult, aElement, aIsInline, aWasPending,
                                  aURI, aLineNo, aScript);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoaderObserverProxy::ScriptEvaluated(nsresult aResult,
                                             nsIDOMHTMLScriptElement *aElement,
                                             PRBool aIsInline,
                                             PRBool aWasPending)
{
  nsCOMPtr<nsIScriptLoaderObserver> inner = do_QueryReferent(mInner);

  if (inner) {
    return inner->ScriptEvaluated(aResult, aElement, aIsInline, aWasPending);
  }

  return NS_OK;
}


NS_IMPL_ISUPPORTS3(nsContentSink,
                   nsICSSLoaderObserver,
                   nsISupportsWeakReference,
                   nsIScriptLoaderObserver)

nsContentSink::nsContentSink()
  : mNeedToBlockParser(PR_FALSE)
{
}

nsContentSink::~nsContentSink()
{
}

nsresult
nsContentSink::Init(nsIDocument* aDoc,
                    nsIURI* aURI,
                    nsISupports* aContainer,
                    nsIChannel* aChannel)
{
  NS_PRECONDITION(aDoc, "null ptr");
  NS_PRECONDITION(aURI, "null ptr");

  if (!aDoc || !aURI) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;

  mDocumentURI = aURI;
  mDocumentBaseURI = aURI;
  mDocShell = do_QueryInterface(aContainer);

  // use this to avoid a circular reference sink->document->scriptloader->sink
  nsCOMPtr<nsIScriptLoaderObserver> proxy =
      new nsScriptLoaderObserverProxy(this);
  NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

  nsIScriptLoader *loader = mDocument->GetScriptLoader();
  NS_ENSURE_TRUE(loader, NS_ERROR_FAILURE);
  nsresult rv = loader->AddObserver(proxy);
  NS_ENSURE_SUCCESS(rv, rv);

  mCSSLoader = aDoc->GetCSSLoader();

  // XXX this presumes HTTP header info is already set in document
  // XXX if it isn't we need to set it here...
  ProcessHTTPHeaders(aChannel);

  mNodeInfoManager = aDoc->GetNodeInfoManager();
  return mNodeInfoManager ? NS_OK : NS_ERROR_NOT_INITIALIZED;

}

NS_IMETHODIMP
nsContentSink::StyleSheetLoaded(nsICSSStyleSheet* aSheet, 
                                PRBool aDidNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptAvailable(nsresult aResult,
                               nsIDOMHTMLScriptElement *aElement,
                               PRBool aIsInline,
                               PRBool aWasPending,
                               nsIURI *aURI,
                               PRInt32 aLineNo,
                               const nsAString& aScript)
{
  PRUint32 count = mScriptElements.Count();

  if (count == 0) {
    return NS_OK;
  }

  // Check if this is the element we were waiting for
  if (aElement != mScriptElements[count - 1]) {
    return NS_OK;
  }

  if (mParser && !mParser->IsParserEnabled()) {
    // make sure to unblock the parser before evaluating the script,
    // we must unblock the parser even if loading the script failed or
    // if the script was empty, if we don't, the parser will never be
    // unblocked.
    mParser->UnblockParser();
  }

  // Mark the current script as loaded
  mNeedToBlockParser = PR_FALSE;

  if (NS_SUCCEEDED(aResult) && aResult != NS_CONTENT_SCRIPT_IS_EVENTHANDLER) {
    PreEvaluateScript();
  } else {
    mScriptElements.RemoveObjectAt(count - 1);

    if (mParser && aWasPending && aResult != NS_BINDING_ABORTED) {
      // Loading external script failed!. So, resume
      // parsing since the parser got blocked when loading
      // external script. - Ref. Bug: 94903
      mParser->ContinueParsing();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptEvaluated(nsresult aResult,
                               nsIDOMHTMLScriptElement *aElement,
                               PRBool aIsInline,
                               PRBool aWasPending)
{
  // Check if this is the element we were waiting for
  PRInt32 count = mScriptElements.Count();
  if (count == 0) {
    return NS_OK;
  }
  
  if (aElement != mScriptElements[count - 1]) {
    return NS_OK;
  }

  // Pop the script element stack
  mScriptElements.RemoveObjectAt(count - 1); 

  if (NS_SUCCEEDED(aResult)) {
    PostEvaluateScript();
  }

  if (mParser && mParser->IsParserEnabled() && aWasPending) {
    mParser->ContinueParsing();
  }

  return NS_OK;
}

nsresult
nsContentSink::ProcessHTTPHeaders(nsIChannel* aChannel)
{
  nsCOMPtr<nsIHttpChannel> httpchannel(do_QueryInterface(aChannel));
  
  if (!httpchannel) {
    return NS_OK;
  }

  static const char *const headers[] = {
    "link",
    "default-style",
    "content-style-type",
    // add more http headers if you need
    // XXXbz don't add content-location support without reading bug
    // 238654 and its dependencies/dups first.
    0
  };
  
  const char *const *name = headers;
  nsCAutoString tmp;
  
  while (*name) {
    nsresult rv = httpchannel->GetResponseHeader(nsDependentCString(*name), tmp);
    if (NS_SUCCEEDED(rv) && !tmp.IsEmpty()) {
      nsCOMPtr<nsIAtom> key = do_GetAtom(*name);
      ProcessHeaderData(key, NS_ConvertASCIItoUCS2(tmp));
    }
    ++name;
  }
  
  return NS_OK;
}

nsresult
nsContentSink::ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                                 nsIContent* aContent)
{
  nsresult rv = NS_OK;
  // necko doesn't process headers coming in from the parser

  mDocument->SetHeaderData(aHeader, aValue);

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // see if we have a refresh "header".
  if (aHeader == nsHTMLAtoms::refresh) {
    // first get our baseURI

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mDocShell);
    rv = webNav->GetCurrentURI(getter_AddRefs(baseURI));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIRefreshURI> reefer = do_QueryInterface(mDocShell);
    if (reefer) {
      rv = reefer->SetupRefreshURIFromHeader(baseURI,
                                             NS_ConvertUCS2toUTF8(aValue));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  else if (aHeader == nsHTMLAtoms::setcookie) {
    nsCOMPtr<nsICookieService> cookieServ =
      do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Get a URI from the document principal

    // We use the original codebase in case the codebase was changed
    // by SetDomain

    nsIPrincipal *docPrincipal = mDocument->GetPrincipal();
    if (!docPrincipal) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPrincipal> systemPrincipal;
    nsContentUtils::GetSecurityManager()->
      GetSystemPrincipal(getter_AddRefs(systemPrincipal));
    NS_ASSERTION(systemPrincipal, "No system principal");
    
    if (docPrincipal == systemPrincipal) {
      // Document's principal is not a codebase, so we can't set cookies
      return NS_OK;
    }

    nsCOMPtr<nsIURI> codebaseURI;
    rv = docPrincipal->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrompt> prompt;
    nsCOMPtr<nsIDOMWindowInternal> window (do_QueryInterface(mDocument->GetScriptGlobalObject()));
    if (window) {
      window->GetPrompter(getter_AddRefs(prompt));
    }

    nsCOMPtr<nsIChannel> channel;
    if (mParser) {
      mParser->GetChannel(getter_AddRefs(channel));
    }

    rv = cookieServ->SetCookieString(codebaseURI,
                                     prompt,
                                     NS_ConvertUCS2toUTF8(aValue).get(),
                                     channel);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else if (aHeader == nsHTMLAtoms::link) {
    rv = ProcessLinkHeader(aContent, aValue);
  }
  else if (aHeader == nsHTMLAtoms::msthemecompatible) {
    // Disable theming for the presshell if the value is no.
    nsAutoString value(aValue);
    if (value.LowerCaseEqualsLiteral("no")) {
      nsIPresShell* shell = mDocument->GetShellAt(0);
      if (shell) {
        shell->DisableThemeSupport();
      }
    }
  }
  else if (mParser) {
    // we also need to report back HTTP-EQUIV headers to the channel
    // so that it can process things like pragma: no-cache or other
    // cache-control headers. Ideally this should also be the way for
    // cookies to be set! But we'll worry about that in the next
    // iteration
    nsCOMPtr<nsIChannel> channel;
    if (NS_SUCCEEDED(mParser->GetChannel(getter_AddRefs(channel)))) {
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel) {
        const char* header;
        (void)aHeader->GetUTF8String(&header);
        (void)httpChannel->SetResponseHeader(nsDependentCString(header),
                                             NS_ConvertUCS2toUTF8(aValue),
                                             PR_TRUE);
      }
    }
  }

  return rv;
}


static const PRUnichar kSemiCh = PRUnichar(';');
static const PRUnichar kCommaCh = PRUnichar(',');
static const PRUnichar kEqualsCh = PRUnichar('=');
static const PRUnichar kLessThanCh = PRUnichar('<');
static const PRUnichar kGreaterThanCh = PRUnichar('>');

nsresult
nsContentSink::ProcessLinkHeader(nsIContent* aElement,
                                 const nsAString& aLinkData)
{
  nsresult rv = NS_OK;

  // parse link content and call process style link
  nsAutoString href;
  nsAutoString rel;
  nsAutoString title;
  nsAutoString type;
  nsAutoString media;
  PRBool didBlock = PR_FALSE;

  // copy to work buffer
  nsAutoString stringList(aLinkData);

  // put an extra null at the end
  stringList.Append(kNullCh);

  PRUnichar* start = stringList.BeginWriting();
  PRUnichar* end   = start;
  PRUnichar* last  = start;
  PRUnichar  endCh;

  while (*start != kNullCh) {
    // skip leading space
    while ((*start != kNullCh) && nsCRT::IsAsciiSpace(*start)) {
      ++start;
    }

    end = start;
    last = end - 1;

    // look for semicolon or comma
    while (*end != kNullCh && *end != kSemiCh && *end != kCommaCh) {
      PRUnichar ch = *end;

      if (ch == kApostrophe || ch == kQuote || ch == kLessThanCh) {
        // quoted string

        PRUnichar quote = *end;
        if (quote == kLessThanCh) {
          quote = kGreaterThanCh;
        }

        PRUnichar* closeQuote = (end + 1);

        // seek closing quote
        while (*closeQuote != kNullCh && quote != *closeQuote) {
          ++closeQuote;
        }

        if (quote == *closeQuote) {
          // found closer

          // skip to close quote
          end = closeQuote;

          last = end - 1;

          ch = *(end + 1);

          if (ch != kNullCh && ch != kSemiCh && ch != kCommaCh) {
            // end string here
            *(++end) = kNullCh;

            ch = *(end + 1);

            // keep going until semi or comma
            while (ch != kNullCh && ch != kSemiCh && ch != kCommaCh) {
              ++end;

              ch = *end;
            }
          }
        }
      }

      ++end;
      ++last;
    }

    endCh = *end;

    // end string here
    *end = kNullCh;

    if (start < end) {
      if ((*start == kLessThanCh) && (*last == kGreaterThanCh)) {
        *last = kNullCh;

        if (href.IsEmpty()) { // first one wins
          href = (start + 1);
          href.StripWhitespace();
        }
      } else {
        PRUnichar* equals = start;

        while ((*equals != kNullCh) && (*equals != kEqualsCh)) {
          equals++;
        }

        if (*equals != kNullCh) {
          *equals = kNullCh;
          nsAutoString  attr(start);
          attr.StripWhitespace();

          PRUnichar* value = ++equals;
          while (nsCRT::IsAsciiSpace(*value)) {
            value++;
          }

          if (((*value == kApostrophe) || (*value == kQuote)) &&
              (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (attr.LowerCaseEqualsLiteral("rel")) {
            if (rel.IsEmpty()) {
              rel = value;
              rel.CompressWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("title")) {
            if (title.IsEmpty()) {
              title = value;
              title.CompressWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("type")) {
            if (type.IsEmpty()) {
              type = value;
              type.StripWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("media")) {
            if (media.IsEmpty()) {
              media = value;

              // HTML4.0 spec is inconsistent, make it case INSENSITIVE
              ToLowerCase(media);
            }
          }
        }
      }
    }

    if (endCh == kCommaCh) {
      // hit a comma, process what we've got so far

      if (!href.IsEmpty() && !rel.IsEmpty()) {
        rv = ProcessLink(aElement, href, rel, title, type, media);
        if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
          didBlock = PR_TRUE;
        }
      }

      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
    }

    start = ++end;
  }

  if (!href.IsEmpty() && !rel.IsEmpty()) {
    rv = ProcessLink(aElement, href, rel, title, type, media);

    if (NS_SUCCEEDED(rv) && didBlock) {
      rv = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }

  return rv;
}


nsresult
nsContentSink::ProcessLink(nsIContent* aElement,
                           const nsAString& aHref, const nsAString& aRel,
                           const nsAString& aTitle, const nsAString& aType,
                           const nsAString& aMedia)
{
  // XXX seems overkill to generate this string array
  nsStringArray linkTypes;
  nsStyleLinkElement::ParseLinkTypes(aRel, linkTypes);

  PRBool hasPrefetch = (linkTypes.IndexOf(NS_LITERAL_STRING("prefetch")) != -1);
  // prefetch href if relation is "next" or "prefetch"
  if (hasPrefetch || linkTypes.IndexOf(NS_LITERAL_STRING("next")) != -1) {
    PrefetchHref(aHref, hasPrefetch);
  }

  // is it a stylesheet link?
  if (linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet")) == -1) {
    return NS_OK;
  }

  PRBool isAlternate = linkTypes.IndexOf(NS_LITERAL_STRING("alternate")) != -1;
  return ProcessStyleLink(aElement, aHref, isAlternate, aTitle, aType,
                          aMedia);
}

nsresult
nsContentSink::ProcessStyleLink(nsIContent* aElement,
                                const nsAString& aHref,
                                PRBool aAlternate,
                                const nsAString& aTitle,
                                const nsAString& aType,
                                const nsAString& aMedia)
{
  if (aAlternate && aTitle.IsEmpty()) {
    // alternates must have title return without error, for now
    return NS_OK;
  }

  nsAutoString  mimeType;
  nsAutoString  params;
  nsParserUtils::SplitMimeType(aType, mimeType, params);

  // see bug 18817
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    // Unknown stylesheet language
    return NS_OK;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentBaseURI);
  
  if (NS_FAILED(rv)) {
    // The URI is bad, move along, don't propagate the error (for now)
    return NS_OK;
  }

  if (!aAlternate) {
    // possibly preferred sheet

    if (!aTitle.IsEmpty()) {
      nsAutoString preferredStyle;
      mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                               preferredStyle);
      if (preferredStyle.IsEmpty()) {
        mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
      }
    }
  }

  PRBool blockParser = kBlockByDefault;
  if (aAlternate) {
    blockParser = PR_FALSE;
  }

  // NOTE: no longer honoring the important keyword to indicate
  // blocking as it is proprietary and unnecessary since all
  // non-alternate will block the parser now -mja
#if 0
  if (linkTypes.IndexOf("important") != -1) {
    blockParser = PR_TRUE;
  }
#endif

  PRBool doneLoading;
  nsIParser* parser = nsnull;
  if (blockParser) {
    parser = mParser;
  }
  rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia,
                                 parser, doneLoading, this);

  if (NS_SUCCEEDED(rv) && blockParser && !doneLoading) {
    rv = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return rv;
}


nsresult
nsContentSink::ProcessMETATag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing base-element");

  nsresult rv = NS_OK;

  // set any HTTP-EQUIV data into document's header data as well as url
  nsAutoString header;
  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::httpEquiv, header);
  if (!header.IsEmpty()) {
    nsAutoString result;
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::content, result);
    if (!result.IsEmpty()) {
      ToLowerCase(header);
      nsCOMPtr<nsIAtom> fieldAtom(do_GetAtom(header));
      rv = ProcessHeaderData(fieldAtom, result, aContent); 
    }
  }

  return rv;
}


void
nsContentSink::PrefetchHref(const nsAString &aHref, PRBool aExplicit)
{
  //
  // SECURITY CHECK: disable prefetching from mailnews!
  //
  // walk up the docshell tree to see if any containing
  // docshell are of type MAIL.
  //
  if (!mDocShell)
    return;

  nsCOMPtr<nsIDocShell> docshell = mDocShell;

  nsCOMPtr<nsIDocShellTreeItem> treeItem, parentItem;
  do {
    PRUint32 appType = 0;
    nsresult rv = docshell->GetAppType(&appType);
    if (NS_FAILED(rv) || appType == nsIDocShell::APP_TYPE_MAIL)
      return; // do not prefetch from mailnews
    if (treeItem = do_QueryInterface(docshell)) {
      treeItem->GetParent(getter_AddRefs(parentItem));
      if (parentItem) {
        treeItem = parentItem;
        docshell = do_QueryInterface(treeItem);
        if (!docshell) {
          NS_ERROR("cannot get a docshell from a treeItem!");
          return;
        }
      }
    }
  } while (parentItem);
  
  // OK, we passed the security check...
  
  nsCOMPtr<nsIPrefetchService> prefetchService(do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
  if (prefetchService) {
    // construct URI using document charset
    const nsACString &charset = mDocument->GetDocumentCharacterSet();
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref,
              charset.IsEmpty() ? nsnull : PromiseFlatCString(charset).get(),
              mDocumentBaseURI);
    if (uri) {
      prefetchService->PrefetchURI(uri, mDocumentURI, aExplicit);
    }
  }
}


// Convert the ref from document charset to unicode.
static nsresult
CharsetConvRef(const nsACString& aDocCharset,
               const nsCString& aRefInDocCharset,
               nsString& aRefInUnicode)
{
  nsresult rv;

  nsCOMPtr <nsIAtom> docCharsetAtom;
  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = ccm->GetUnicodeDecoder(PromiseFlatCString(aDocCharset).get(),
                              getter_AddRefs(decoder));
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 srcLen = aRefInDocCharset.Length();
  PRInt32 dstLen;
  rv = decoder->GetMaxLength(aRefInDocCharset.get(), srcLen, &dstLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUnichar *ustr = (PRUnichar *)nsMemory::Alloc((dstLen + 1) *
                                                 sizeof(PRUnichar));
  if (!ustr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = decoder->Convert(aRefInDocCharset.get(), &srcLen, ustr, &dstLen);
  if (NS_SUCCEEDED(rv)) {
    ustr[dstLen] = 0;
    aRefInUnicode.Assign(ustr, dstLen);
  }

  nsMemory::Free(ustr);

  return rv;
}

PRBool
nsContentSink::ScrollToRef(PRBool aReallyScroll)
{
  if (mRef.IsEmpty()) {
    return PR_FALSE;
  }

  PRBool didScroll = PR_FALSE;

  char* tmpstr = ToNewCString(mRef);
  if (!tmpstr) {
    return PR_FALSE;
  }

  nsUnescape(tmpstr);
  nsCAutoString unescapedRef;
  unescapedRef.Assign(tmpstr);
  nsMemory::Free(tmpstr);

  nsresult rv = NS_ERROR_FAILURE;
  // We assume that the bytes are in UTF-8, as it says in the spec:
  // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
  NS_ConvertUTF8toUCS2 ref(unescapedRef);

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (shell) {
      // Check an empty string which might be caused by the UTF-8 conversion
      if (!ref.IsEmpty()) {
        // Note that GoToAnchor will handle flushing layout as needed.
        rv = shell->GoToAnchor(ref, aReallyScroll);
      } else {
        rv = NS_ERROR_FAILURE;
      }

      // If UTF-8 URI failed then try to assume the string as a
      // document's charset.

      if (NS_FAILED(rv)) {
        const nsACString &docCharset = mDocument->GetDocumentCharacterSet();

        rv = CharsetConvRef(docCharset, unescapedRef, ref);

        if (NS_SUCCEEDED(rv) && !ref.IsEmpty())
          rv = shell->GoToAnchor(ref, aReallyScroll);
      }
      if (NS_SUCCEEDED(rv)) {
        didScroll = PR_TRUE;
      }
    }
  }

  return didScroll;
}

nsresult
nsContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (!vm) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    PRBool enabled;
    contentViewer->GetEnableRendering(&enabled);
    if (enabled) {
      vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
    }
  }

  return NS_OK;
}

void
nsContentSink::StartLayout(PRBool aIsFrameset)
{
  PRUint32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell *shell = mDocument->GetShellAt(i);

    if (shell) {
      // Make sure we don't call InitialReflow() for a shell that has
      // already called it. This can happen when the layout frame for
      // an iframe is constructed *between* the Embed() call for the
      // docshell in the iframe, and the content sink's call to OpenBody().
      // (Bug 153815)

      PRBool didInitialReflow = PR_FALSE;
      shell->GetDidInitialReflow(&didInitialReflow);
      if (didInitialReflow) {
        // XXX: The assumption here is that if something already
        // called InitialReflow() on this shell, it also did some of
        // the setup below, so we do nothing and just move on to the
        // next shell in the list.

        continue;
      }

      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsRect r = cx->GetVisibleArea();
      shell->InitialReflow(r.width, r.height);

      // Now trigger a refresh
      RefreshIfEnabled(shell->GetViewManager());
    }
  }

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  if (mDocumentURI) {
    nsCAutoString ref;

    // Since all URI's that pass through here aren't URL's we can't
    // rely on the nsIURI implementation for providing a way for
    // finding the 'ref' part of the URI, we'll haveto revert to
    // string routines for finding the data past '#'

    mDocumentURI->GetSpec(ref);

    nsReadingIterator<char> start, end;

    ref.BeginReading(start);
    ref.EndReading(end);

    if (FindCharInReadable('#', start, end)) {
      ++start; // Skip over the '#'

      mRef = Substring(start, end);
    }
  }

  if (!mRef.IsEmpty() || aIsFrameset) {
    // Disable the scroll bars.
    for (i = 0; i < ns; i++) {
      nsIPresShell *shell = mDocument->GetShellAt(i);

      nsIViewManager* vm = shell->GetViewManager();
      if (vm) {
        nsIView* rootView = nsnull;
        vm->GetRootView(rootView);
        nsCOMPtr<nsIScrollableView> sview(do_QueryInterface(rootView));

        if (sview) {
          sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
        }
      }
    }
  }
}
