/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
#include "nsIDOMDocument.h"
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
#include "nsIContentViewer.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsNetCID.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMLoadStatus.h"
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
#include "nsThreadUtils.h"
#include "nsPresShellIterator.h"
#include "nsPIDOMWindow.h"
#include "mozAutoDocUpdate.h"
#include "nsIWebNavigation.h"
#include "nsIDocumentLoader.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLDNSPrefetch.h"

PRLogModuleInfo* gContentSinkLogModuleInfo;

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
                                             nsIURI *aURI,
                                             PRInt32 aLineNo)
{
  nsCOMPtr<nsIScriptLoaderObserver> inner = do_QueryReferent(mInner);

  if (inner) {
    return inner->ScriptAvailable(aResult, aElement, aIsInline, aURI,
                                  aLineNo);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoaderObserverProxy::ScriptEvaluated(nsresult aResult,
                                             nsIScriptElement *aElement,
                                             PRBool aIsInline)
{
  nsCOMPtr<nsIScriptLoaderObserver> inner = do_QueryReferent(mInner);

  if (inner) {
    return inner->ScriptEvaluated(aResult, aElement, aIsInline);
  }

  return NS_OK;
}


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsContentSink)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsContentSink)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsContentSink)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIScriptLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptLoaderObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsContentSink)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsContentSink)
  if (tmp->mDocument) {
    tmp->mDocument->RemoveObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNodeInfoManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mNodeInfoManager,
                                                  nsNodeInfoManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


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
  NS_ASSERTION(mDeferredLayoutStart == PR_FALSE, "What?");

#ifdef NS_DEBUG
  if (!gContentSinkLogModuleInfo) {
    gContentSinkLogModuleInfo = PR_NewLogModule("nscontentsink");
  }
#endif
}

nsContentSink::~nsContentSink()
{
  if (mDocument) {
    // Remove ourselves just to be safe, though we really should have
    // been removed in DidBuildModel if everything worked right.
    mDocument->RemoveObserver(this);
  }
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

  mScriptLoader = mDocument->ScriptLoader();
  mScriptLoader->AddObserver(proxy);

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
  if (!aWasAlternate) {
    NS_ASSERTION(mPendingSheetCount > 0, "How'd that happen?");
    --mPendingSheetCount;

    if (mPendingSheetCount == 0 &&
        (mDeferredLayoutStart || mDeferredFlushTags)) {
      if (mDeferredFlushTags) {
        FlushTags();
      }
      if (mDeferredLayoutStart) {
        // We might not have really started layout, since this sheet was still
        // loading.  Do it now.  Probably doesn't matter whether we do this
        // before or after we unblock scripts, but before feels saner.  Note
        // that if mDeferredLayoutStart is true, that means any subclass
        // StartLayout() stuff that needs to happen has already happened, so we
        // don't need to worry about it.
        StartLayout(PR_FALSE);
      }

      // Go ahead and try to scroll to our ref if we have one
      ScrollToRef();
    }
    
    mScriptLoader->RemoveExecuteBlocker();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptAvailable(nsresult aResult,
                               nsIScriptElement *aElement,
                               PRBool aIsInline,
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

    if (mParser && aResult != NS_BINDING_ABORTED) {
      // Loading external script failed!. So, resume parsing since the parser
      // got blocked when loading external script. See
      // http://bugzilla.mozilla.org/show_bug.cgi?id=94903.
      //
      // XXX We don't resume parsing if we get NS_BINDING_ABORTED from the
      //     script load, assuming that that error code means that the user
      //     stopped the load through some action (like clicking a link). See
      //     http://bugzilla.mozilla.org/show_bug.cgi?id=243392.
      ContinueInterruptedParsingAsync();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::ScriptEvaluated(nsresult aResult,
                               nsIScriptElement *aElement,
                               PRBool aIsInline)
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

  if (mParser && mParser->IsParserEnabled()) {
    ContinueInterruptedParsingAsync();
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
      nsIPresShell* shell = mDocument->GetPrimaryShell();
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
    PrefetchHref(aHref, aElement, hasPrefetch);
  }

  if ((!aHref.IsEmpty()) && linkTypes.IndexOf(NS_LITERAL_STRING("dns-prefetch")) != -1) {
    PrefetchDNS(aHref);
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

  PRBool isAlternate;
  rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, aAlternate,
                                 this, &isAlternate);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!isAlternate) {
    ++mPendingSheetCount;
    mScriptLoader->AddExecuteBlocker();
  }

  return NS_OK;
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
nsContentSink::PrefetchHref(const nsAString &aHref,
                            nsIContent *aSource,
                            PRBool aExplicit)
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
      nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(aSource);
      prefetchService->PrefetchURI(uri, mDocumentURI, domNode, aExplicit);
    }
  }
}

