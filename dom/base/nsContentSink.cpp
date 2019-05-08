/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#include "nsContentSink.h"
#include "mozilla/Components.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/SRILogHelper.h"
#include "nsStyleLinkElement.h"
#include "nsIDocShell.h"
#include "nsILoadContext.h"
#include "nsIPrefetchService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsViewManager.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindowInner.h"
#include "nsNetCID.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsICookieService.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "mozAutoDocUpdate.h"
#include "nsIWebNavigation.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsIObserverService.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsParserConstants.h"
#include "nsSandboxFlags.h"
#include "Link.h"
#include "HTMLLinkElement.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

LazyLogModule gContentSinkLogModuleInfo("nscontentsink");

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsContentSink)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsContentSink)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsContentSink)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsContentSink)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsContentSink)
  if (tmp->mDocument) {
    tmp->mDocument->RemoveObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCSSLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScriptLoader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCSSLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsContentSink::nsContentSink()
    : mBackoffCount(0),
      mLastNotificationTime(0),
      mLayoutStarted(0),
      mDynamicLowerValue(0),
      mParsing(0),
      mDroppedTimer(0),
      mDeferredLayoutStart(0),
      mDeferredFlushTags(0),
      mIsDocumentObserver(0),
      mRunsToCompletion(0),
      mIsBlockingOnload(false),
      mDeflectedCount(0),
      mHasPendingEvent(false),
      mCurrentParseEndTime(0),
      mBeginLoadTime(0),
      mLastSampledUserEventTime(0),
      mInMonolithicContainer(0),
      mInNotification(0),
      mUpdatesInNotification(0),
      mPendingSheetCount(0) {
  NS_ASSERTION(!mLayoutStarted, "What?");
  NS_ASSERTION(!mDynamicLowerValue, "What?");
  NS_ASSERTION(!mParsing, "What?");
  NS_ASSERTION(mLastSampledUserEventTime == 0, "What?");
  NS_ASSERTION(mDeflectedCount == 0, "What?");
  NS_ASSERTION(!mDroppedTimer, "What?");
  NS_ASSERTION(mInMonolithicContainer == 0, "What?");
  NS_ASSERTION(mInNotification == 0, "What?");
  NS_ASSERTION(!mDeferredLayoutStart, "What?");
}

nsContentSink::~nsContentSink() {
  if (mDocument) {
    // Remove ourselves just to be safe, though we really should have
    // been removed in DidBuildModel if everything worked right.
    mDocument->RemoveObserver(this);
  }
}

bool nsContentSink::sNotifyOnTimer;
int32_t nsContentSink::sBackoffCount;
int32_t nsContentSink::sNotificationInterval;
int32_t nsContentSink::sInteractiveDeflectCount;
int32_t nsContentSink::sPerfDeflectCount;
int32_t nsContentSink::sPendingEventMode;
int32_t nsContentSink::sEventProbeRate;
int32_t nsContentSink::sInteractiveParseTime;
int32_t nsContentSink::sPerfParseTime;
int32_t nsContentSink::sInteractiveTime;
int32_t nsContentSink::sInitialPerfTime;
int32_t nsContentSink::sEnablePerfMode;

void nsContentSink::InitializeStatics() {
  Preferences::AddBoolVarCache(&sNotifyOnTimer, "content.notify.ontimer", true);
  // -1 means never.
  Preferences::AddIntVarCache(&sBackoffCount, "content.notify.backoffcount",
                              -1);
  // The gNotificationInterval has a dramatic effect on how long it
  // takes to initially display content for slow connections.
  // The current value provides good
  // incremental display of content without causing an increase
  // in page load time. If this value is set below 1/10 of second
  // it starts to impact page load performance.
  // see bugzilla bug 72138 for more info.
  Preferences::AddIntVarCache(&sNotificationInterval, "content.notify.interval",
                              120000);
  Preferences::AddIntVarCache(&sInteractiveDeflectCount,
                              "content.sink.interactive_deflect_count", 0);
  Preferences::AddIntVarCache(&sPerfDeflectCount,
                              "content.sink.perf_deflect_count", 200);
  Preferences::AddIntVarCache(&sPendingEventMode,
                              "content.sink.pending_event_mode", 1);
  Preferences::AddIntVarCache(&sEventProbeRate, "content.sink.event_probe_rate",
                              1);
  Preferences::AddIntVarCache(&sInteractiveParseTime,
                              "content.sink.interactive_parse_time", 3000);
  Preferences::AddIntVarCache(&sPerfParseTime, "content.sink.perf_parse_time",
                              360000);
  Preferences::AddIntVarCache(&sInteractiveTime,
                              "content.sink.interactive_time", 750000);
  Preferences::AddIntVarCache(&sInitialPerfTime,
                              "content.sink.initial_perf_time", 2000000);
  Preferences::AddIntVarCache(&sEnablePerfMode, "content.sink.enable_perf_mode",
                              0);
}

nsresult nsContentSink::Init(Document* aDoc, nsIURI* aURI,
                             nsISupports* aContainer, nsIChannel* aChannel) {
  MOZ_ASSERT(aDoc, "null ptr");
  MOZ_ASSERT(aURI, "null ptr");

  if (!aDoc || !aURI) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;

  mDocumentURI = aURI;
  mDocShell = do_QueryInterface(aContainer);
  mScriptLoader = mDocument->ScriptLoader();

  if (!mRunsToCompletion) {
    if (mDocShell) {
      uint32_t loadType = 0;
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
nsContentSink::StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                                nsresult aStatus) {
  MOZ_ASSERT(!mRunsToCompletion, "How come a fragment parser observed sheets?");
  if (!aWasDeferred) {
    MOZ_ASSERT(mPendingSheetCount > 0, "How'd that happen?");
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

    mScriptLoader->RemoveParserBlockingScriptExecutionBlocker();
  }

  return NS_OK;
}

nsresult nsContentSink::ProcessHTTPHeaders(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannel> httpchannel(do_QueryInterface(aChannel));

  if (!httpchannel) {
    return NS_OK;
  }

  // Note that the only header we care about is the "link" header, since we
  // have all the infrastructure for kicking off stylesheet loads.

  nsAutoCString linkHeader;

  nsresult rv =
      httpchannel->GetResponseHeader(NS_LITERAL_CSTRING("link"), linkHeader);
  if (NS_SUCCEEDED(rv) && !linkHeader.IsEmpty()) {
    mDocument->SetHeaderData(nsGkAtoms::link,
                             NS_ConvertASCIItoUTF16(linkHeader));

    NS_ASSERTION(!mProcessLinkHeaderEvent.get(),
                 "Already dispatched an event?");

    mProcessLinkHeaderEvent =
        NewNonOwningRunnableMethod("nsContentSink::DoProcessLinkHeader", this,
                                   &nsContentSink::DoProcessLinkHeader);
    rv = NS_DispatchToCurrentThread(mProcessLinkHeaderEvent.get());
    if (NS_FAILED(rv)) {
      mProcessLinkHeaderEvent.Forget();
    }
  }

  return NS_OK;
}

nsresult nsContentSink::ProcessHeaderData(nsAtom* aHeader,
                                          const nsAString& aValue,
                                          nsIContent* aContent) {
  nsresult rv = NS_OK;
  // necko doesn't process headers coming in from the parser

  mDocument->SetHeaderData(aHeader, aValue);

  if (aHeader == nsGkAtoms::setcookie &&
      StaticPrefs::dom_metaElement_setCookie_allowed()) {
    // Note: Necko already handles cookies set via the channel.  We can't just
    // call SetCookie on the channel because we want to do some security checks
    // here.
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

    nsCOMPtr<nsIChannel> channel;
    if (mParser) {
      mParser->GetChannel(getter_AddRefs(channel));
    }

    rv = cookieServ->SetCookieString(
        codebaseURI, nullptr, NS_ConvertUTF16toUTF8(aValue).get(), channel);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return rv;
}

void nsContentSink::DoProcessLinkHeader() {
  nsAutoString value;
  mDocument->GetHeaderData(nsGkAtoms::link, value);
  ProcessLinkHeader(value);
}

// check whether the Link header field applies to the context resource
// see <http://tools.ietf.org/html/rfc5988#section-5.2>

bool nsContentSink::LinkContextIsOurDocument(const nsAString& aAnchor) {
  if (aAnchor.IsEmpty()) {
    // anchor parameter not present or empty -> same document reference
    return true;
  }

  nsIURI* docUri = mDocument->GetDocumentURI();

  // the document URI might contain a fragment identifier ("#...')
  // we want to ignore that because it's invisible to the server
  // and just affects the local interpretation in the recipient
  nsCOMPtr<nsIURI> contextUri;
  nsresult rv = NS_GetURIWithoutRef(docUri, getter_AddRefs(contextUri));

  if (NS_FAILED(rv)) {
    // copying failed
    return false;
  }

  // resolve anchor against context
  nsCOMPtr<nsIURI> resolvedUri;
  rv = NS_NewURI(getter_AddRefs(resolvedUri), aAnchor, nullptr, contextUri);

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
bool nsContentSink::Decode5987Format(nsAString& aEncoded) {
  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
      do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return false;

  nsAutoCString asciiValue;

  const char16_t* encstart = aEncoded.BeginReading();
  const char16_t* encend = aEncoded.EndReading();

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
  nsAutoCString language;

  rv = mimehdrpar->DecodeRFC5987Param(asciiValue, language, decoded);
  if (NS_FAILED(rv)) return false;

  aEncoded = decoded;
  return true;
}

nsresult nsContentSink::ProcessLinkHeader(const nsAString& aLinkData) {
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
  nsAutoString crossOrigin;
  nsAutoString referrerPolicy;
  nsAutoString as;

  crossOrigin.SetIsVoid(true);

  // copy to work buffer
  nsAutoString stringList(aLinkData);

  // put an extra null at the end
  stringList.Append(kNullCh);

  char16_t* start = stringList.BeginWriting();
  char16_t* end = start;
  char16_t* last = start;
  char16_t endCh;

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
      char16_t ch = *end;

      if (ch == kQuote || ch == kLessThan) {
        // quoted string

        char16_t quote = ch;
        if (quote == kLessThan) {
          quote = kGreaterThan;
        }

        wasQuotedString = (ch == kQuote);

        char16_t* closeQuote = (end + 1);

        // seek closing quote
        while (*closeQuote != kNullCh && quote != *closeQuote) {
          // in quoted-string, "\" is an escape character
          if (wasQuotedString && *closeQuote == kBackSlash &&
              *(closeQuote + 1) != kNullCh) {
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

              ch = *(end + 1);
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
        char16_t* equals = start;
        seenParameters = true;

        while ((*equals != kNullCh) && (*equals != kEqual)) {
          equals++;
        }

        if (*equals != kNullCh) {
          *equals = kNullCh;
          nsAutoString attr(start);
          attr.StripWhitespace();

          char16_t* value = ++equals;
          while (nsCRT::IsAsciiSpace(*value)) {
            value++;
          }

          if ((*value == kQuote) && (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (wasQuotedString) {
            // unescape in-place
            char16_t* unescaped = value;
            char16_t* src = value;

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
          } else if (attr.LowerCaseEqualsLiteral("crossorigin")) {
            if (crossOrigin.IsVoid()) {
              crossOrigin.SetIsVoid(false);
              crossOrigin = value;
              crossOrigin.StripWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("as")) {
            if (as.IsEmpty()) {
              as = value;
              as.CompressWhitespace();
            }
          } else if (attr.LowerCaseEqualsLiteral("referrerpolicy")) {
            // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#referrer-policy-attribute
            // Specs says referrer policy attribute is an enumerated attribute,
            // case insensitive and includes the empty string
            // We will parse the value with AttributeReferrerPolicyFromString
            // later, which will handle parsing it as an enumerated attribute.
            if (referrerPolicy.IsEmpty()) {
              referrerPolicy = value;
            }
          }
        }
      }
    }

    if (endCh == kComma) {
      // hit a comma, process what we've got so far

      href.Trim(" \t\n\r\f");  // trim HTML5 whitespace
      if (!href.IsEmpty() && !rel.IsEmpty()) {
        rv = ProcessLinkFromHeader(
            anchor, href, rel,
            // prefer RFC 5987 variant over non-I18zed version
            titleStar.IsEmpty() ? title : titleStar, type, media, crossOrigin,
            referrerPolicy, as);
      }

      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
      anchor.Truncate();
      referrerPolicy.Truncate();
      crossOrigin.SetIsVoid(true);
      as.Truncate();

      seenParameters = false;
    }

    start = ++end;
  }

  href.Trim(" \t\n\r\f");  // trim HTML5 whitespace
  if (!href.IsEmpty() && !rel.IsEmpty()) {
    rv =
        ProcessLinkFromHeader(anchor, href, rel,
                              // prefer RFC 5987 variant over non-I18zed version
                              titleStar.IsEmpty() ? title : titleStar, type,
                              media, crossOrigin, referrerPolicy, as);
  }

  return rv;
}

nsresult nsContentSink::ProcessLinkFromHeader(
    const nsAString& aAnchor, const nsAString& aHref, const nsAString& aRel,
    const nsAString& aTitle, const nsAString& aType, const nsAString& aMedia,
    const nsAString& aCrossOrigin, const nsAString& aReferrerPolicy,
    const nsAString& aAs) {
  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(aRel);

  // The link relation may apply to a different resource, specified
  // in the anchor parameter. For the link relations supported so far,
  // we simply abort if the link applies to a resource different to the
  // one we've loaded
  if (!LinkContextIsOurDocument(aAnchor)) {
    return NS_OK;
  }

  if (nsContentUtils::PrefetchPreloadEnabled(mDocShell)) {
    // prefetch href if relation is "next" or "prefetch"
    if ((linkTypes & nsStyleLinkElement::eNEXT) ||
        (linkTypes & nsStyleLinkElement::ePREFETCH) ||
        (linkTypes & nsStyleLinkElement::ePRELOAD)) {
      PrefetchPreloadHref(aHref, mDocument, linkTypes, aAs, aType, aMedia);
    }

    if (!aHref.IsEmpty() && (linkTypes & nsStyleLinkElement::eDNS_PREFETCH)) {
      PrefetchDNS(aHref);
    }

    if (!aHref.IsEmpty() && (linkTypes & nsStyleLinkElement::ePRECONNECT)) {
      Preconnect(aHref, aCrossOrigin);
    }
  }

  // is it a stylesheet link?
  if (!(linkTypes & nsStyleLinkElement::eSTYLESHEET)) {
    return NS_OK;
  }

  bool isAlternate = linkTypes & nsStyleLinkElement::eALTERNATE;
  return ProcessStyleLinkFromHeader(aHref, isAlternate, aTitle, aType, aMedia,
                                    aReferrerPolicy);
}

nsresult nsContentSink::ProcessStyleLinkFromHeader(
    const nsAString& aHref, bool aAlternate, const nsAString& aTitle,
    const nsAString& aType, const nsAString& aMedia,
    const nsAString& aReferrerPolicy) {
  if (aAlternate && aTitle.IsEmpty()) {
    // alternates must have title return without error, for now
    return NS_OK;
  }

  nsAutoString mimeType;
  nsAutoString params;
  nsContentUtils::SplitMimeType(aType, mimeType, params);

  // see bug 18817
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    // Unknown stylesheet language
    return NS_OK;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aHref, nullptr,
                          mDocument->GetDocBaseURI());

  if (NS_FAILED(rv)) {
    // The URI is bad, move along, don't propagate the error (for now)
    return NS_OK;
  }

  Loader::SheetInfo info{
      *mDocument,
      nullptr,
      url.forget(),
      nullptr,
      net::AttributeReferrerPolicyFromString(aReferrerPolicy),
      CORS_NONE,
      aTitle,
      aMedia,
      aAlternate ? Loader::HasAlternateRel::Yes : Loader::HasAlternateRel::No,
      Loader::IsInline::No,
      Loader::IsExplicitlyEnabled::No,
  };

  auto loadResultOrErr =
      mCSSLoader->LoadStyleLink(info, mRunsToCompletion ? nullptr : this);
  if (loadResultOrErr.isErr()) {
    return loadResultOrErr.unwrapErr();
  }

  if (loadResultOrErr.unwrap().ShouldBlock() && !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddParserBlockingScriptExecutionBlocker();
  }

  return NS_OK;
}

nsresult nsContentSink::ProcessMETATag(nsIContent* aContent) {
  NS_ASSERTION(aContent, "missing meta-element");
  MOZ_ASSERT(aContent->IsElement());

  Element* element = aContent->AsElement();

  nsresult rv = NS_OK;

  // set any HTTP-EQUIV data into document's header data as well as url
  nsAutoString header;
  element->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, header);
  if (!header.IsEmpty()) {
    // Ignore META REFRESH when document is sandboxed from automatic features.
    nsContentUtils::ASCIIToLower(header);
    if (nsGkAtoms::refresh->Equals(header) &&
        (mDocument->GetSandboxFlags() & SANDBOXED_AUTOMATIC_FEATURES)) {
      return NS_OK;
    }

    // Don't allow setting cookies in <meta http-equiv> in cookie averse
    // documents.
    if (nsGkAtoms::setcookie->Equals(header) && mDocument->IsCookieAverse() &&
        StaticPrefs::dom_metaElement_setCookie_allowed()) {
      return NS_OK;
    }

    nsAutoString result;
    element->GetAttr(kNameSpaceID_None, nsGkAtoms::content, result);
    if (!result.IsEmpty()) {
      RefPtr<nsAtom> fieldAtom(NS_Atomize(header));
      rv = ProcessHeaderData(fieldAtom, result, element);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                           nsGkAtoms::handheldFriendly, eIgnoreCase)) {
    nsAutoString result;
    element->GetAttr(kNameSpaceID_None, nsGkAtoms::content, result);
    if (!result.IsEmpty()) {
      nsContentUtils::ASCIIToLower(result);
      mDocument->SetHeaderData(nsGkAtoms::handheldFriendly, result);
    }
  }

  return rv;
}

void nsContentSink::PrefetchPreloadHref(const nsAString& aHref,
                                        nsINode* aSource, uint32_t aLinkTypes,
                                        const nsAString& aAs,
                                        const nsAString& aType,
                                        const nsAString& aMedia) {
  nsCOMPtr<nsIPrefetchService> prefetchService(components::Prefetch::Service());
  if (prefetchService) {
    // construct URI using document charset
    auto encoding = mDocument->GetDocumentCharacterSet();
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref, encoding, mDocument->GetDocBaseURI());
    if (uri) {
      if (aLinkTypes & nsStyleLinkElement::ePRELOAD) {
        nsAttrValue asAttr;
        Link::ParseAsValue(aAs, asAttr);
        nsContentPolicyType policyType = Link::AsValueToContentPolicy(asAttr);

        if (policyType == nsIContentPolicy::TYPE_INVALID) {
          // Ignore preload with a wrong or empty as attribute.
          return;
        }

        nsAutoString mimeType;
        nsAutoString notUsed;
        nsContentUtils::SplitMimeType(aType, mimeType, notUsed);
        if (!HTMLLinkElement::CheckPreloadAttrs(asAttr, mimeType, aMedia,
                                                mDocument)) {
          policyType = nsIContentPolicy::TYPE_INVALID;
        }

        prefetchService->PreloadURI(uri, mDocumentURI, aSource, policyType);
      } else {
        prefetchService->PrefetchURI(
            uri, mDocumentURI, aSource,
            aLinkTypes & nsStyleLinkElement::ePREFETCH);
      }
    }
  }
}

void nsContentSink::PrefetchDNS(const nsAString& aHref) {
  nsAutoString hostname;
  bool isHttps = false;

  if (StringBeginsWith(aHref, NS_LITERAL_STRING("//"))) {
    hostname = Substring(aHref, 2);
  } else {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref);
    if (!uri) {
      return;
    }
    nsresult rv;
    bool isLocalResource = false;
    rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                             &isLocalResource);
    if (NS_SUCCEEDED(rv) && !isLocalResource) {
      nsAutoCString host;
      uri->GetHost(host);
      CopyUTF8toUTF16(host, hostname);
    }
    uri->SchemeIs("https", &isHttps);
  }

  if (!hostname.IsEmpty() && nsHTMLDNSPrefetch::IsAllowed(mDocument)) {
    nsHTMLDNSPrefetch::PrefetchLow(
        hostname, isHttps, mDocument->NodePrincipal()->OriginAttributesRef());
  }
}

void nsContentSink::Preconnect(const nsAString& aHref,
                               const nsAString& aCrossOrigin) {
  // construct URI using document charset
  auto encoding = mDocument->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aHref, encoding, mDocument->GetDocBaseURI());

  if (uri && mDocument) {
    mDocument->MaybePreconnect(uri,
                               dom::Element::StringToCORSMode(aCrossOrigin));
  }
}

nsresult nsContentSink::SelectDocAppCache(
    nsIApplicationCache* aLoadApplicationCache, nsIURI* aManifestURI,
    bool aFetchedWithHTTPGetOrEquiv, CacheSelectionAction* aAction) {
  nsresult rv;

  *aAction = CACHE_SELECTION_NONE;

  if (aLoadApplicationCache) {
    nsCOMPtr<nsIURI> groupURI;
    rv = aLoadApplicationCache->GetManifestURI(getter_AddRefs(groupURI));
    NS_ENSURE_SUCCESS(rv, rv);

    bool equal = false;
    rv = groupURI->Equals(aManifestURI, &equal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!equal) {
      // This is a foreign entry, force a reload to avoid loading the foreign
      // entry. The entry will be marked as foreign to avoid loading it again.

      *aAction = CACHE_SELECTION_RELOAD;
    } else {
      // The http manifest attribute URI is equal to the manifest URI of
      // the cache the document was loaded from - associate the document with
      // that cache and invoke the cache update process.
#ifdef DEBUG
      nsAutoCString docURISpec, clientID;
      mDocumentURI->GetAsciiSpec(docURISpec);
      aLoadApplicationCache->GetClientID(clientID);
      SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
                 SINK_TRACE_CALLS,
                 ("Selection: assigning app cache %s to document %s",
                  clientID.get(), docURISpec.get()));
#endif

      rv = mDocument->SetApplicationCache(aLoadApplicationCache);
      NS_ENSURE_SUCCESS(rv, rv);

      // Document will be added as implicit entry to the cache as part of
      // the update process.
      *aAction = CACHE_SELECTION_UPDATE;
    }
  } else {
    // The document was not loaded from an application cache
    // Here we know the manifest has the same origin as the
    // document. There is call to CheckMayLoad() on it above.

    if (!aFetchedWithHTTPGetOrEquiv) {
      // The document was not loaded using HTTP GET or equivalent
      // method. The spec says to run the cache selection algorithm w/o
      // the manifest specified.
      *aAction = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
    } else {
      // Always do an update in this case
      *aAction = CACHE_SELECTION_UPDATE;
    }
  }

  return NS_OK;
}

nsresult nsContentSink::SelectDocAppCacheNoManifest(
    nsIApplicationCache* aLoadApplicationCache, nsIURI** aManifestURI,
    CacheSelectionAction* aAction) {
  *aManifestURI = nullptr;
  *aAction = CACHE_SELECTION_NONE;

  nsresult rv;

  if (aLoadApplicationCache) {
    // The document was loaded from an application cache, use that
    // application cache as the document's application cache.
#ifdef DEBUG
    nsAutoCString docURISpec, clientID;
    mDocumentURI->GetAsciiSpec(docURISpec);
    aLoadApplicationCache->GetClientID(clientID);
    SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
               SINK_TRACE_CALLS,
               ("Selection, no manifest: assigning app cache %s to document %s",
                clientID.get(), docURISpec.get()));
#endif

    rv = mDocument->SetApplicationCache(aLoadApplicationCache);
    NS_ENSURE_SUCCESS(rv, rv);

    // Return the uri and invoke the update process for the selected
    // application cache.
    rv = aLoadApplicationCache->GetManifestURI(aManifestURI);
    NS_ENSURE_SUCCESS(rv, rv);

    *aAction = CACHE_SELECTION_UPDATE;
  }

  return NS_OK;
}

