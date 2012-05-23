/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#include "nsContentSink.h"
#include "nsScriptLoader.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "mozilla/css/Loader.h"
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
#include "nsContentErrors.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIViewManager.h"
#include "nsIContentViewer.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsNetCID.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMLoadStatus.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsWeakReference.h"
#include "nsUnicharUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsIRequest.h"
#include "nsNodeUtils.h"
#include "nsIDOMNode.h"
#include "nsThreadUtils.h"
#include "nsPIDOMWindow.h"
#include "mozAutoDocUpdate.h"
#include "nsIWebNavigation.h"
#include "nsIDocumentLoader.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/Preferences.h"
#include "nsParserConstants.h"

using namespace mozilla;

PRLogModuleInfo* gContentSinkLogModuleInfo;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsContentSink)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsContentSink)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsContentSink)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsContentSink)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsContentSink)
  if (tmp->mDocument) {
    tmp->mDocument->RemoveObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptLoader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mNodeInfoManager,
                                                  nsNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


nsContentSink::nsContentSink()
{
  // We have a zeroing operator new
  NS_ASSERTION(!mLayoutStarted, "What?");
  NS_ASSERTION(!mDynamicLowerValue, "What?");
  NS_ASSERTION(!mParsing, "What?");
  NS_ASSERTION(mLastSampledUserEventTime == 0, "What?");
  NS_ASSERTION(mDeflectedCount == 0, "What?");
  NS_ASSERTION(!mDroppedTimer, "What?");
  NS_ASSERTION(mInMonolithicContainer == 0, "What?");
  NS_ASSERTION(mInNotification == 0, "What?");
  NS_ASSERTION(!mDeferredLayoutStart, "What?");

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

bool    nsContentSink::sNotifyOnTimer;
PRInt32 nsContentSink::sBackoffCount;
PRInt32 nsContentSink::sNotificationInterval;
PRInt32 nsContentSink::sInteractiveDeflectCount;
PRInt32 nsContentSink::sPerfDeflectCount;
PRInt32 nsContentSink::sPendingEventMode;
PRInt32 nsContentSink::sEventProbeRate;
PRInt32 nsContentSink::sInteractiveParseTime;
PRInt32 nsContentSink::sPerfParseTime;
PRInt32 nsContentSink::sInteractiveTime;
PRInt32 nsContentSink::sInitialPerfTime;
PRInt32 nsContentSink::sEnablePerfMode;

void
nsContentSink::InitializeStatics()
{
  Preferences::AddBoolVarCache(&sNotifyOnTimer,
                               "content.notify.ontimer", true);
  // -1 means never.
  Preferences::AddIntVarCache(&sBackoffCount,
                              "content.notify.backoffcount", -1);
  // The gNotificationInterval has a dramatic effect on how long it
  // takes to initially display content for slow connections.
  // The current value provides good
  // incremental display of content without causing an increase
  // in page load time. If this value is set below 1/10 of second
  // it starts to impact page load performance.
  // see bugzilla bug 72138 for more info.
  Preferences::AddIntVarCache(&sNotificationInterval,
                              "content.notify.interval", 120000);
  Preferences::AddIntVarCache(&sInteractiveDeflectCount,
                              "content.sink.interactive_deflect_count", 0);
  Preferences::AddIntVarCache(&sPerfDeflectCount,
                              "content.sink.perf_deflect_count", 200);
  Preferences::AddIntVarCache(&sPendingEventMode,
                              "content.sink.pending_event_mode", 1);
  Preferences::AddIntVarCache(&sEventProbeRate,
                              "content.sink.event_probe_rate", 1);
  Preferences::AddIntVarCache(&sInteractiveParseTime,
                              "content.sink.interactive_parse_time", 3000);
  Preferences::AddIntVarCache(&sPerfParseTime,
                              "content.sink.perf_parse_time", 360000);
  Preferences::AddIntVarCache(&sInteractiveTime,
                              "content.sink.interactive_time", 750000);
  Preferences::AddIntVarCache(&sInitialPerfTime,
                              "content.sink.initial_perf_time", 2000000);
  Preferences::AddIntVarCache(&sEnablePerfMode,
                              "content.sink.enable_perf_mode", 0);
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
  mDocShell = do_QueryInterface(aContainer);
  mScriptLoader = mDocument->ScriptLoader();

  if (!mRunsToCompletion) {
    if (mDocShell) {
      PRUint32 loadType = 0;
      mDocShell->GetLoadType(&loadType);
      mDocument->SetChangeScrollPosWhenScrollingToRef(
        (loadType & nsIDocShell::LOAD_CMD_HISTORY) == 0);
    }

    ProcessHTTPHeaders(aChannel);
  }

  mCSSLoader = aDoc->CSSLoader();

  mNodeInfoManager = aDoc->NodeInfoManager();

  mBackoffCount = sBackoffCount;

  if (sEnablePerfMode != 0) {
    mDynamicLowerValue = sEnablePerfMode == 1;
    FavorPerformanceHint(!mDynamicLowerValue, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::StyleSheetLoaded(nsCSSStyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus)
{
  NS_ASSERTION(!mRunsToCompletion, "How come a fragment parser observed sheets?");
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
        StartLayout(false);
      }

      // Go ahead and try to scroll to our ref if we have one
      ScrollToRef();
    }
    
    mScriptLoader->RemoveExecuteBlocker();
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
    mDocument->SetHeaderData(nsGkAtoms::link,
                             NS_ConvertASCIItoUTF16(linkHeader));

    NS_ASSERTION(!mProcessLinkHeaderEvent.get(),
                 "Already dispatched an event?");

    mProcessLinkHeaderEvent =
      NS_NewNonOwningRunnableMethod(this,
        &nsContentSink::DoProcessLinkHeader);
    rv = NS_DispatchToCurrentThread(mProcessLinkHeaderEvent.get());
    if (NS_FAILED(rv)) {
      mProcessLinkHeaderEvent.Forget();
    }
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
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mDocument->GetScriptGlobalObject());
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
  else if (aHeader == nsGkAtoms::msthemecompatible) {
    // Disable theming for the presshell if the value is no.
    // XXXbz don't we want to support this as an HTTP header too?
    nsAutoString value(aValue);
    if (value.LowerCaseEqualsLiteral("no")) {
      nsIPresShell* shell = mDocument->GetShell();
      if (shell) {
        shell->DisableThemeSupport();
      }
    }
  }

  return rv;
}


void
nsContentSink::DoProcessLinkHeader()
{
  nsAutoString value;
  mDocument->GetHeaderData(nsGkAtoms::link, value);
  ProcessLinkHeader(nsnull, value);
}

// check whether the Link header field applies to the context resource
// see <http://tools.ietf.org/html/rfc5988#section-5.2>

bool
nsContentSink::LinkContextIsOurDocument(const nsSubstring& aAnchor)
{
  if (aAnchor.IsEmpty()) {
    // anchor parameter not present or empty -> same document reference
    return true;
  }

  nsIURI* docUri = mDocument->GetDocumentURI();

  // the document URI might contain a fragment identifier ("#...')
  // we want to ignore that because it's invisible to the server
  // and just affects the local interpretation in the recipient
  nsCOMPtr<nsIURI> contextUri;
  nsresult rv = docUri->CloneIgnoringRef(getter_AddRefs(contextUri));
  
  if (NS_FAILED(rv)) {
    // copying failed
    return false;
  }
  
  // resolve anchor against context    
  nsCOMPtr<nsIURI> resolvedUri;
  rv = NS_NewURI(getter_AddRefs(resolvedUri), aAnchor,
      nsnull, contextUri);
  
  if (NS_FAILED(rv)) {
    // resolving failed
    return false;
  }

  bool same;
  rv = contextUri->Equals(resolvedUri, &same); 
  if (NS_FAILED(rv)) {
    // comparison failed
    return false;
  }

  return same;
}

// Decode a parameter value using the encoding defined in RFC 5987 (in place)
//
//   charset  "'" [ language ] "'" value-chars
//
// returns true when decoding happened successfully (otherwise leaves
// passed value alone)
bool
nsContentSink::Decode5987Format(nsAString& aEncoded) {

  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
  do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return false;

  nsCAutoString asciiValue;

  const PRUnichar* encstart = aEncoded.BeginReading();
  const PRUnichar* encend = aEncoded.EndReading();

  // create a plain ASCII string, aborting if we can't do that
  // converted form is always shorter than input
  while (encstart != encend) {
    if (*encstart > 0 && *encstart < 128) {
      asciiValue.Append((char)*encstart);
    } else {
      return false;
    }
    encstart++;
  }

  nsAutoString decoded;
  nsCAutoString language;

  rv = mimehdrpar->DecodeRFC5987Param(asciiValue, language, decoded);
  if (NS_FAILED(rv))
    return false;

  aEncoded = decoded;
  return true;
}

nsresult
nsContentSink::ProcessLinkHeader(nsIContent* aElement,
                                 const nsAString& aLinkData)
{
  nsresult rv = NS_OK;

  // keep track where we are within the header field
  bool seenParameters = false;

  // parse link content and call process style link
  nsAutoString href;
  nsAutoString rel;
  nsAutoString title;
  nsAutoString titleStar;
  nsAutoString type;
  nsAutoString media;
  nsAutoString anchor;

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

    bool wasQuotedString = false;
    
    // look for semicolon or comma
    while (*end != kNullCh && *end != kSemicolon && *end != kComma) {
      PRUnichar ch = *end;

      if (ch == kQuote || ch == kLessThan) {
        // quoted string

        PRUnichar quote = ch;
        if (quote == kLessThan) {
          quote = kGreaterThan;
        }
        
        wasQuotedString = (ch == kQuote);
        
        PRUnichar* closeQuote = (end + 1);

        // seek closing quote
        while (*closeQuote != kNullCh && quote != *closeQuote) {
          // in quoted-string, "\" is an escape character
          if (wasQuotedString && *closeQuote == kBackSlash && *(closeQuote + 1) != kNullCh) {
            ++closeQuote;
          }

          ++closeQuote;
        }

        if (quote == *closeQuote) {
          // found closer

          // skip to close quote
          end = closeQuote;

          last = end - 1;

          ch = *(end + 1);

          if (ch != kNullCh && ch != kSemicolon && ch != kComma) {
            // end string here
            *(++end) = kNullCh;

            ch = *(end + 1);

            // keep going until semi or comma
            while (ch != kNullCh && ch != kSemicolon && ch != kComma) {
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
      if ((*start == kLessThan) && (*last == kGreaterThan)) {
        *last = kNullCh;

        // first instance of <...> wins
        // also, do not allow hrefs after the first param was seen
        if (href.IsEmpty() && !seenParameters) {
          href = (start + 1);
          href.StripWhitespace();
        }
      } else {
        PRUnichar* equals = start;
        seenParameters = true;

        while ((*equals != kNullCh) && (*equals != kEqual)) {
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

          if ((*value == kQuote) && (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (wasQuotedString) {
            // unescape in-place
            PRUnichar* unescaped = value;
            PRUnichar *src = value;
            
            while (*src != kNullCh) {
              if (*src == kBackSlash && *(src + 1) != kNullCh) {
                src++;
              }
              *unescaped++ = *src++;
            }

            *unescaped = kNullCh;
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
          } else if (attr.LowerCaseEqualsLiteral("title*")) {
            if (titleStar.IsEmpty() && !wasQuotedString) {
              // RFC 5987 encoding; uses token format only, so skip if we get
              // here with a quoted-string
              nsAutoString tmp;
              tmp = value;
              if (Decode5987Format(tmp)) {
                titleStar = tmp;
                titleStar.CompressWhitespace();
              } else {
                // header value did not parse, throw it away
                titleStar.Truncate();
              }
            }
          } else if (attr.LowerCaseEqualsLiteral("type")) {
            if (type.IsEmpty()) {
              type = value;
              type.StripWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("media")) {
            if (media.IsEmpty()) {
              media = value;

              // The HTML5 spec is formulated in terms of the CSS3 spec,
              // which specifies that media queries are case insensitive.
              nsContentUtils::ASCIIToLower(media);
            }
          } else if (attr.LowerCaseEqualsLiteral("anchor")) {
            if (anchor.IsEmpty()) {
              anchor = value;
              anchor.StripWhitespace();
            }
          }
        }
      }
    }

    if (endCh == kComma) {
      // hit a comma, process what we've got so far

      href.Trim(" \t\n\r\f"); // trim HTML5 whitespace
      if (!href.IsEmpty() && !rel.IsEmpty()) {
        rv = ProcessLink(aElement, anchor, href, rel,
                         // prefer RFC 5987 variant over non-I18zed version
                         titleStar.IsEmpty() ? title : titleStar,
                         type, media);
      }

      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
      anchor.Truncate();
      
      seenParameters = false;
    }

    start = ++end;
  }
                
  href.Trim(" \t\n\r\f"); // trim HTML5 whitespace
  if (!href.IsEmpty() && !rel.IsEmpty()) {
    rv = ProcessLink(aElement, anchor, href, rel,
                     // prefer RFC 5987 variant over non-I18zed version
                     titleStar.IsEmpty() ? title : titleStar,
                     type, media);
  }

  return rv;
}


nsresult
nsContentSink::ProcessLink(nsIContent* aElement,
                           const nsSubstring& aAnchor, const nsSubstring& aHref,
                           const nsSubstring& aRel, const nsSubstring& aTitle,
                           const nsSubstring& aType, const nsSubstring& aMedia)
{
  PRUint32 linkTypes = nsStyleLinkElement::ParseLinkTypes(aRel);

  // The link relation may apply to a different resource, specified
  // in the anchor parameter. For the link relations supported so far,
  // we simply abort if the link applies to a resource different to the
  // one we've loaded
  if (!LinkContextIsOurDocument(aAnchor)) {
    return NS_OK;
  }
  
  bool hasPrefetch = linkTypes & PREFETCH;
  // prefetch href if relation is "next" or "prefetch"
  if (hasPrefetch || (linkTypes & NEXT)) {
    PrefetchHref(aHref, aElement, hasPrefetch);
  }

  if (!aHref.IsEmpty() && (linkTypes & DNS_PREFETCH)) {
    PrefetchDNS(aHref);
  }

  // is it a stylesheet link?
  if (!(linkTypes & STYLESHEET)) {
    return NS_OK;
  }

  bool isAlternate = linkTypes & ALTERNATE;
  return ProcessStyleLink(aElement, aHref, isAlternate, aTitle, aType,
                          aMedia);
}

nsresult
nsContentSink::ProcessStyleLink(nsIContent* aElement,
                                const nsSubstring& aHref,
                                bool aAlternate,
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
  nsContentUtils::SplitMimeType(aType, mimeType, params);

  // see bug 18817
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    // Unknown stylesheet language
    return NS_OK;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull,
                          mDocument->GetDocBaseURI());
  
  if (NS_FAILED(rv)) {
    // The URI is bad, move along, don't propagate the error (for now)
    return NS_OK;
  }

  // If this is a fragment parser, we don't want to observe.
  bool isAlternate;
  rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, aAlternate,
                                 mRunsToCompletion ? nsnull : this, &isAlternate);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!isAlternate && !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddExecuteBlocker();
  }

  return NS_OK;
}


nsresult
nsContentSink::ProcessMETATag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing meta-element");

  nsresult rv = NS_OK;

  // set any HTTP-EQUIV data into document's header data as well as url
  nsAutoString header;
  aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
  if (!header.IsEmpty()) {
    nsAutoString result;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::content, result);
    if (!result.IsEmpty()) {
      nsContentUtils::ASCIIToLower(header);
      nsCOMPtr<nsIAtom> fieldAtom(do_GetAtom(header));
      rv = ProcessHeaderData(fieldAtom, result, aContent); 
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                            nsGkAtoms::handheldFriendly, eIgnoreCase)) {
    nsAutoString result;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::content, result);
    if (!result.IsEmpty()) {
      nsContentUtils::ASCIIToLower(result);
      mDocument->SetHeaderData(nsGkAtoms::handheldFriendly, result);
    }
  }

  return rv;
}


void
nsContentSink::PrefetchHref(const nsAString &aHref,
                            nsIContent *aSource,
                            bool aExplicit)
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
    treeItem = do_QueryInterface(docshell);
    if (treeItem) {
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
              mDocument->GetDocBaseURI());
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
  else {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref);
    if (!uri) {
      return;
    }
    nsCAutoString host;
    uri->GetHost(host);
    CopyUTF8toUTF16(host, hostname);
  }

  if (!hostname.IsEmpty() && nsHTMLDNSPrefetch::IsAllowed(mDocument)) {
    nsHTMLDNSPrefetch::PrefetchLow(hostname);
  }
}