void
nsContentSink::PrefetchDNS(const nsAString &aHref)
{
  nsAutoString hostname;

  if (StringBeginsWith(aHref, NS_LITERAL_STRING("//")))  {
    hostname = Substring(aHref, 2);
  }
  else
    nsGenericHTMLElement::GetHostnameFromHrefString(aHref, hostname);

  if (nsHTMLDNSPrefetch::IsAllowed(mDocument)) {
    nsHTMLDNSPrefetch::PrefetchLow(hostname);
  }
}

nsresult
nsContentSink::GetChannelCacheKey(nsIChannel* aChannel, nsACString& aCacheKey)
{
  aCacheKey.Truncate();

  nsresult rv;
  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(aChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> token;
  rv = cachingChannel->GetCacheToken(getter_AddRefs(token));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheEntryDescriptor> descriptor = do_QueryInterface(token, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = descriptor->GetKey(aCacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsContentSink::SelectDocAppCache(nsIApplicationCache *aLoadApplicationCache,
                                 nsIURI *aManifestURI,
                                 PRBool aFetchedWithHTTPGetOrEquiv,
                                 CacheSelectionAction *aAction)
{
  nsresult rv;

  *aAction = CACHE_SELECTION_NONE;

  nsCOMPtr<nsIApplicationCacheContainer> applicationCacheDocument =
    do_QueryInterface(mDocument);
  NS_ASSERTION(applicationCacheDocument,
               "mDocument must implement nsIApplicationCacheContainer.");

  if (aLoadApplicationCache) {
    nsCAutoString groupID;
    rv = aLoadApplicationCache->GetGroupID(groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> groupURI;
    rv = NS_NewURI(getter_AddRefs(groupURI), groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool equal = PR_FALSE;
    rv = groupURI->Equals(aManifestURI, &equal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!equal) {
      // This is a foreign entry, mark it as such and force a reload to avoid
      // loading the foreign entry.  The next attempt will not choose this
      // cache entry (because it has been marked foreign).

      nsCAutoString cachekey;
      rv = GetChannelCacheKey(mDocument->GetChannel(), cachekey);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aLoadApplicationCache->MarkEntry(cachekey,
                                            nsIApplicationCache::ITEM_FOREIGN);
      NS_ENSURE_SUCCESS(rv, rv);

      *aAction = CACHE_SELECTION_RELOAD;
    }
    else {
      // The http manifest attribute URI is equal to the manifest URI of
      // the cache the document was loaded from - associate the document with
      // that cache and invoke the cache update process.
#ifdef NS_DEBUG
      nsCAutoString docURISpec, clientID;
      mDocumentURI->GetAsciiSpec(docURISpec);
      aLoadApplicationCache->GetClientID(clientID);
      SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
          ("Selection: assigning app cache %s to document %s", clientID.get(), docURISpec.get()));
#endif

      rv = applicationCacheDocument->SetApplicationCache(aLoadApplicationCache);
      NS_ENSURE_SUCCESS(rv, rv);

      // Document will be added as implicit entry to the cache as part of
      // the update process.
      *aAction = CACHE_SELECTION_UPDATE;
    }
  }
  else {
    // The document was not loaded from an application cache
    // Here we know the manifest has the same origin as the
    // document. There is call to CheckMayLoad() on it above.

    if (!aFetchedWithHTTPGetOrEquiv) {
      // The document was not loaded using HTTP GET or equivalent
      // method. The spec says to run the cache selection algorithm w/o
      // the manifest specified.
      *aAction = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
    }
    else {
      // Always do an update in this case
      *aAction = CACHE_SELECTION_UPDATE;
    }
  }

  return NS_OK;
}

nsresult
nsContentSink::SelectDocAppCacheNoManifest(nsIApplicationCache *aLoadApplicationCache,
                                           nsIURI **aManifestURI,
                                           CacheSelectionAction *aAction)
{
  *aManifestURI = nsnull;
  *aAction = CACHE_SELECTION_NONE;

  nsresult rv;

  if (aLoadApplicationCache) {
    // The document was loaded from an application cache, use that
    // application cache as the document's application cache.
    nsCOMPtr<nsIApplicationCacheContainer> applicationCacheDocument =
      do_QueryInterface(mDocument);
    NS_ASSERTION(applicationCacheDocument,
                 "mDocument must implement nsIApplicationCacheContainer.");

#ifdef NS_DEBUG
    nsCAutoString docURISpec, clientID;
    mDocumentURI->GetAsciiSpec(docURISpec);
    aLoadApplicationCache->GetClientID(clientID);
    SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
        ("Selection, no manifest: assigning app cache %s to document %s", clientID.get(), docURISpec.get()));
#endif

    rv = applicationCacheDocument->SetApplicationCache(aLoadApplicationCache);
    NS_ENSURE_SUCCESS(rv, rv);

    // Return the uri and invoke the update process for the selected
    // application cache.
    nsCAutoString groupID;
    rv = aLoadApplicationCache->GetGroupID(groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewURI(aManifestURI, groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    *aAction = CACHE_SELECTION_UPDATE;
  }

  return NS_OK;
}

void
nsContentSink::ProcessOfflineManifest(nsIContent *aElement)
{
  // Only check the manifest for root document nodes.
  if (aElement != mDocument->GetRootContent()) {
    return;
  }

  nsresult rv;

  // Check for a manifest= attribute.
  nsAutoString manifestSpec;
  aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest, manifestSpec);

  // Grab the application cache the document was loaded from, if any.
  nsCOMPtr<nsIApplicationCache> applicationCache;

  nsCOMPtr<nsIApplicationCacheChannel> applicationCacheChannel =
    do_QueryInterface(mDocument->GetChannel());
  if (applicationCacheChannel) {
    PRBool loadedFromApplicationCache;
    rv = applicationCacheChannel->GetLoadedFromApplicationCache(
      &loadedFromApplicationCache);
    if (NS_FAILED(rv)) {
      return;
    }

    if (loadedFromApplicationCache) {
      rv = applicationCacheChannel->GetApplicationCache(
        getter_AddRefs(applicationCache));
      if (NS_FAILED(rv)) {
        return;
      }
    }
  }

  if (manifestSpec.IsEmpty() && !applicationCache) {
    // Not loaded from an application cache, and no manifest
    // attribute.  Nothing to do here.
    return;
  }

  CacheSelectionAction action = CACHE_SELECTION_NONE;
  nsCOMPtr<nsIURI> manifestURI;

  if (manifestSpec.IsEmpty()) {
    action = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
  }
  else {
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(manifestURI),
                                              manifestSpec, mDocument,
                                              mDocumentURI);
    if (!manifestURI) {
      return;
    }

    // Documents must list a manifest from the same origin
    rv = mDocument->NodePrincipal()->CheckMayLoad(manifestURI, PR_TRUE);
    if (NS_FAILED(rv)) {
      action = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
    }
    else {
      // Only continue if the document has permission to use offline APIs.
      if (!nsContentUtils::OfflineAppAllowed(mDocument->NodePrincipal())) {
        return;
      }

      PRBool fetchedWithHTTPGetOrEquiv = PR_FALSE;
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mDocument->GetChannel()));
      if (httpChannel) {
        nsCAutoString method;
        rv = httpChannel->GetRequestMethod(method);
        if (NS_SUCCEEDED(rv))
          fetchedWithHTTPGetOrEquiv = method.Equals("GET");
      }

      rv = SelectDocAppCache(applicationCache, manifestURI,
                             fetchedWithHTTPGetOrEquiv, &action);
      if (NS_FAILED(rv)) {
        return;
      }
    }
  }

  if (action == CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST) {
    rv = SelectDocAppCacheNoManifest(applicationCache,
                                     getter_AddRefs(manifestURI),
                                     &action);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  switch (action)
  {
  case CACHE_SELECTION_NONE:
    break;
  case CACHE_SELECTION_UPDATE: {
    nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
      do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);

    if (updateService) {
      nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(mDocument);
      updateService->ScheduleOnDocumentStop(manifestURI, mDocumentURI, domdoc);
    }
    break;
  }
  case CACHE_SELECTION_RELOAD: {
    // This situation occurs only for toplevel documents, see bottom
    // of SelectDocAppCache method.
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mDocShell);

    webNav->Stop(nsIWebNavigation::STOP_ALL);
    webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
    break;
  }
  default:
    NS_ASSERTION(PR_FALSE,
          "Cache selection algorithm didn't decide on proper action");
    break;
  }
}

void
nsContentSink::ScrollToRef()
{
  if (mRef.IsEmpty()) {
    return;
  }

  if (mScrolledToRefAlready) {
    return;
  }

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

  nsPresShellIterator iter(mDocument);
  nsCOMPtr<nsIPresShell> shell;
  while ((shell = iter.GetNextShell())) {
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

nsresult
nsContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (!vm) {
    // vm might be null if the shell got Destroy() called already
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
nsContentSink::StartLayout(PRBool aIgnorePendingSheets)
{
  if (mLayoutStarted) {
    // Nothing to do here
    return;
  }
  
  mDeferredLayoutStart = PR_TRUE;

  if (!aIgnorePendingSheets && WaitForPendingSheets()) {
    // Bail out; we'll start layout when the sheets load
    return;
  }

  mDeferredLayoutStart = PR_FALSE;

  // Notify on all our content.  If none of our presshells have started layout
  // yet it'll be a no-op except for updating our data structures, a la
  // UpdateChildCounts() (because we don't want to double-notify on whatever we
  // have right now).  If some of them _have_ started layout, we want to make
  // sure to flush tags instead of just calling UpdateChildCounts() after we
  // loop over the shells.
  FlushTags();

  mLayoutStarted = PR_TRUE;
  mLastNotificationTime = PR_Now();

  mDocument->SetMayStartLayout(PR_TRUE);
  nsPresShellIterator iter(mDocument);
  nsCOMPtr<nsIPresShell> shell;
  while ((shell = iter.GetNextShell())) {
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

    nsRect r = shell->GetPresContext()->GetVisibleArea();
    nsCOMPtr<nsIPresShell> shellGrip = shell;
    nsresult rv = shell->InitialReflow(r.width, r.height);
    if (NS_FAILED(rv)) {
      return;
    }

    // Now trigger a refresh
    RefreshIfEnabled(shell->GetViewManager());
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

  {
    // Scope so we call EndUpdate before we decrease mInNotification
    MOZ_AUTO_DOC_UPDATE(mDocument, UPDATE_CONTENT_MODEL, !mBeganUpdate);
    nsNodeUtils::ContentAppended(aContainer, aStartIndex);
    mLastNotificationTime = PR_Now();
  }

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

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = PR_TRUE;
  } else {
    FlushTags();

    // Now try and scroll to the reference
    // XXX Should we scroll unconditionally for history loads??
    ScrollToRef();
  }

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

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = PR_TRUE;
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
  if (WaitForPendingSheets()) {
    mDeferredFlushTags = PR_TRUE;
  } else if (mNotifyOnTimer && mLayoutStarted) {
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
          ScrollToRef();
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
  nsIPresShell *shell = mDocument->GetPrimaryShell();

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
    static_cast<PRUint32>(((2 * mDynamicIntervalSwitchThreshold) +
                              NS_DELAY_FOR_WINDOW_CREATION));

  if ((currentTime - mBeginLoadTime) > delayBeforeLoweringThreshold) {
    if ((currentTime - eventTime) <
        static_cast<PRUint32>(mDynamicIntervalSwitchThreshold)) {

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
      static_cast<PRUint32>(GetMaxTokenProcessingTime())) {
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
  // Remember nested updates from updates that we started.
  if (mInNotification && mUpdatesInNotification < 2) {
    ++mUpdatesInNotification;
  }

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
  if (!mDocument->HaveFiredDOMTitleChange()) {
    mDocument->NotifyPossibleTitleChange(PR_FALSE);
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
  if (!mParser) {
    // Make sure we don't unblock unload too many times
    return;
  }
  
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
    mDocument->UnblockOnload(PR_TRUE);
  }
}

nsresult
nsContentSink::WillParseImpl(void)
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
    mDocument->BlockOnload();

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mScrolledToRefAlready = PR_FALSE;
}

void
nsContentSink::ContinueInterruptedParsing()
{
  if (mParser) {
    mParser->ContinueInterruptedParsing();
  }
}

void
nsContentSink::ContinueInterruptedParsingIfEnabled()
{
  if (mParser && mParser->IsParserEnabled()) {
    mParser->ContinueInterruptedParsing();
  }
}

void
nsContentSink::ContinueInterruptedParsingAsync()
{
  nsCOMPtr<nsIRunnable> ev = new nsRunnableMethod<nsContentSink>(this,
    &nsContentSink::ContinueInterruptedParsingIfEnabled);

  NS_DispatchToCurrentThread(ev);
}

// URIs: action, href, src, longdesc, usemap, cite
PRBool 
IsAttrURI(nsIAtom *aName)
{
  return (aName == nsGkAtoms::action ||
          aName == nsGkAtoms::href ||
          aName == nsGkAtoms::src ||
          aName == nsGkAtoms::longdesc ||
          aName == nsGkAtoms::usemap ||
          aName == nsGkAtoms::cite ||
          aName == nsGkAtoms::background);
}

//
// these two lists are used by the sanitizing fragment serializers
// Thanks to Mark Pilgrim and Sam Ruby for the initial whitelist
//
nsIAtom** const kDefaultAllowedTags [] = {
  &nsGkAtoms::a,
  &nsGkAtoms::abbr,
  &nsGkAtoms::acronym,
  &nsGkAtoms::address,
  &nsGkAtoms::area,
  &nsGkAtoms::b,
  &nsGkAtoms::bdo,
  &nsGkAtoms::big,
  &nsGkAtoms::blockquote,
  &nsGkAtoms::br,
  &nsGkAtoms::button,
  &nsGkAtoms::caption,
  &nsGkAtoms::center,
  &nsGkAtoms::cite,
  &nsGkAtoms::code,
  &nsGkAtoms::col,
  &nsGkAtoms::colgroup,
  &nsGkAtoms::dd,
  &nsGkAtoms::del,
  &nsGkAtoms::dfn,
  &nsGkAtoms::dir,
  &nsGkAtoms::div,
  &nsGkAtoms::dl,
  &nsGkAtoms::dt,
  &nsGkAtoms::em,
  &nsGkAtoms::fieldset,
  &nsGkAtoms::font,
  &nsGkAtoms::form,
  &nsGkAtoms::h1,
  &nsGkAtoms::h2,
  &nsGkAtoms::h3,
  &nsGkAtoms::h4,
  &nsGkAtoms::h5,
  &nsGkAtoms::h6,
  &nsGkAtoms::hr,
  &nsGkAtoms::i,
  &nsGkAtoms::img,
  &nsGkAtoms::input,
  &nsGkAtoms::ins,
  &nsGkAtoms::kbd,
  &nsGkAtoms::label,
  &nsGkAtoms::legend,
  &nsGkAtoms::li,
  &nsGkAtoms::listing,
  &nsGkAtoms::map,
  &nsGkAtoms::menu,
  &nsGkAtoms::nobr,
  &nsGkAtoms::ol,
  &nsGkAtoms::optgroup,
  &nsGkAtoms::option,
  &nsGkAtoms::p,
  &nsGkAtoms::pre,
  &nsGkAtoms::q,
  &nsGkAtoms::s,
  &nsGkAtoms::samp,
  &nsGkAtoms::select,
  &nsGkAtoms::small,
  &nsGkAtoms::span,
  &nsGkAtoms::strike,
  &nsGkAtoms::strong,
  &nsGkAtoms::sub,
  &nsGkAtoms::sup,
  &nsGkAtoms::table,
  &nsGkAtoms::tbody,
  &nsGkAtoms::td,
  &nsGkAtoms::textarea,
  &nsGkAtoms::tfoot,
  &nsGkAtoms::th,
  &nsGkAtoms::thead,
  &nsGkAtoms::tr,
  &nsGkAtoms::tt,
  &nsGkAtoms::u,
  &nsGkAtoms::ul,
  &nsGkAtoms::var,
  nsnull
};

nsIAtom** const kDefaultAllowedAttributes [] = {
  &nsGkAtoms::abbr,
  &nsGkAtoms::accept,
  &nsGkAtoms::acceptcharset,
  &nsGkAtoms::accesskey,
  &nsGkAtoms::action,
  &nsGkAtoms::align,
  &nsGkAtoms::alt,
  &nsGkAtoms::autocomplete,
  &nsGkAtoms::axis,
  &nsGkAtoms::background,
  &nsGkAtoms::bgcolor,
  &nsGkAtoms::border,
  &nsGkAtoms::cellpadding,
  &nsGkAtoms::cellspacing,
  &nsGkAtoms::_char,
  &nsGkAtoms::charoff,
  &nsGkAtoms::charset,
  &nsGkAtoms::checked,
  &nsGkAtoms::cite,
  &nsGkAtoms::_class,
  &nsGkAtoms::clear,
  &nsGkAtoms::cols,
  &nsGkAtoms::colspan,
  &nsGkAtoms::color,
  &nsGkAtoms::compact,
  &nsGkAtoms::coords,
  &nsGkAtoms::datetime,
  &nsGkAtoms::dir,
  &nsGkAtoms::disabled,
  &nsGkAtoms::enctype,
  &nsGkAtoms::_for,
  &nsGkAtoms::frame,
  &nsGkAtoms::headers,
  &nsGkAtoms::height,
  &nsGkAtoms::href,
  &nsGkAtoms::hreflang,
  &nsGkAtoms::hspace,
  &nsGkAtoms::id,
  &nsGkAtoms::ismap,
  &nsGkAtoms::label,
  &nsGkAtoms::lang,
  &nsGkAtoms::longdesc,
  &nsGkAtoms::maxlength,
  &nsGkAtoms::media,
  &nsGkAtoms::method,
  &nsGkAtoms::multiple,
  &nsGkAtoms::name,
  &nsGkAtoms::nohref,
  &nsGkAtoms::noshade,
  &nsGkAtoms::nowrap,
  &nsGkAtoms::pointSize,
  &nsGkAtoms::prompt,
  &nsGkAtoms::readonly,
  &nsGkAtoms::rel,
  &nsGkAtoms::rev,
  &nsGkAtoms::role,
  &nsGkAtoms::rows,
  &nsGkAtoms::rowspan,
  &nsGkAtoms::rules,
  &nsGkAtoms::scope,
  &nsGkAtoms::selected,
  &nsGkAtoms::shape,
  &nsGkAtoms::size,
  &nsGkAtoms::span,
  &nsGkAtoms::src,
  &nsGkAtoms::start,
  &nsGkAtoms::summary,
  &nsGkAtoms::tabindex,
  &nsGkAtoms::target,
  &nsGkAtoms::title,
  &nsGkAtoms::type,
  &nsGkAtoms::usemap,
  &nsGkAtoms::valign,
  &nsGkAtoms::value,
  &nsGkAtoms::vspace,
  &nsGkAtoms::width,
  nsnull
};
