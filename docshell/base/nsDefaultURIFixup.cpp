/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsCRT.h"

#include "nsIFile.h"
#include <algorithm>

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#include "nsIURIFixup.h"
#include "nsDefaultURIFixup.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Tokenizer.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

// Used to check if external protocol schemes are usable
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

using namespace mozilla;

/* Implementation file */
NS_IMPL_ISUPPORTS(nsDefaultURIFixup, nsIURIFixup)

static bool sInitializedPrefCaches = false;
static bool sFixTypos = true;
static bool sDNSFirstForSingleWords = false;
static bool sFixupKeywords = true;

nsDefaultURIFixup::nsDefaultURIFixup()
{
}

nsDefaultURIFixup::~nsDefaultURIFixup()
{
}

NS_IMETHODIMP
nsDefaultURIFixup::CreateExposableURI(nsIURI* aURI, nsIURI** aReturn)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aReturn);

  bool isWyciwyg = false;
  aURI->SchemeIs("wyciwyg", &isWyciwyg);

  nsAutoCString userPass;
  aURI->GetUserPass(userPass);

  // most of the time we can just AddRef and return
  if (!isWyciwyg && userPass.IsEmpty()) {
    *aReturn = aURI;
    NS_ADDREF(*aReturn);
    return NS_OK;
  }

  // Rats, we have to massage the URI
  nsCOMPtr<nsIURI> uri;
  if (isWyciwyg) {
    nsAutoCString path;
    nsresult rv = aURI->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t pathLength = path.Length();
    if (pathLength <= 2) {
      return NS_ERROR_FAILURE;
    }

    // Path is of the form "//123/http://foo/bar", with a variable number of
    // digits. To figure out where the "real" URL starts, search path for a '/',
    // starting at the third character.
    int32_t slashIndex = path.FindChar('/', 2);
    if (slashIndex == kNotFound) {
      return NS_ERROR_FAILURE;
    }

    // Get the charset of the original URI so we can pass it to our fixed up
    // URI.
    nsAutoCString charset;
    aURI->GetOriginCharset(charset);

    rv = NS_NewURI(getter_AddRefs(uri),
                   Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                   charset.get());
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // clone the URI so zapping user:pass doesn't change the original
    nsresult rv = aURI->Clone(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // hide user:pass unless overridden by pref
  if (Preferences::GetBool("browser.fixup.hide_user_pass", true)) {
    uri->SetUserPass(EmptyCString());
  }

  uri.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const nsACString& aStringURI,
                                  uint32_t aFixupFlags,
                                  nsIInputStream** aPostData, nsIURI** aURI)
{
  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  nsresult rv = GetFixupURIInfo(aStringURI, aFixupFlags, aPostData,
                                getter_AddRefs(fixupInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  fixupInfo->GetPreferredURI(aURI);
  return rv;
}

// Returns true if the URL contains a user:password@ or user@
static bool
HasUserPassword(const nsACString& aStringURI)
{
  mozilla::Tokenizer parser(aStringURI);
  mozilla::Tokenizer::Token token;

  // May start with any of "protocol:", "protocol://",  "//", "://"
  if (parser.Check(Tokenizer::TOKEN_WORD, token)) { // Skip protocol if any
  }
  if (parser.CheckChar(':')) { // Skip colon if found
  }
  while (parser.CheckChar('/')) { // Skip all of the following slashes
  }

  while (parser.Next(token)) {
    if (token.Type() == Tokenizer::TOKEN_CHAR) {
      if (token.AsChar() == '/') {
        return false;
      }
      if (token.AsChar() == '@') {
        return true;
      }
    }
  }

  return false;
}

NS_IMETHODIMP
nsDefaultURIFixup::GetFixupURIInfo(const nsACString& aStringURI,
                                   uint32_t aFixupFlags,
                                   nsIInputStream** aPostData,
                                   nsIURIFixupInfo** aInfo)
{
  NS_ENSURE_ARG(!aStringURI.IsEmpty());

  nsresult rv;

  nsAutoCString uriString(aStringURI);

  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripChars("\r\n");
  // Cleanup the empty spaces that might be on each end:
  uriString.Trim(" ");

  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

  RefPtr<nsDefaultURIFixupInfo> info = new nsDefaultURIFixupInfo(uriString);
  NS_ADDREF(*aInfo = info);

  nsCOMPtr<nsIIOService> ioService =
    do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString scheme;
  ioService->ExtractScheme(aStringURI, scheme);

  // View-source is a pseudo scheme. We're interested in fixing up the stuff
  // after it. The easiest way to do that is to call this method again with the
  // "view-source:" lopped off and then prepend it again afterwards.

  if (scheme.LowerCaseEqualsLiteral("view-source")) {
    nsCOMPtr<nsIURIFixupInfo> uriInfo;
    // We disable keyword lookup and alternate URIs so that small typos don't
    // cause us to look at very different domains
    uint32_t newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
                                         & ~FIXUP_FLAGS_MAKE_ALTERNATE_URI;

    rv = GetFixupURIInfo(Substring(uriString,
                                   sizeof("view-source:") - 1,
                                   uriString.Length() -
                                   (sizeof("view-source:") - 1)),
                         newFixupFlags, aPostData, getter_AddRefs(uriInfo));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    nsAutoCString spec;
    nsCOMPtr<nsIURI> uri;
    uriInfo->GetPreferredURI(getter_AddRefs(uri));
    if (!uri) {
      return NS_ERROR_FAILURE;
    }
    uri->GetSpec(spec);
    uriString.AssignLiteral("view-source:");
    uriString.Append(spec);
  } else {
    // Check for if it is a file URL
    nsCOMPtr<nsIURI> uri;
    FileURIFixup(uriString, getter_AddRefs(uri));
    // NB: FileURIFixup only returns a URI if it had to fix the protocol to
    // do so, so passing in file:///foo/bar will not hit this path:
    if (uri) {
      uri.swap(info->mFixedURI);
      info->mPreferredURI = info->mFixedURI;
      info->mFixupChangedProtocol = true;
      return NS_OK;
    }
  }

  if (!sInitializedPrefCaches) {
    // Check if we want to fix up common scheme typos.
    rv = Preferences::AddBoolVarCache(&sFixTypos,
                                      "browser.fixup.typo.scheme",
                                      sFixTypos);
    MOZ_ASSERT(NS_SUCCEEDED(rv),
               "Failed to observe \"browser.fixup.typo.scheme\"");

    rv = Preferences::AddBoolVarCache(&sDNSFirstForSingleWords,
                                      "browser.fixup.dns_first_for_single_words",
                                      sDNSFirstForSingleWords);
    MOZ_ASSERT(NS_SUCCEEDED(rv),
               "Failed to observe \"browser.fixup.dns_first_for_single_words\"");

    rv = Preferences::AddBoolVarCache(&sFixupKeywords, "keyword.enabled",
                                      sFixupKeywords);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to observe \"keyword.enabled\"");
    sInitializedPrefCaches = true;
  }

  // Fix up common scheme typos.
  if (sFixTypos && (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {
    // Fast-path for common cases.
    if (scheme.IsEmpty() ||
        scheme.LowerCaseEqualsLiteral("http") ||
        scheme.LowerCaseEqualsLiteral("https") ||
        scheme.LowerCaseEqualsLiteral("ftp") ||
        scheme.LowerCaseEqualsLiteral("file")) {
      // Do nothing.
    } else if (scheme.LowerCaseEqualsLiteral("ttp")) {
      // ttp -> http.
      uriString.Replace(0, 3, "http");
      scheme.AssignLiteral("http");
      info->mFixupChangedProtocol = true;
    } else if (scheme.LowerCaseEqualsLiteral("ttps")) {
      // ttps -> https.
      uriString.Replace(0, 4, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.LowerCaseEqualsLiteral("tps")) {
      // tps -> https.
      uriString.Replace(0, 3, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.LowerCaseEqualsLiteral("ps")) {
      // ps -> https.
      uriString.Replace(0, 2, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.LowerCaseEqualsLiteral("ile")) {
      // ile -> file.
      uriString.Replace(0, 3, "file");
      scheme.AssignLiteral("file");
      info->mFixupChangedProtocol = true;
    } else if (scheme.LowerCaseEqualsLiteral("le")) {
      // le -> file.
      uriString.Replace(0, 2, "file");
      scheme.AssignLiteral("file");
      info->mFixupChangedProtocol = true;
    }
  }

  // Now we need to check whether "scheme" is something we don't
  // really know about.
  nsCOMPtr<nsIProtocolHandler> ourHandler, extHandler;

  ioService->GetProtocolHandler(scheme.get(), getter_AddRefs(ourHandler));
  extHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "default");

  if (ourHandler != extHandler || !PossiblyHostPortUrl(uriString)) {
    // Just try to create an URL out of it
    rv = NS_NewURI(getter_AddRefs(info->mFixedURI), uriString, nullptr);

    if (!info->mFixedURI && rv != NS_ERROR_MALFORMED_URI) {
      return rv;
    }
  }

  if (info->mFixedURI && ourHandler == extHandler && sFixupKeywords &&
      (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {
    nsCOMPtr<nsIExternalProtocolService> extProtService =
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
    if (extProtService) {
      bool handlerExists = false;
      rv = extProtService->ExternalProtocolHandlerExists(scheme.get(),
                                                         &handlerExists);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // This basically means we're dealing with a theoretically valid
      // URI... but we have no idea how to load it. (e.g. "christmas:humbug")
      // It's more likely the user wants to search, and so we
      // chuck this over to their preferred search provider instead:
      if (!handlerExists) {
        bool hasUserPassword = HasUserPassword(uriString);
        if (!hasUserPassword) {
          TryKeywordFixupForURIInfo(uriString, info, aPostData);
        } else {
          // If the given URL has a user:password we can't just pass it to the
          // external protocol handler; we'll try using it with http instead later
          info->mFixedURI = nullptr;
        }
      }
    }
  }

  if (info->mFixedURI) {
    if (!info->mPreferredURI) {
      if (aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        info->mFixupCreatedAlternateURI = MakeAlternateURI(info->mFixedURI);
      }
      info->mPreferredURI = info->mFixedURI;
    }
    return NS_OK;
  }

  // Fix up protocol string before calling KeywordURIFixup, because
  // it cares about the hostname of such URIs:
  nsCOMPtr<nsIURI> uriWithProtocol;
  bool inputHadDuffProtocol = false;

  // Prune duff protocol schemes
  //
  //   ://totallybroken.url.com
  //   //shorthand.url.com
  //
  if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("://"))) {
    uriString = StringTail(uriString, uriString.Length() - 3);
    inputHadDuffProtocol = true;
  } else if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("//"))) {
    uriString = StringTail(uriString, uriString.Length() - 2);
    inputHadDuffProtocol = true;
  }

  // NB: this rv gets returned at the end of this method if we never
  // do a keyword fixup after this (because the pref or the flags passed
  // might not let us).
  rv = FixupURIProtocol(uriString, info, getter_AddRefs(uriWithProtocol));
  if (uriWithProtocol) {
    info->mFixedURI = uriWithProtocol;
  }

  // See if it is a keyword
  // Test whether keywords need to be fixed up
  if (sFixupKeywords && (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP) &&
      !inputHadDuffProtocol) {
    if (NS_SUCCEEDED(KeywordURIFixup(uriString, info, aPostData)) &&
        info->mPreferredURI) {
      return NS_OK;
    }
  }

  // Did the caller want us to try an alternative URI?
  // If so, attempt to fixup http://foo into http://www.foo.com

  if (info->mFixedURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
    info->mFixupCreatedAlternateURI = MakeAlternateURI(info->mFixedURI);
  }

  if (info->mFixedURI) {
    info->mPreferredURI = info->mFixedURI;
    return NS_OK;
  }

  // If we still haven't been able to construct a valid URI, try to force a
  // keyword match.  This catches search strings with '.' or ':' in them.
  if (sFixupKeywords && (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
    rv = TryKeywordFixupForURIInfo(aStringURI, info, aPostData);
  }

  return rv;
}

NS_IMETHODIMP
nsDefaultURIFixup::KeywordToURI(const nsACString& aKeyword,
                                nsIInputStream** aPostData,
                                nsIURIFixupInfo** aInfo)
{
  RefPtr<nsDefaultURIFixupInfo> info = new nsDefaultURIFixupInfo(aKeyword);
  NS_ADDREF(*aInfo = info);

  if (aPostData) {
    *aPostData = nullptr;
  }
  NS_ENSURE_STATE(Preferences::GetRootBranch());

  // Strip leading "?" and leading/trailing spaces from aKeyword
  nsAutoCString keyword(aKeyword);
  if (StringBeginsWith(keyword, NS_LITERAL_CSTRING("?"))) {
    keyword.Cut(0, 1);
  }
  keyword.Trim(" ");

  if (XRE_IsContentProcess()) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (!contentChild) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    ipc::OptionalInputStreamParams postData;
    ipc::OptionalURIParams uri;
    nsAutoString providerName;
    if (!contentChild->SendKeywordToURI(keyword, &providerName, &postData,
                                        &uri)) {
      return NS_ERROR_FAILURE;
    }

    CopyUTF8toUTF16(keyword, info->mKeywordAsSent);
    info->mKeywordProviderName = providerName;

    if (aPostData) {
      nsTArray<ipc::FileDescriptor> fds;
      nsCOMPtr<nsIInputStream> temp = DeserializeInputStream(postData, fds);
      temp.forget(aPostData);

      MOZ_ASSERT(fds.IsEmpty());
    }

    nsCOMPtr<nsIURI> temp = DeserializeURI(uri);
    info->mPreferredURI = temp.forget();
    return NS_OK;
  }

#ifdef MOZ_TOOLKIT_SEARCH
  // Try falling back to the search service's default search engine
  nsCOMPtr<nsIBrowserSearchService> searchSvc =
    do_GetService("@mozilla.org/browser/search-service;1");
  if (searchSvc) {
    nsCOMPtr<nsISearchEngine> defaultEngine;
    searchSvc->GetDefaultEngine(getter_AddRefs(defaultEngine));
    if (defaultEngine) {
      nsCOMPtr<nsISearchSubmission> submission;
      nsAutoString responseType;
      // We allow default search plugins to specify alternate
      // parameters that are specific to keyword searches.
      NS_NAMED_LITERAL_STRING(mozKeywordSearch,
                              "application/x-moz-keywordsearch");
      bool supportsResponseType = false;
      defaultEngine->SupportsResponseType(mozKeywordSearch,
                                          &supportsResponseType);
      if (supportsResponseType) {
        responseType.Assign(mozKeywordSearch);
      }

      NS_ConvertUTF8toUTF16 keywordW(keyword);
      defaultEngine->GetSubmission(keywordW,
                                   responseType,
                                   NS_LITERAL_STRING("keyword"),
                                   getter_AddRefs(submission));

      if (submission) {
        nsCOMPtr<nsIInputStream> postData;
        submission->GetPostData(getter_AddRefs(postData));
        if (aPostData) {
          postData.forget(aPostData);
        } else if (postData) {
          // The submission specifies POST data (i.e. the search
          // engine's "method" is POST), but our caller didn't allow
          // passing post data back. No point passing back a URL that
          // won't load properly.
          return NS_ERROR_FAILURE;
        }

        defaultEngine->GetName(info->mKeywordProviderName);
        info->mKeywordAsSent = keywordW;
        return submission->GetUri(getter_AddRefs(info->mPreferredURI));
      }
    }
  }
#endif

  // out of options
  return NS_ERROR_NOT_AVAILABLE;
}

// Helper to deal with passing around uri fixup stuff
nsresult
nsDefaultURIFixup::TryKeywordFixupForURIInfo(const nsACString& aURIString,
                                             nsDefaultURIFixupInfo* aFixupInfo,
                                             nsIInputStream** aPostData)
{
  nsCOMPtr<nsIURIFixupInfo> keywordInfo;
  nsresult rv = KeywordToURI(aURIString, aPostData,
                             getter_AddRefs(keywordInfo));
  if (NS_SUCCEEDED(rv)) {
    keywordInfo->GetKeywordProviderName(aFixupInfo->mKeywordProviderName);
    keywordInfo->GetKeywordAsSent(aFixupInfo->mKeywordAsSent);
    keywordInfo->GetPreferredURI(getter_AddRefs(aFixupInfo->mPreferredURI));
  }
  return rv;
}

bool
nsDefaultURIFixup::MakeAlternateURI(nsIURI* aURI)
{
  if (!Preferences::GetRootBranch()) {
    return false;
  }
  if (!Preferences::GetBool("browser.fixup.alternate.enabled", true)) {
    return false;
  }

  // Code only works for http. Not for any other protocol including https!
  bool isHttp = false;
  aURI->SchemeIs("http", &isHttp);
  if (!isHttp) {
    return false;
  }

  // Security - URLs with user / password info should NOT be fixed up
  nsAutoCString userpass;
  aURI->GetUserPass(userpass);
  if (!userpass.IsEmpty()) {
    return false;
  }

  nsAutoCString oldHost;
  nsAutoCString newHost;
  aURI->GetHost(oldHost);

  // Count the dots
  int32_t numDots = 0;
  nsReadingIterator<char> iter;
  nsReadingIterator<char> iterEnd;
  oldHost.BeginReading(iter);
  oldHost.EndReading(iterEnd);
  while (iter != iterEnd) {
    if (*iter == '.') {
      numDots++;
    }
    ++iter;
  }

  // Get the prefix and suffix to stick onto the new hostname. By default these
  // are www. & .com but they could be any other value, e.g. www. & .org

  nsAutoCString prefix("www.");
  nsAdoptingCString prefPrefix =
    Preferences::GetCString("browser.fixup.alternate.prefix");
  if (prefPrefix) {
    prefix.Assign(prefPrefix);
  }

  nsAutoCString suffix(".com");
  nsAdoptingCString prefSuffix =
    Preferences::GetCString("browser.fixup.alternate.suffix");
  if (prefSuffix) {
    suffix.Assign(prefSuffix);
  }

  if (numDots == 0) {
    newHost.Assign(prefix);
    newHost.Append(oldHost);
    newHost.Append(suffix);
  } else if (numDots == 1) {
    if (!prefix.IsEmpty() &&
        oldHost.EqualsIgnoreCase(prefix.get(), prefix.Length())) {
      newHost.Assign(oldHost);
      newHost.Append(suffix);
    } else if (!suffix.IsEmpty()) {
      newHost.Assign(prefix);
      newHost.Append(oldHost);
    } else {
      // Do nothing
      return false;
    }
  } else {
    // Do nothing
    return false;
  }

  if (newHost.IsEmpty()) {
    return false;
  }

  // Assign the new host string over the old one
  aURI->SetHost(newHost);
  return true;
}

nsresult
nsDefaultURIFixup::FileURIFixup(const nsACString& aStringURI, nsIURI** aURI)
{
  nsAutoCString uriSpecOut;

  nsresult rv = ConvertFileToStringURI(aStringURI, uriSpecOut);
  if (NS_SUCCEEDED(rv)) {
    // if this is file url, uriSpecOut is already in FS charset
    if (NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nullptr))) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsDefaultURIFixup::ConvertFileToStringURI(const nsACString& aIn,
                                          nsCString& aResult)
{
  bool attemptFixup = false;

#if defined(XP_WIN)
  // Check for \ in the url-string or just a drive (PC)
  if (aIn.Contains('\\') ||
      (aIn.Length() == 2 && (aIn.Last() == ':' || aIn.Last() == '|'))) {
    attemptFixup = true;
  }
#elif defined(XP_UNIX)
  // Check if it starts with / (UNIX)
  if (aIn.First() == '/') {
    attemptFixup = true;
  }
#else
  // Do nothing (All others for now)
#endif

  if (attemptFixup) {
    // Test if this is a valid path by trying to create a local file
    // object. The URL of that is returned if successful.

    // NOTE: Please be sure to check that the call to NS_NewLocalFile
    //       rejects bad file paths when using this code on a new
    //       platform.

    nsCOMPtr<nsIFile> filePath;
    nsresult rv;

    // this is not the real fix but a temporary fix
    // in order to really fix the problem, we need to change the
    // nsICmdLineService interface to use wstring to pass paramenters
    // instead of string since path name and other argument could be
    // in non ascii.(see bug 87127) Since it is too risky to make interface
    // change right now, we decide not to do so now.
    // Therefore, the aIn we receive here maybe already in damage form
    // (e.g. treat every bytes as ISO-8859-1 and cast up to char16_t
    //  while the real data could be in file system charset )
    // we choice the following logic which will work for most of the case.
    // Case will still failed only if it meet ALL the following condiction:
    //    1. running on CJK, Russian, or Greek system, and
    //    2. user type it from URL bar
    //    3. the file name contains character in the range of
    //       U+00A1-U+00FF but encode as different code point in file
    //       system charset (e.g. ACP on window)- this is very rare case
    // We should remove this logic and convert to File system charset here
    // once we change nsICmdLineService to use wstring and ensure
    // all the Unicode data come in is correctly converted.
    // XXXbz nsICmdLineService doesn't hand back unicode, so in some cases
    // what we have is actually a "utf8" version of a "utf16" string that's
    // actually byte-expanded native-encoding data.  Someone upstream needs
    // to stop using AssignWithConversion and do things correctly.  See bug
    // 58866 for what happens if we remove this
    // PossiblyByteExpandedFileName check.
    NS_ConvertUTF8toUTF16 in(aIn);
    if (PossiblyByteExpandedFileName(in)) {
      // removes high byte
      rv = NS_NewNativeLocalFile(NS_LossyConvertUTF16toASCII(in), false,
                                 getter_AddRefs(filePath));
    } else {
      // input is unicode
      rv = NS_NewLocalFile(in, false, getter_AddRefs(filePath));
    }

    if (NS_SUCCEEDED(rv)) {
      NS_GetURLSpecFromFile(filePath, aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsDefaultURIFixup::FixupURIProtocol(const nsACString& aURIString,
                                    nsDefaultURIFixupInfo* aFixupInfo,
                                    nsIURI** aURI)
{
  nsAutoCString uriString(aURIString);
  *aURI = nullptr;

  // Add ftp:// or http:// to front of url if it has no spec
  //
  // Should fix:
  //
  //   no-scheme.com
  //   ftp.no-scheme.com
  //   ftp4.no-scheme.com
  //   no-scheme.com/query?foo=http://www.foo.com
  //   user:pass@no-scheme.com
  //
  int32_t schemeDelim = uriString.Find("://", 0);
  int32_t firstDelim = uriString.FindCharInSet("/:");
  if (schemeDelim <= 0 ||
      (firstDelim != -1 && schemeDelim > firstDelim)) {
    // find host name
    int32_t hostPos = uriString.FindCharInSet("/:?#");
    if (hostPos == -1) {
      hostPos = uriString.Length();
    }

    // extract host name
    nsAutoCString hostSpec;
    uriString.Left(hostSpec, hostPos);

    // insert url spec corresponding to host name
    uriString.InsertLiteral("http://", 0);
    aFixupInfo->mFixupChangedProtocol = true;
  } // end if checkprotocol

  return NS_NewURI(aURI, uriString, nullptr);
}

bool
nsDefaultURIFixup::PossiblyHostPortUrl(const nsACString& aUrl)
{
  // Oh dear, the protocol is invalid. Test if the protocol might
  // actually be a url without a protocol:
  //
  //   http://www.faqs.org/rfcs/rfc1738.html
  //   http://www.faqs.org/rfcs/rfc2396.html
  //
  // e.g. Anything of the form:
  //
  //   <hostname>:<port> or
  //   <hostname>:<port>/
  //
  // Where <hostname> is a string of alphanumeric characters and dashes
  // separated by dots.
  // and <port> is a 5 or less digits. This actually breaks the rfc2396
  // definition of a scheme which allows dots in schemes.
  //
  // Note:
  //   People expecting this to work with
  //   <user>:<password>@<host>:<port>/<url-path> will be disappointed!
  //
  // Note: Parser could be a lot tighter, tossing out silly hostnames
  //       such as those containing consecutive dots and so on.

  // Read the hostname which should of the form
  // [a-zA-Z0-9\-]+(\.[a-zA-Z0-9\-]+)*:

  nsACString::const_iterator iterBegin;
  nsACString::const_iterator iterEnd;
  aUrl.BeginReading(iterBegin);
  aUrl.EndReading(iterEnd);
  nsACString::const_iterator iter = iterBegin;

  while (iter != iterEnd) {
    uint32_t chunkSize = 0;
    // Parse a chunk of the address
    while (iter != iterEnd &&
           (*iter == '-' ||
            nsCRT::IsAsciiAlpha(*iter) ||
            nsCRT::IsAsciiDigit(*iter))) {
      ++chunkSize;
      ++iter;
    }
    if (chunkSize == 0 || iter == iterEnd) {
      return false;
    }
    if (*iter == ':') {
      // Go onto checking the for the digits
      break;
    }
    if (*iter != '.') {
      // Whatever it is, it ain't a hostname!
      return false;
    }
    ++iter;
  }
  if (iter == iterEnd) {
    // No point continuing since there is no colon
    return false;
  }
  ++iter;

  // Count the number of digits after the colon and before the
  // next forward slash (or end of string)

  uint32_t digitCount = 0;
  while (iter != iterEnd && digitCount <= 5) {
    if (nsCRT::IsAsciiDigit(*iter)) {
      digitCount++;
    } else if (*iter == '/') {
      break;
    } else {
      // Whatever it is, it ain't a port!
      return false;
    }
    ++iter;
  }
  if (digitCount == 0 || digitCount > 5) {
    // No digits or more digits than a port would have.
    return false;
  }

  // Yes, it's possibly a host:port url
  return true;
}

bool
nsDefaultURIFixup::PossiblyByteExpandedFileName(const nsAString& aIn)
{
  // XXXXX HACK XXXXX : please don't copy this code.
  // There are cases where aIn contains the locale byte chars padded to short
  // (thus the name "ByteExpanded"); whereas other cases
  // have proper Unicode code points.
  // This is a temporary fix.  Please refer to 58866, 86948

  nsReadingIterator<char16_t> iter;
  nsReadingIterator<char16_t> iterEnd;
  aIn.BeginReading(iter);
  aIn.EndReading(iterEnd);
  while (iter != iterEnd) {
    if (*iter >= 0x0080 && *iter <= 0x00FF) {
      return true;
    }
    ++iter;
  }
  return false;
}

nsresult
nsDefaultURIFixup::KeywordURIFixup(const nsACString& aURIString,
                                   nsDefaultURIFixupInfo* aFixupInfo,
                                   nsIInputStream** aPostData)
{
  // These are keyword formatted strings
  // "what is mozilla"
  // "what is mozilla?"
  // "docshell site:mozilla.org" - has no dot/colon in the first space-separated substring
  // "?mozilla" - anything that begins with a question mark
  // "?site:mozilla.org docshell"
  // Things that have a quote before the first dot/colon
  // "mozilla" - checked against a whitelist to see if it's a host or not
  // ".mozilla", "mozilla." - ditto

  // These are not keyword formatted strings
  // "www.blah.com" - first space-separated substring contains a dot, doesn't start with "?"
  // "www.blah.com stuff"
  // "nonQualifiedHost:80" - first space-separated substring contains a colon, doesn't start with "?"
  // "nonQualifiedHost:80 args"
  // "nonQualifiedHost?"
  // "nonQualifiedHost?args"
  // "nonQualifiedHost?some args"
  // "blah.com."

  // Note: uint32_t(kNotFound) is greater than any actual location
  // in practice.  So if we cast all locations to uint32_t, then a <
  // b guarantees that either b is kNotFound and a is found, or both
  // are found and a found before b.

  uint32_t firstDotLoc = uint32_t(kNotFound);
  uint32_t lastDotLoc = uint32_t(kNotFound);
  uint32_t firstColonLoc = uint32_t(kNotFound);
  uint32_t firstQuoteLoc = uint32_t(kNotFound);
  uint32_t firstSpaceLoc = uint32_t(kNotFound);
  uint32_t firstQMarkLoc = uint32_t(kNotFound);
  uint32_t lastLSBracketLoc = uint32_t(kNotFound);
  uint32_t lastSlashLoc = uint32_t(kNotFound);
  uint32_t pos = 0;
  uint32_t foundDots = 0;
  uint32_t foundColons = 0;
  uint32_t foundDigits = 0;
  uint32_t foundRSBrackets = 0;
  bool looksLikeIpv6 = true;
  bool hasAsciiAlpha = false;

  nsACString::const_iterator iterBegin;
  nsACString::const_iterator iterEnd;
  aURIString.BeginReading(iterBegin);
  aURIString.EndReading(iterEnd);
  nsACString::const_iterator iter = iterBegin;

  while (iter != iterEnd) {
    if (pos >= 1 && foundRSBrackets == 0) {
      if (!(lastLSBracketLoc == 0 &&
            (*iter == ':' ||
             *iter == '.' ||
             *iter == ']' ||
             (*iter >= 'a' && *iter <= 'f') ||
             (*iter >= 'A' && *iter <= 'F') ||
             nsCRT::IsAsciiDigit(*iter)))) {
        looksLikeIpv6 = false;
      }
    }

    // If we're at the end of the string or this is the first slash,
    // check if the thing before the slash looks like ipv4:
    if ((iter.size_forward() == 1 ||
         (lastSlashLoc == uint32_t(kNotFound) && *iter == '/')) &&
        // Need 2 or 3 dots + only digits
        (foundDots == 2 || foundDots == 3) &&
        // and they should be all that came before now:
        (foundDots + foundDigits == pos ||
         // or maybe there was also exactly 1 colon that came after the last dot,
         // and the digits, dots and colon were all that came before now:
         (foundColons == 1 && firstColonLoc > lastDotLoc &&
          foundDots + foundDigits + foundColons == pos))) {
      // Hurray, we got ourselves some ipv4!
      // At this point, there's no way we will do a keyword lookup, so just bail immediately:
      return NS_OK;
    }

    if (*iter == '.') {
      ++foundDots;
      lastDotLoc = pos;
      if (firstDotLoc == uint32_t(kNotFound)) {
        firstDotLoc = pos;
      }
    } else if (*iter == ':') {
      ++foundColons;
      if (firstColonLoc == uint32_t(kNotFound)) {
        firstColonLoc = pos;
      }
    } else if (*iter == ' ' && firstSpaceLoc == uint32_t(kNotFound)) {
      firstSpaceLoc = pos;
    } else if (*iter == '?' && firstQMarkLoc == uint32_t(kNotFound)) {
      firstQMarkLoc = pos;
    } else if ((*iter == '\'' || *iter == '"') &&
               firstQuoteLoc == uint32_t(kNotFound)) {
      firstQuoteLoc = pos;
    } else if (*iter == '[') {
      lastLSBracketLoc = pos;
    } else if (*iter == ']') {
      foundRSBrackets++;
    } else if (*iter == '/') {
      lastSlashLoc = pos;
    } else if (nsCRT::IsAsciiAlpha(*iter)) {
      hasAsciiAlpha = true;
    } else if (nsCRT::IsAsciiDigit(*iter)) {
      ++foundDigits;
    }

    pos++;
    iter++;
  }

  if (lastLSBracketLoc > 0 || foundRSBrackets != 1) {
    looksLikeIpv6 = false;
  }

  // If there are only colons and only hexadecimal characters ([a-z][0-9])
  // enclosed in [], then don't do a keyword lookup
  if (looksLikeIpv6) {
    return NS_OK;
  }

  nsAutoCString asciiHost;
  nsAutoCString host;

  bool isValidAsciiHost =
    aFixupInfo->mFixedURI &&
    NS_SUCCEEDED(aFixupInfo->mFixedURI->GetAsciiHost(asciiHost)) &&
    !asciiHost.IsEmpty();

  bool isValidHost =
    aFixupInfo->mFixedURI &&
    NS_SUCCEEDED(aFixupInfo->mFixedURI->GetHost(host)) &&
    !host.IsEmpty();

  nsresult rv = NS_OK;
  // We do keyword lookups if a space or quote preceded the dot, colon
  // or question mark (or if the latter is not found, or if the input starts
  // with a question mark)
  if (((firstSpaceLoc < firstDotLoc || firstQuoteLoc < firstDotLoc) &&
       (firstSpaceLoc < firstColonLoc || firstQuoteLoc < firstColonLoc) &&
       (firstSpaceLoc < firstQMarkLoc || firstQuoteLoc < firstQMarkLoc)) ||
      firstQMarkLoc == 0) {
    rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                   aPostData);
    // ... or when the host is the same as asciiHost and there are no
    // characters from [a-z][A-Z]
  } else if (isValidAsciiHost && isValidHost && !hasAsciiAlpha &&
             host.EqualsIgnoreCase(asciiHost.get())) {
    if (!sDNSFirstForSingleWords) {
      rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                     aPostData);
    }
  }
  // ... or if there is no question mark or colon, and there is either no
  // dot, or exactly 1 and it is the first or last character of the input:
  else if ((firstDotLoc == uint32_t(kNotFound) ||
            (foundDots == 1 && (firstDotLoc == 0 ||
                                firstDotLoc == aURIString.Length() - 1))) &&
           firstColonLoc == uint32_t(kNotFound) &&
           firstQMarkLoc == uint32_t(kNotFound)) {
    if (isValidAsciiHost && IsDomainWhitelisted(asciiHost, firstDotLoc)) {
      return NS_OK;
    }

    // ... unless there are no dots, and a slash, and alpha characters, and
    // this is a valid host:
    if (firstDotLoc == uint32_t(kNotFound) &&
        lastSlashLoc != uint32_t(kNotFound) &&
        hasAsciiAlpha && isValidAsciiHost) {
      return NS_OK;
    }

    // If we get here, we don't have a valid URI, or we did but the
    // host is not whitelisted, so we do a keyword search *anyway*:
    rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                   aPostData);
  }
  return rv;
}

bool
nsDefaultURIFixup::IsDomainWhitelisted(const nsACString& aAsciiHost,
                                       const uint32_t aDotLoc)
{
  if (sDNSFirstForSingleWords) {
    return true;
  }
  // Check if this domain is whitelisted as an actual
  // domain (which will prevent a keyword query)
  // NB: any processing of the host here should stay in sync with
  // code in the front-end(s) that set the pref.

  nsAutoCString pref("browser.fixup.domainwhitelist.");

  if (aDotLoc == aAsciiHost.Length() - 1) {
    pref.Append(Substring(aAsciiHost, 0, aAsciiHost.Length() - 1));
  } else {
    pref.Append(aAsciiHost);
  }

  return Preferences::GetBool(pref.get(), false);
}

NS_IMETHODIMP
nsDefaultURIFixup::IsDomainWhitelisted(const nsACString& aDomain,
                                       const uint32_t aDotLoc,
                                       bool* aResult)
{
  *aResult = IsDomainWhitelisted(aDomain, aDotLoc);
  return NS_OK;
}

/* Implementation of nsIURIFixupInfo */
NS_IMPL_ISUPPORTS(nsDefaultURIFixupInfo, nsIURIFixupInfo)

nsDefaultURIFixupInfo::nsDefaultURIFixupInfo(const nsACString& aOriginalInput)
  : mFixupChangedProtocol(false)
  , mFixupCreatedAlternateURI(false)
{
  mOriginalInput = aOriginalInput;
}

nsDefaultURIFixupInfo::~nsDefaultURIFixupInfo()
{
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetConsumer(nsISupports** aConsumer)
{
  *aConsumer = mConsumer;
  NS_IF_ADDREF(*aConsumer);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::SetConsumer(nsISupports* aConsumer)
{
  mConsumer = aConsumer;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetPreferredURI(nsIURI** aPreferredURI)
{
  *aPreferredURI = mPreferredURI;
  NS_IF_ADDREF(*aPreferredURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixedURI(nsIURI** aFixedURI)
{
  *aFixedURI = mFixedURI;
  NS_IF_ADDREF(*aFixedURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetKeywordProviderName(nsAString& aResult)
{
  aResult = mKeywordProviderName;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetKeywordAsSent(nsAString& aResult)
{
  aResult = mKeywordAsSent;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixupChangedProtocol(bool* aResult)
{
  *aResult = mFixupChangedProtocol;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixupCreatedAlternateURI(bool* aResult)
{
  *aResult = mFixupCreatedAlternateURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetOriginalInput(nsACString& aResult)
{
  aResult = mOriginalInput;
  return NS_OK;
}