nsresult
nsContentSink::SelectDocAppCache(nsIApplicationCache *aLoadApplicationCache,
                                 nsIURI *aManifestURI,
                                 bool aFetchedWithHTTPGetOrEquiv,
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

    bool equal = false;
    rv = groupURI->Equals(aManifestURI, &equal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!equal) {
      // This is a foreign entry, force a reload to avoid loading the foreign
      // entry. The entry will be marked as foreign to avoid loading it again.

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
  if (aElement != mDocument->GetRootElement()) {
    return;
  }

  // Don't bother processing offline manifest for documents
  // without a docshell
  if (!mDocShell) {
    return;
  }

  // Check for a manifest= attribute.
  nsAutoString manifestSpec;
  aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest, manifestSpec);
  ProcessOfflineManifest(manifestSpec);
}

void
nsContentSink::ProcessOfflineManifest(const nsAString& aManifestSpec)
{
  // Don't bother processing offline manifest for documents
  // without a docshell
  if (!mDocShell) {
    return;
  }

  nsresult rv;

  // Grab the application cache the document was loaded from, if any.
  nsCOMPtr<nsIApplicationCache> applicationCache;

  nsCOMPtr<nsIApplicationCacheChannel> applicationCacheChannel =
    do_QueryInterface(mDocument->GetChannel());
  if (applicationCacheChannel) {
    bool loadedFromApplicationCache;
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

  if (aManifestSpec.IsEmpty() && !applicationCache) {
    // Not loaded from an application cache, and no manifest
    // attribute.  Nothing to do here.
    return;
  }

  CacheSelectionAction action = CACHE_SELECTION_NONE;
  nsCOMPtr<nsIURI> manifestURI;

  if (aManifestSpec.IsEmpty()) {
    action = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
  }
  else {
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(manifestURI),
                                              aManifestSpec, mDocument,
                                              mDocumentURI);
    if (!manifestURI) {
      return;
    }

    // Documents must list a manifest from the same origin
    rv = mDocument->NodePrincipal()->CheckMayLoad(manifestURI, true);
    if (NS_FAILED(rv)) {
      action = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
    }
    else {
      // Only continue if the document has permission to use offline APIs.
      if (!nsContentUtils::OfflineAppAllowed(mDocument->NodePrincipal())) {
        return;
      }

      bool fetchedWithHTTPGetOrEquiv = false;
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
    // The document has been loaded from a different offline cache group than
    // the manifest it refers to, i.e. this is a foreign entry, mark it as such 
    // and force a reload to avoid loading it.  The next attempt will not 
    // choose it.

    applicationCacheChannel->MarkOfflineCacheEntryAsForeign();

    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mDocShell);

    webNav->Stop(nsIWebNavigation::STOP_ALL);
    webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
    break;
  }
  default:
    NS_ASSERTION(false,
          "Cache selection algorithm didn't decide on proper action");
    break;
  }
}