void nsContentSink::ProcessOfflineManifest(nsIContent* aElement) {
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
  aElement->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::manifest,
                                 manifestSpec);
  ProcessOfflineManifest(manifestSpec);
}

void nsContentSink::ProcessOfflineManifest(const nsAString& aManifestSpec) {
  // Don't bother processing offline manifest for documents
  // without a docshell
  if (!mDocShell) {
    return;
  }

  // If this document has been interecepted, let's skip the processing of the
  // manifest.
  if (mDocument->GetController().isSome()) {
    return;
  }

  // If the docshell's in private browsing mode, we don't want to do any
  // manifest processing.
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(mDocShell);
  if (loadContext->UsePrivateBrowsing()) {
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
  } else {
    nsContentUtils::NewURIWithDocumentCharset(
        getter_AddRefs(manifestURI), aManifestSpec, mDocument, mDocumentURI);
    if (!manifestURI) {
      return;
    }

    // Documents must list a manifest from the same origin
    rv = mDocument->NodePrincipal()->CheckMayLoad(manifestURI, true, false);
    if (NS_FAILED(rv)) {
      action = CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST;
    } else {
      // Only continue if the document has permission to use offline APIs or
      // when preferences indicate to permit it automatically.
      if (!nsContentUtils::OfflineAppAllowed(mDocument->NodePrincipal()) &&
          !nsContentUtils::MaybeAllowOfflineAppByDefault(
              mDocument->NodePrincipal()) &&
          !nsContentUtils::OfflineAppAllowed(mDocument->NodePrincipal())) {
        return;
      }

      bool fetchedWithHTTPGetOrEquiv = false;
      nsCOMPtr<nsIHttpChannel> httpChannel(
          do_QueryInterface(mDocument->GetChannel()));
      if (httpChannel) {
        nsAutoCString method;
        rv = httpChannel->GetRequestMethod(method);
        if (NS_SUCCEEDED(rv))
          fetchedWithHTTPGetOrEquiv = method.EqualsLiteral("GET");
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
                                     getter_AddRefs(manifestURI), &action);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  switch (action) {
    case CACHE_SELECTION_NONE:
      break;
    case CACHE_SELECTION_UPDATE: {
      nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
          components::OfflineCacheUpdate::Service();

      if (updateService) {
        updateService->ScheduleOnDocumentStop(
            manifestURI, mDocumentURI, mDocument->NodePrincipal(), mDocument);
      }
      break;
    }
    case CACHE_SELECTION_RELOAD: {
      // This situation occurs only for toplevel documents, see bottom
      // of SelectDocAppCache method.
      // The document has been loaded from a different offline cache group than
      // the manifest it refers to, i.e. this is a foreign entry, mark it as
      // such and force a reload to avoid loading it.  The next attempt will not
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

void nsContentSink::ScrollToRef() {
  RefPtr<Document> document = mDocument;
  document->ScrollToRef();
}

void nsContentSink::StartLayout(bool aIgnorePendingSheets) {
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("nsContentSink::StartLayout", LAYOUT,
                                        mDocumentURI->GetSpecOrDefault());

  if (mLayoutStarted) {
    // Nothing to do here
    return;
  }

  mDeferredLayoutStart = true;

  if (!aIgnorePendingSheets &&
      (WaitForPendingSheets() || mDocument->HasPendingInitialTranslation())) {
    // Bail out; we'll start layout when the sheets and l10n load
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
  RefPtr<PresShell> presShell = mDocument->GetPresShell();
  // Make sure we don't call Initialize() for a shell that has
  // already called it. This can happen when the layout frame for
  // an iframe is constructed *between* the Embed() call for the
  // docshell in the iframe, and the content sink's call to OpenBody().
  // (Bug 153815)
  if (presShell && !presShell->DidInitialize()) {
    nsresult rv = presShell->Initialize();
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  mDocument->SetScrollToRef(mDocument->GetDocumentURI());
}

void nsContentSink::NotifyAppend(nsIContent* aContainer, uint32_t aStartIndex) {
  mInNotification++;

  {
    // Scope so we call EndUpdate before we decrease mInNotification
    //
    // Note that aContainer->OwnerDoc() may not be mDocument.
    MOZ_AUTO_DOC_UPDATE(aContainer->OwnerDoc(), true);
    nsNodeUtils::ContentAppended(
        aContainer, aContainer->GetChildAt_Deprecated(aStartIndex));
    mLastNotificationTime = PR_Now();
  }

  mInNotification--;
}

NS_IMETHODIMP
nsContentSink::Notify(nsITimer* timer) {
  if (mParsing) {
    // We shouldn't interfere with our normal DidProcessAToken logic
    mDroppedTimer = true;
    return NS_OK;
  }

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
  } else {
    FlushTags();

    // Now try and scroll to the reference
    // XXX Should we scroll unconditionally for history loads??
    ScrollToRef();
  }

  mNotificationTimer = nullptr;
  return NS_OK;
}

bool nsContentSink::IsTimeToNotify() {
  if (!sNotifyOnTimer || !mLayoutStarted || !mBackoffCount ||
      mInMonolithicContainer) {
    return false;
  }

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
    return false;
  }

  PRTime now = PR_Now();

  int64_t interval = GetNotificationInterval();
  int64_t diff = now - mLastNotificationTime;

  if (diff > interval) {
    mBackoffCount--;
    return true;
  }

  return false;
}

nsresult nsContentSink::WillInterruptImpl() {
  nsresult result = NS_OK;

  SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
             SINK_TRACE_CALLS, ("nsContentSink::WillInterrupt: this=%p", this));
#ifndef SINK_NO_INCREMENTAL
  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
  } else if (sNotifyOnTimer && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      int64_t now = PR_Now();
      int64_t interval = GetNotificationInterval();
      int64_t diff = now - mLastNotificationTime;

      // If it's already time for us to have a notification
      if (diff > interval || mDroppedTimer) {
        mBackoffCount--;
        SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
                   SINK_TRACE_REFLOW,
                   ("nsContentSink::WillInterrupt: flushing tags since we've "
                    "run out time; backoff count: %d",
                    mBackoffCount));
        result = FlushTags();
        if (mDroppedTimer) {
          ScrollToRef();
          mDroppedTimer = false;
        }
      } else if (!mNotificationTimer) {
        interval -= diff;
        int32_t delay = interval;

        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;

        NS_NewTimerWithCallback(getter_AddRefs(mNotificationTimer), this, delay,
                                nsITimer::TYPE_ONE_SHOT);
        if (mNotificationTimer) {
          SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
                     SINK_TRACE_REFLOW,
                     ("nsContentSink::WillInterrupt: setting up timer with "
                      "delay %d",
                      delay));
        }
      }
    }
  } else {
    SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
               SINK_TRACE_REFLOW,
               ("nsContentSink::WillInterrupt: flushing tags "
                "unconditionally"));
    result = FlushTags();
  }
