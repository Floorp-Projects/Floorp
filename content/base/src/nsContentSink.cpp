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
 *   Henri Sivonen <hsivonen@iki.fi>
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

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#include "nsContentSink.h"
#include "nsScriptLoader.h"
#include "nsIDocument.h"
#include "nsICSSLoader.h"
#include "nsStyleConsts.h"
#include "nsStyleLinkElement.h"
#include "nsINodeInfo.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsCPrefetchService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsIContent.h"
#include "nsIScriptElement.h"
#include "nsIParser.h"
#include "nsContentErrors.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsNetCID.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsParserUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsWeakReference.h"
#include "nsUnicharUtils.h"
#include "nsNodeInfoManager.h"
#include "nsTimer.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsIDOMNSDocument.h"
#include "nsIRequest.h"
#include "nsNodeUtils.h"
#include "nsIDOMNode.h"

PRLogModuleInfo* gContentSinkLogModuleInfo;

//----------------------------------------------------------------------
//
// DummyParserRequest
//
//   This is a dummy request implementation that we add to the document's load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't fire
//   before we've finished all of parsing and tokenizing of the document.
//

class DummyParserRequest : public nsIRequest
{
protected:
  nsContentSink* mSink; // Weak reference

public:
  DummyParserRequest(nsContentSink* aSink);

  NS_DECL_ISUPPORTS

  // nsIRequest
  NS_IMETHOD GetName(nsACString &result)
  {
    result.AssignLiteral("about:layout-dummy-request");
    return NS_OK;
  }

  NS_IMETHOD IsPending(PRBool *_retval)
  {
    *_retval = PR_TRUE;
    return NS_OK;
  }