void
nsContentSink::ScrollToRef()
{
  mDocument->ScrollToRef();
}

void
nsContentSink::StartLayout(bool aIgnorePendingSheets)
{
  if (mLayoutStarted) {
    // Nothing to do here
    return;
  }
  
  mDeferredLayoutStart = true;

  if (!aIgnorePendingSheets && WaitForPendingSheets()) {
    // Bail out; we'll start layout when the sheets load
    return;
  }

  mDeferredLayoutStart = false;

  // Notify on all our content.  If none of our presshells have started layout
  // yet it'll be a no-op except for updating our data structures, a la
  // UpdateChildCounts() (because we don't want to double-notify on whatever we
  // have right now).  If some of them _have_ started layout, we want to make
  // sure to flush tags instead of just calling UpdateChildCounts() after we
  // loop over the shells.
  FlushTags();

  mLayoutStarted = true;
  mLastNotificationTime = PR_Now();

  mDocument->SetMayStartLayout(true);
  nsCOMPtr<nsIPresShell> shell = mDocument->GetShell();
  // Make sure we don't call InitialReflow() for a shell that has
  // already called it. This can happen when the layout frame for
  // an iframe is constructed *between* the Embed() call for the
  // docshell in the iframe, and the content sink's call to OpenBody().
  // (Bug 153815)
  if (shell && !shell->DidInitialReflow()) {
    nsRect r = shell->GetPresContext()->GetVisibleArea();
    nsCOMPtr<nsIPresShell> shellGrip = shell;
    nsresult rv = shell->InitialReflow(r.width, r.height);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  mDocument->SetScrollToRef(mDocumentURI);
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
  
  {
    // Scope so we call EndUpdate before we decrease mInNotification
    MOZ_AUTO_DOC_UPDATE(mDocument, UPDATE_CONTENT_MODEL, !mBeganUpdate);
    nsNodeUtils::ContentAppended(aContainer,
                                 aContainer->GetChildAt(aStartIndex),
                                 aStartIndex);
    mLastNotificationTime = PR_Now();
  }

  mInNotification--;
}

NS_IMETHODIMP
nsContentSink::Notify(nsITimer *timer)
{
  if (mParsing) {
    // We shouldn't interfere with our normal DidProcessAToken logic
    mDroppedTimer = true;
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
    mDeferredFlushTags = true;
  } else {
    FlushTags();

    // Now try and scroll to the reference
    // XXX Should we scroll unconditionally for history loads??
    ScrollToRef();
  }

  mNotificationTimer = nsnull;
  return NS_OK;
}

bool
nsContentSink::IsTimeToNotify()
{
  if (!sNotifyOnTimer || !mLayoutStarted || !mBackoffCount ||
      mInMonolithicContainer) {
    return false;
  }

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
    return false;
  }

  PRTime now = PR_Now();
  PRInt64 interval, diff;

  LL_I2L(interval, GetNotificationInterval());
  LL_SUB(diff, now, mLastNotificationTime);

  if (LL_CMP(diff, >, interval)) {
    mBackoffCount--;
    return true;
  }

  return false;
}