#endif

  mParsing = false;

  return result;
}

nsresult nsContentSink::WillResumeImpl() {
  SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
             SINK_TRACE_CALLS, ("nsContentSink::WillResume: this=%p", this));

  mParsing = true;

  return NS_OK;
}

nsresult nsContentSink::DidProcessATokenImpl() {
  if (mRunsToCompletion || !mParser) {
    return NS_OK;
  }

  // Get the current user event time
  PresShell* presShell = mDocument->GetPresShell();
  if (!presShell) {
    // If there's no pres shell in the document, return early since
    // we're not laying anything out here.
    return NS_OK;
  }

  // Increase before comparing to gEventProbeRate
  ++mDeflectedCount;

  // Check if there's a pending event
  if (sPendingEventMode != 0 && !mHasPendingEvent &&
      (mDeflectedCount % sEventProbeRate) == 0) {
    nsViewManager* vm = presShell->GetViewManager();
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
      mDeflectedCount < uint32_t(mDynamicLowerValue ? sInteractiveDeflectCount
                                                    : sPerfDeflectCount)) {
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

void nsContentSink::FavorPerformanceHint(bool perfOverStarvation,
                                         uint32_t starvationDelay) {
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell)
    appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
}

void nsContentSink::BeginUpdate(Document* aDocument) {
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

void nsContentSink::EndUpdate(Document* aDocument) {
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Update our notion of how much
  // has been flushed to include any new content if ending
  // this update leaves us not inside a notification.
  if (!--mInNotification) {
    UpdateChildCounts();
  }
}

void nsContentSink::DidBuildModelImpl(bool aTerminated) {
  if (mDocument) {
    MOZ_ASSERT(aTerminated || mDocument->GetReadyStateEnum() ==
                                  Document::READYSTATE_LOADING,
               "Bad readyState");
    mDocument->SetReadyStateInternal(Document::READYSTATE_INTERACTIVE);
  }

  if (mScriptLoader) {
    mScriptLoader->ParsingComplete(aTerminated);
  }

  if (!mDocument->HaveFiredDOMTitleChange()) {
    mDocument->NotifyPossibleTitleChange(false);
  }

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
               SINK_TRACE_REFLOW,
               ("nsContentSink::DidBuildModel: canceling notification "
                "timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = nullptr;
  }
}

void nsContentSink::DropParserAndPerfHint(void) {
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
  RefPtr<nsParserBase> kungFuDeathGrip(mParser.forget());

  if (mDynamicLowerValue) {
    // Reset the performance hint which was set to FALSE
    // when mDynamicLowerValue was set.
    FavorPerformanceHint(true, 0);
  }

  // Call UnblockOnload only if mRunsToComletion is false and if
  // we have already started loading because it's possible that this function
  // is called (i.e. the parser is terminated) before we start loading due to
  // destroying the window inside unload event callbacks for the previous
  // document.
  if (!mRunsToCompletion && mIsBlockingOnload) {
    mDocument->UnblockOnload(true);
    mIsBlockingOnload = false;
  }
}

bool nsContentSink::IsScriptExecutingImpl() {
  return !!mScriptLoader->GetCurrentScript();
}

nsresult nsContentSink::WillParseImpl(void) {
  if (mRunsToCompletion || !mDocument) {
    return NS_OK;
  }

  PresShell* presShell = mDocument->GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  uint32_t currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  if (sEnablePerfMode == 0) {
    nsViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    uint32_t lastEventTime;
    vm->GetLastUserEventTime(lastEventTime);

    bool newDynLower =
        mDocument->IsInBackgroundWindow() ||
        ((currentTime - mBeginLoadTime) > uint32_t(sInitialPerfTime) &&
         (currentTime - lastEventTime) < uint32_t(sInteractiveTime));

    if (mDynamicLowerValue != newDynLower) {
      FavorPerformanceHint(!newDynLower, 0);
      mDynamicLowerValue = newDynLower;
    }
  }

  mDeflectedCount = 0;
  mHasPendingEvent = false;

  mCurrentParseEndTime =
      currentTime +
      (mDynamicLowerValue ? sInteractiveParseTime : sPerfParseTime);

  return NS_OK;
}

void nsContentSink::WillBuildModelImpl() {
  if (!mRunsToCompletion) {
    mDocument->BlockOnload();
    mIsBlockingOnload = true;

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mDocument->ResetScrolledToRefAlready();

  if (mProcessLinkHeaderEvent.get()) {
    mProcessLinkHeaderEvent.Revoke();

    DoProcessLinkHeader();
  }
}

/* static */
void nsContentSink::NotifyDocElementCreated(Document* aDoc) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  auto* win = nsGlobalWindowInner::Cast(aDoc->GetInnerWindow());
  bool fireInitialInsertion = !win || !win->DidFireDocElemInserted();
  if (win) {
    win->SetDidFireDocElemInserted();
  }
  if (fireInitialInsertion) {
    observerService->NotifyObservers(ToSupports(aDoc),
                                     "initial-document-element-inserted",
                                     EmptyString().get());
  }
  observerService->NotifyObservers(
      ToSupports(aDoc), "document-element-inserted", EmptyString().get());

  nsContentUtils::DispatchChromeEvent(
      aDoc, ToSupports(aDoc), NS_LITERAL_STRING("DOMDocElementInserted"),
      CanBubble::eYes, Cancelable::eNo);
}

NS_IMETHODIMP
nsContentSink::GetName(nsACString& aName) {
  aName.AssignLiteral("nsContentSink_timer");
  return NS_OK;
}