  NS_IMETHOD GetStatus(nsresult *status)
  {
    *status = NS_OK;
    return NS_OK;
  }

  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void)
  {
    return NS_OK;
  }

  NS_IMETHOD Resume(void)
  {
    return NS_OK;
  }

  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup)
  {
    *aLoadGroup = nsnull;

    return NS_OK;
  }

  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup)
  {
    return NS_OK;
  }

  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags)
  {
    *aLoadFlags = nsIRequest::LOAD_NORMAL;

    return NS_OK;
  }

  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags)
  {
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(DummyParserRequest, nsIRequest)


DummyParserRequest::DummyParserRequest(nsContentSink* aSink)
  : mSink(aSink)
{
}

NS_IMETHODIMP
DummyParserRequest::Cancel(nsresult status)
{
  // Cancel parser
  if (mSink && mSink->mParser) {
    mSink->mParser->CancelParsingEvents();
  }
  return NS_OK;
}


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
                                             nsIScriptElement *aElement,
                                             PRBool aIsInline,
                                             PRBool aWasPending,
                                             nsIURI *aURI,
                                             PRInt32 aLineNo)
{
  nsCOMPtr<nsIScriptLoaderObserver> inner = do_QueryReferent(mInner);

  if (inner) {
    return inner->ScriptAvailable(aResult, aElement, aIsInline, aWasPending,
                                  aURI, aLineNo);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoaderObserverProxy::ScriptEvaluated(nsresult aResult,
                                             nsIScriptElement *aElement,
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
{
  // We have a zeroing operator new
  NS_ASSERTION(mLayoutStarted == PR_FALSE, "What?");
  NS_ASSERTION(mDynamicLowerValue == PR_FALSE, "What?");
  NS_ASSERTION(mParsing == PR_FALSE, "What?");
  NS_ASSERTION(mLastSampledUserEventTime == 0, "What?");
  NS_ASSERTION(mDeflectedCount == 0, "What?");
  NS_ASSERTION(mDroppedTimer == PR_FALSE, "What?");
  NS_ASSERTION(mInMonolithicContainer == 0, "What?");
  NS_ASSERTION(mInNotification == 0, "What?");

#ifdef NS_DEBUG
  if (!gContentSinkLogModuleInfo) {
    gContentSinkLogModuleInfo = PR_NewLogModule("nscontentsink");
  }
#endif
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
  if (mDocShell) {
    PRUint32 loadType = 0;
    mDocShell->GetLoadType(&loadType);
    mChangeScrollPosWhenScrollingToRef =
      ((loadType & nsIDocShell::LOAD_CMD_HISTORY) == 0);
  }

  // use this to avoid a circular reference sink->document->scriptloader->sink
  nsCOMPtr<nsIScriptLoaderObserver> proxy =
      new nsScriptLoaderObserverProxy(this);
  NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

  nsScriptLoader *loader = mDocument->GetScriptLoader();
  NS_ENSURE_TRUE(loader, NS_ERROR_FAILURE);
  loader->AddObserver(proxy);

  mCSSLoader = aDoc->CSSLoader();

  ProcessHTTPHeaders(aChannel);

  mNodeInfoManager = aDoc->NodeInfoManager();

  mNotifyOnTimer =
    nsContentUtils::GetBoolPref("content.notify.ontimer", PR_TRUE);

  // -1 means never
  mBackoffCount =
    nsContentUtils::GetIntPref("content.notify.backoffcount", -1);

  // The mNotificationInterval has a dramatic effect on how long it
  // takes to initially display content for slow connections.
  // The current value provides good
  // incremental display of content without causing an increase
  // in page load time. If this value is set below 1/10 of second
  // it starts to impact page load performance.
  // see bugzilla bug 72138 for more info.
  mNotificationInterval =
    nsContentUtils::GetIntPref("content.notify.interval", 120000);

  // The mMaxTokenProcessingTime controls how long we stay away from
  // the event loop when processing token. A lower value makes the app
  // more responsive, but may increase page load time.  The content
  // sink mNotificationInterval gates how frequently the content is
  // processed so it will also affect how interactive the app is
  // during page load also. The mNotification prevents contents
  // flushes from happening too frequently. while
  // mMaxTokenProcessingTime prevents flushes from happening too
  // infrequently.

  // The current ratio of 3 to 1 was determined to be the lowest
  // mMaxTokenProcessingTime which does not impact page load
  // performance.  See bugzilla bug 76722 for details.

  mMaxTokenProcessingTime =
    nsContentUtils::GetIntPref("content.max.tokenizing.time",
                               mNotificationInterval * 3);

  // 3/4 second (750000us) default for switching
  mDynamicIntervalSwitchThreshold =
    nsContentUtils::GetIntPref("content.switch.threshold", 750000);

  mCanInterruptParser =
    nsContentUtils::GetBoolPref("content.interrupt.parsing", PR_TRUE);

  return NS_OK;

}

NS_IMETHODIMP
nsContentSink::StyleSheetLoaded(nsICSSStyleSheet* aSheet,
                                PRBool aWasAlternate,
                                nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptAvailable(nsresult aResult,
                               nsIScriptElement *aElement,
                               PRBool aIsInline,
                               PRBool aWasPending,
                               nsIURI *aURI,
                               PRInt32 aLineNo)
{
  PRUint32 count = mScriptElements.Count();

  if (count == 0) {
    return NS_OK;
  }

  // aElement will not be in mScriptElements if a <script> was added
  // using the DOM during loading, or if the script was inline and thus
  // never blocked.
  NS_ASSERTION(mScriptElements.IndexOf(aElement) == count - 1 ||
               mScriptElements.IndexOf(aElement) == PRUint32(-1),
               "script found at unexpected position");

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

  if (NS_SUCCEEDED(aResult)) {
    PreEvaluateScript();
  } else {
    mScriptElements.RemoveObjectAt(count - 1);

    if (mParser && aWasPending && aResult != NS_BINDING_ABORTED) {
      // Loading external script failed!. So, resume parsing since the parser
      // got blocked when loading external script. See
      // http://bugzilla.mozilla.org/show_bug.cgi?id=94903.
      //
      // XXX We don't resume parsing if we get NS_BINDING_ABORTED from the
      //     script load, assuming that that error code means that the user
      //     stopped the load through some action (like clicking a link). See
      //     http://bugzilla.mozilla.org/show_bug.cgi?id=243392.
      mParser->ContinueInterruptedParsing();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptEvaluated(nsresult aResult,
                               nsIScriptElement *aElement,
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
    PostEvaluateScript(aElement);
  }

  if (mParser && mParser->IsParserEnabled() && aWasPending) {
    mParser->ContinueInterruptedParsing();
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

  // Note that the only header we care about is the "link" header, since we
  // have all the infrastructure for kicking off stylesheet loads.
  
  nsCAutoString linkHeader;
  
  nsresult rv = httpchannel->GetResponseHeader(NS_LITERAL_CSTRING("link"),
                                               linkHeader);
  if (NS_SUCCEEDED(rv) && !linkHeader.IsEmpty()) {
    ProcessHeaderData(nsGkAtoms::link,
                      NS_ConvertASCIItoUTF16(linkHeader));
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

  if (aHeader == nsGkAtoms::setcookie) {
    // Note: Necko already handles cookies set via the channel.  We can't just
    // call SetCookie on the channel because we want to do some security checks
    // here and want to use the prompt associated to our current window, not
    // the window where the channel was dispatched.
    nsCOMPtr<nsICookieService> cookieServ =
      do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Get a URI from the document principal

    // We use the original codebase in case the codebase was changed
    // by SetDomain

    // Note that a non-codebase principal (eg the system principal) will return
    // a null URI.
    nsCOMPtr<nsIURI> codebaseURI;
    rv = mDocument->NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_TRUE(codebaseURI, rv);

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
                                     NS_ConvertUTF16toUTF8(aValue).get(),
                                     channel);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else if (aHeader == nsGkAtoms::link) {
    rv = ProcessLinkHeader(aContent, aValue);
  }
  else if (aHeader == nsGkAtoms::msthemecompatible) {
    // Disable theming for the presshell if the value is no.
    // XXXbz don't we want to support this as an HTTP header too?
    nsAutoString value(aValue);
    if (value.LowerCaseEqualsLiteral("no")) {
      nsIPresShell* shell = mDocument->GetShellAt(0);
      if (shell) {
        shell->DisableThemeSupport();
      }
    }
  }
  // Don't report "refresh" headers back to necko, since our document handles
  // them
  else if (aHeader != nsGkAtoms::refresh && mParser) {
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
                                             NS_ConvertUTF16toUTF8(aValue),
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
                           const nsSubstring& aHref, const nsSubstring& aRel,
                           const nsSubstring& aTitle, const nsSubstring& aType,
                           const nsSubstring& aMedia)
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
                                const nsSubstring& aHref,
                                PRBool aAlternate,
                                const nsSubstring& aTitle,
                                const nsSubstring& aType,
                                const nsSubstring& aMedia)
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

  nsIParser* parser = nsnull;
  if (kBlockByDefault) {
    parser = mParser;
  }
  
  PRBool isAlternate;
  rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, aAlternate,
                                 parser, this, &isAlternate);
  if (NS_SUCCEEDED(rv) && parser && !isAlternate) {
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
  aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
  if (!header.IsEmpty()) {
    nsAutoString result;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::content, result);
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


void
nsContentSink::ScrollToRef()
{
  if (mRef.IsEmpty()) {
    return;
  }

  PRBool didScroll = PR_FALSE;

  char* tmpstr = ToNewCString(mRef);
  if (!tmpstr) {
    return;
  }

  nsUnescape(tmpstr);
  nsCAutoString unescapedRef;
  unescapedRef.Assign(tmpstr);
  nsMemory::Free(tmpstr);

  nsresult rv = NS_ERROR_FAILURE;
  // We assume that the bytes are in UTF-8, as it says in the spec:
  // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
  NS_ConvertUTF8toUTF16 ref(unescapedRef);

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (shell) {
      // Check an empty string which might be caused by the UTF-8 conversion
      if (!ref.IsEmpty()) {
        // Note that GoToAnchor will handle flushing layout as needed.
        rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
      } else {
        rv = NS_ERROR_FAILURE;
      }

      // If UTF-8 URI failed then try to assume the string as a
      // document's charset.

      if (NS_FAILED(rv)) {
        const nsACString &docCharset = mDocument->GetDocumentCharacterSet();

        rv = nsContentUtils::ConvertStringFromCharset(docCharset, unescapedRef, ref);

        if (NS_SUCCEEDED(rv) && !ref.IsEmpty())
          rv = shell->GoToAnchor(ref, mChangeScrollPosWhenScrollingToRef);
      }
      if (NS_SUCCEEDED(rv)) {
        mScrolledToRefAlready = PR_TRUE;
      }
    }
  }
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
  mLayoutStarted = PR_TRUE;
  mLastNotificationTime = PR_Now();
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
      nsRect r = shell->GetPresContext()->GetVisibleArea();
      nsresult rv = shell->InitialReflow(r.width, r.height);
      if (NS_FAILED(rv)) {
        return;
      }

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
}

void
nsContentSink::TryToScrollToRef()
{
  if (mRef.IsEmpty()) {
    return;
  }

  if (mScrolledToRefAlready) {
    return;
  }

  ScrollToRef();
}

void
nsContentSink::NotifyAppend(nsIContent* aContainer, PRUint32 aStartIndex)
{
  if (aContainer->GetCurrentDoc() != mDocument) {
    // aContainer is not actually in our document anymore.... Just bail out of
    // here; notifying on our document for this append would be wrong.
    return;
  }

  mInNotification++;

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  nsNodeUtils::ContentAppended(aContainer, aStartIndex);
  mLastNotificationTime = PR_Now();

  MOZ_TIMER_DEBUGLOG(("Restore: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_RESTORE(mWatch);

  mInNotification--;
}

NS_IMETHODIMP
nsContentSink::Notify(nsITimer *timer)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_START(mWatch);

  if (mParsing) {
    // We shouldn't interfere with our normal DidProcessAToken logic
    mDroppedTimer = PR_TRUE;
    return NS_OK;
  }
  
#ifdef MOZ_DEBUG
  {
    PRTime now = PR_Now();
    PRInt64 diff, interval;
    PRInt32 delay;

    LL_I2L(interval, GetNotificationInterval());
    LL_SUB(diff, now, mLastNotificationTime);

    LL_SUB(diff, diff, interval);
    LL_L2I(delay, diff);
    delay /= PR_USEC_PER_MSEC;

    mBackoffCount--;
    SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("nsContentSink::Notify: reflow on a timer: %d milliseconds "
                "late, backoff count: %d", delay, mBackoffCount));
  }
#endif

  FlushTags();

  // Now try and scroll to the reference
  // XXX Should we scroll unconditionally for history loads??
  TryToScrollToRef();
  mNotificationTimer = nsnull;
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

PRBool
nsContentSink::IsTimeToNotify()
{
  if (!mNotifyOnTimer || !mLayoutStarted || !mBackoffCount ||
      mInMonolithicContainer) {
    return PR_FALSE;
  }

  PRTime now = PR_Now();
  PRInt64 interval, diff;

  LL_I2L(interval, GetNotificationInterval());
  LL_SUB(diff, now, mLastNotificationTime);

  if (LL_CMP(diff, >, interval)) {
    mBackoffCount--;
    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
nsContentSink::WillInterruptImpl()
{
  nsresult result = NS_OK;

  SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("nsContentSink::WillInterrupt: this=%p", this));
#ifndef SINK_NO_INCREMENTAL
  if (mNotifyOnTimer && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      PRInt64 now = PR_Now();
      PRInt64 interval = GetNotificationInterval();
      PRInt64 diff = now - mLastNotificationTime;

      // If it's already time for us to have a notification
      if (diff > interval || mDroppedTimer) {
        mBackoffCount--;
        SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_REFLOW,
                   ("nsContentSink::WillInterrupt: flushing tags since we've "
                    "run out time; backoff count: %d", mBackoffCount));
        result = FlushTags();
        if (mDroppedTimer) {
          TryToScrollToRef();
          mDroppedTimer = PR_FALSE;
        }
      } else if (!mNotificationTimer) {
        interval -= diff;
        PRInt32 delay = interval;

        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;

        mNotificationTimer = do_CreateInstance("@mozilla.org/timer;1",
                                               &result);
        if (NS_SUCCEEDED(result)) {
          SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_REFLOW,
                     ("nsContentSink::WillInterrupt: setting up timer with "
                      "delay %d", delay));

          result =
            mNotificationTimer->InitWithCallback(this, delay,
                                                 nsITimer::TYPE_ONE_SHOT);
          if (NS_FAILED(result)) {
            mNotificationTimer = nsnull;
          }
        }
      }
    }
  } else {
    SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("nsContentSink::WillInterrupt: flushing tags "
                "unconditionally"));
    result = FlushTags();
  }
#endif

  mParsing = PR_FALSE;

  return result;
}

nsresult
nsContentSink::WillResumeImpl()
{
  SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("nsContentSink::WillResume: this=%p", this));

  mParsing = PR_TRUE;

  return NS_OK;
}

nsresult
nsContentSink::DidProcessATokenImpl()
{
  if (!mCanInterruptParser) {
    return NS_OK;
  }
  // There is both a high frequency interrupt mode and a low
  // frequency interupt mode controlled by the flag
  // mDynamicLowerValue The high frequency mode
  // interupts the parser frequently to provide UI responsiveness at
  // the expense of page load time. The low frequency mode
  // interrupts the parser and samples the system clock infrequently
  // to provide fast page load time. When the user moves the mouse,
  // clicks or types the mode switches to the high frequency
  // interrupt mode. If the user stops moving the mouse or typing
  // for a duration of time (mDynamicIntervalSwitchThreshold) it
  // switches to low frequency interrupt mode.

  // Get the current user event time
  nsIPresShell *shell = mDocument->GetShellAt(0);

  if (!shell) {
    // If there's no pres shell in the document, return early since
    // we're not laying anything out here.
    return NS_OK;
  }

  nsIViewManager* vm = shell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
  PRUint32 eventTime;
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = vm->GetWidget(getter_AddRefs(widget));
  if (!widget || NS_FAILED(widget->GetLastInputEventTime(eventTime))) {
      // If we can't get the last input time from the widget
      // then we will get it from the viewmanager.
      rv = vm->GetLastUserEventTime(eventTime);
      NS_ENSURE_SUCCESS(rv , NS_ERROR_FAILURE);
  }


  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  if (!mDynamicLowerValue && mLastSampledUserEventTime == eventTime) {
    // The magic value of NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE
    // was selected by empirical testing. It provides reasonable
    // user response and prevents us from sampling the clock too
    // frequently.
    if (mDeflectedCount < NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE) {
      mDeflectedCount++;
      // return early to prevent sampling the clock. Note: This
      // prevents us from switching to higher frequency (better UI
      // responsive) mode, so limit ourselves to doing for no more
      // than NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE tokens.

      return NS_OK;
    }

    // reset count and drop through to the code which samples the
    // clock and does the dynamic switch between the high
    // frequency and low frequency interruption of the parser.
    mDeflectedCount = 0;
  }
  mLastSampledUserEventTime = eventTime;

  PRUint32 currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  // Get the last user event time and compare it with the current
  // time to determine if the lower value for content notification
  // and max token processing should be used. But only consider
  // using the lower value if the document has already been loading
  // for 2 seconds. 2 seconds was chosen because it is greater than
  // the default 3/4 of second that is used to determine when to
  // switch between the modes and it gives the document a little
  // time to create windows.  This is important because on some
  // systems (Windows, for example) when a window is created and the
  // mouse is over it, a mouse move event is sent, which will kick
  // us into interactive mode otherwise. It also suppresses reaction
  // to pressing the ENTER key in the URL bar...

  PRUint32 delayBeforeLoweringThreshold =
    NS_STATIC_CAST(PRUint32, ((2 * mDynamicIntervalSwitchThreshold) +
                              NS_DELAY_FOR_WINDOW_CREATION));

  if ((currentTime - mBeginLoadTime) > delayBeforeLoweringThreshold) {
    if ((currentTime - eventTime) <
        NS_STATIC_CAST(PRUint32, mDynamicIntervalSwitchThreshold)) {

      if (!mDynamicLowerValue) {
        // lower the dynamic values to favor application
        // responsiveness over page load time.
        mDynamicLowerValue = PR_TRUE;
        // Set the performance hint to prevent event starvation when
        // dispatching PLEvents. This improves application responsiveness
        // during page loads.
        FavorPerformanceHint(PR_FALSE, 0);
      }

    }
    else if (mDynamicLowerValue) {
      // raise the content notification and MaxTokenProcessing time
      // to favor overall page load speed over responsiveness.
      mDynamicLowerValue = PR_FALSE;
      // Reset the hint that to favoring performance for PLEvent dispatch.
      FavorPerformanceHint(PR_TRUE, 0);
    }
  }

  if ((currentTime - mDelayTimerStart) >
      NS_STATIC_CAST(PRUint32, GetMaxTokenProcessingTime())) {
    return NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  return NS_OK;
}

//----------------------------------------------------------------------

void
nsContentSink::FavorPerformanceHint(PRBool perfOverStarvation, PRUint32 starvationDelay)
{
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell)
    appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
}

void
nsContentSink::BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Since this could result in frame
  // creation, make sure we've flushed everything before we
  // continue.

  // Note that UPDATE_CONTENT_STATE notifications never cause
  // synchronous frame construction, so we never have to worry about
  // them here.  The code that handles the async event these
  // notifications post will flush us out if it needs to.

  // Also, if this is not an UPDATE_CONTENT_STATE notification,
  // increment mInNotification to make sure we don't flush again until
  // the end of this update, even if nested updates or
  // FlushPendingNotifications calls happen during it.
  NS_ASSERTION(aUpdateType && (aUpdateType & UPDATE_ALL) == aUpdateType,
               "Weird update type bitmask");
  if (aUpdateType != UPDATE_CONTENT_STATE && !mInNotification++) {
    FlushTags();
  }
}

void
nsContentSink::EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Update our notion of how much
  // has been flushed to include any new content if ending
  // this update leaves us not inside a notification.  Note that we
  // exclude UPDATE_CONTENT_STATE notifications here, since those
  // never affect the frame model directly while inside the
  // notification.
  NS_ASSERTION(aUpdateType && (aUpdateType & UPDATE_ALL) == aUpdateType,
               "Weird update type bitmask");
  if (aUpdateType != UPDATE_CONTENT_STATE && !--mInNotification) {
    UpdateChildCounts();
  }
}

void
nsContentSink::DidBuildModelImpl(void)
{
  if (mDocument && mDocument->GetDocumentTitle().IsVoid()) {
    nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
    dom_doc->SetTitle(EmptyString());
  }

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("nsContentSink::DidBuildModel: canceling notification "
                "timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = 0;
  }	
}

void
nsContentSink::DropParserAndPerfHint(void)
{
  // Ref. Bug 49115
  // Do this hack to make sure that the parser
  // doesn't get destroyed, accidently, before
  // the circularity, between sink & parser, is
  // actually borken.
  nsCOMPtr<nsIParser> kungFuDeathGrip(mParser);

  // Drop our reference to the parser to get rid of a circular
  // reference.
  mParser = nsnull;

  if (mDynamicLowerValue) {
    // Reset the performance hint which was set to FALSE
    // when mDynamicLowerValue was set.
    FavorPerformanceHint(PR_TRUE, 0);
  }

  if (mCanInterruptParser) {
    // Note: Don't return value from RemoveDummyParserRequest,
    // If RemoveDummyParserRequests fails it should not affect
    // DidBuildModel. The remove can fail if the parser request
    // was already removed by a DummyParserRequest::Cancel
    RemoveDummyParserRequest();
  }
}

nsresult
nsContentSink::WillProcessTokensImpl(void)
{
  if (mCanInterruptParser) {
    mDelayTimerStart = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  return NS_OK;
}

void
nsContentSink::WillBuildModelImpl()
{
  if (mCanInterruptParser) {
    nsresult rv = AddDummyParserRequest();
    if (NS_FAILED(rv)) {
      NS_ERROR("Adding dummy parser request failed");

      // Don't return the error result, just reset flag which
      // indicates that it can interrupt parsing. If
      // AddDummyParserRequests fails it should not affect
      // WillBuildModel.
      mCanInterruptParser = PR_FALSE;
    }

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mScrolledToRefAlready = PR_FALSE;
}

// If the content sink can interrupt the parser (@see mCanInteruptParsing)
// then it needs to schedule a dummy parser request to delay the document
// from firing onload handlers and other document done actions until all of the
// parsing has completed.

nsresult
nsContentSink::AddDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(!mDummyParserRequest, "Already have a dummy parser request");

  mDummyParserRequest = new DummyParserRequest(this);
  if (!mDummyParserRequest) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
  }

  if (loadGroup) {
    rv = mDummyParserRequest->SetLoadGroup(loadGroup);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = loadGroup->AddRequest(mDummyParserRequest, nsnull);
  }

  return rv;
}

nsresult
nsContentSink::RemoveDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
  }

  if (loadGroup && mDummyParserRequest) {
    rv = loadGroup->RemoveRequest(mDummyParserRequest, nsnull, NS_OK);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mDummyParserRequest = nsnull;
  }

  return rv;
}