nsresult
nsContentSink::WillInterruptImpl()
{
  nsresult result = NS_OK;

  SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("nsContentSink::WillInterrupt: this=%p", this));
#ifndef SINK_NO_INCREMENTAL
  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
  } else if (sNotifyOnTimer && mLayoutStarted) {
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
          mDroppedTimer = false;
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

  mParsing = false;

  return result;
}

nsresult
nsContentSink::WillResumeImpl()
{
  SINK_TRACE(gContentSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("nsContentSink::WillResume: this=%p", this));

  mParsing = true;

  return NS_OK;
}

nsresult
nsContentSink::DidProcessATokenImpl()
{
  if (mRunsToCompletion || !mParser) {
    return NS_OK;
  }

  // Get the current user event time
  nsIPresShell *shell = mDocument->GetShell();
  if (!shell) {
    // If there's no pres shell in the document, return early since
    // we're not laying anything out here.
    return NS_OK;
  }

  // Increase before comparing to gEventProbeRate
  ++mDeflectedCount;

  // Check if there's a pending event
  if (sPendingEventMode != 0 && !mHasPendingEvent &&
      (mDeflectedCount % sEventProbeRate) == 0) {
    nsIViewManager* vm = shell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    mHasPendingEvent = widget && widget->HasPendingInputEvent();
  }

  if (mHasPendingEvent && sPendingEventMode == 2) {
    return NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  // Have we processed enough tokens to check time?
  if (!mHasPendingEvent &&
      mDeflectedCount < PRUint32(mDynamicLowerValue ? sInteractiveDeflectCount :
                                                      sPerfDeflectCount)) {
    return NS_OK;
  }

  mDeflectedCount = 0;

  // Check if it's time to return to the main event loop
  if (PR_IntervalToMicroseconds(PR_IntervalNow()) > mCurrentParseEndTime) {
    return NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  return NS_OK;
}

//----------------------------------------------------------------------

void
nsContentSink::FavorPerformanceHint(bool perfOverStarvation, PRUint32 starvationDelay)
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
  if (mInNotification > 0 && mUpdatesInNotification < 2) {
    ++mUpdatesInNotification;
  }

  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Since this could result in frame
  // creation, make sure we've flushed everything before we
  // continue.

  if (!mInNotification++) {
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
  // this update leaves us not inside a notification.
  if (!--mInNotification) {
    UpdateChildCounts();
  }
}

void
nsContentSink::DidBuildModelImpl(bool aTerminated)
{
  if (mDocument && !aTerminated) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() ==
               nsIDocument::READYSTATE_LOADING, "Bad readyState");
    mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);
  }

  if (mScriptLoader) {
    mScriptLoader->ParsingComplete(aTerminated);
  }

  if (!mDocument->HaveFiredDOMTitleChange()) {
    mDocument->NotifyPossibleTitleChange(false);
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
  // actually broken.
  // Drop our reference to the parser to get rid of a circular
  // reference.
  nsRefPtr<nsParserBase> kungFuDeathGrip(mParser.forget());

  if (mDynamicLowerValue) {
    // Reset the performance hint which was set to FALSE
    // when mDynamicLowerValue was set.
    FavorPerformanceHint(true, 0);
  }

  if (!mRunsToCompletion) {
    mDocument->UnblockOnload(true);
  }
}

bool
nsContentSink::IsScriptExecutingImpl()
{
  return !!mScriptLoader->GetCurrentScript();
}

nsresult
nsContentSink::WillParseImpl(void)
{
  if (mRunsToCompletion) {
    return NS_OK;
  }

  nsIPresShell *shell = mDocument->GetShell();
  if (!shell) {
    return NS_OK;
  }

  PRUint32 currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  if (sEnablePerfMode == 0) {
    nsIViewManager* vm = shell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    PRUint32 lastEventTime;
    vm->GetLastUserEventTime(lastEventTime);

    bool newDynLower =
      mDocument->IsInBackgroundWindow() ||
      ((currentTime - mBeginLoadTime) > PRUint32(sInitialPerfTime) &&
       (currentTime - lastEventTime) < PRUint32(sInteractiveTime));
    
    if (mDynamicLowerValue != newDynLower) {
      FavorPerformanceHint(!newDynLower, 0);
      mDynamicLowerValue = newDynLower;
    }
  }
  
  mDeflectedCount = 0;
  mHasPendingEvent = false;

  mCurrentParseEndTime = currentTime +
    (mDynamicLowerValue ? sInteractiveParseTime : sPerfParseTime);

  return NS_OK;
}

void
nsContentSink::WillBuildModelImpl()
{
  if (!mRunsToCompletion) {
    mDocument->BlockOnload();

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mDocument->ResetScrolledToRefAlready();

  if (mProcessLinkHeaderEvent.get()) {
    mProcessLinkHeaderEvent.Revoke();

    DoProcessLinkHeader();
  }
}

/* static */
void
nsContentSink::NotifyDocElementCreated(nsIDocument* aDoc)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDoc);
    observerService->
      NotifyObservers(domDoc, "document-element-inserted",
                      EmptyString().get());
  }
}
