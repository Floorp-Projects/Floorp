/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsCRT.h"

#include "nsIPlatformCharset.h"
#include "nsIFile.h"
#include <algorithm>

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#include "nsIURIFixup.h"
#include "nsDefaultURIFixup.h"
#include "mozilla/Preferences.h"
#include "nsIObserverService.h"

using namespace mozilla;

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsDefaultURIFixup, nsIURIFixup)

nsDefaultURIFixup::nsDefaultURIFixup()
{
  /* member initializers and constructor code */
}


nsDefaultURIFixup::~nsDefaultURIFixup()
{
  /* destructor code */
}

/* nsIURI createExposableURI (in nsIRUI aURI); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateExposableURI(nsIURI *aURI, nsIURI **aReturn)
{
    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aReturn);

    bool isWyciwyg = false;
    aURI->SchemeIs("wyciwyg", &isWyciwyg);

    nsAutoCString userPass;
    aURI->GetUserPass(userPass);

    // most of the time we can just AddRef and return
    if (!isWyciwyg && userPass.IsEmpty())
    {
        *aReturn = aURI;
        NS_ADDREF(*aReturn);
        return NS_OK;
    }

    // Rats, we have to massage the URI
    nsCOMPtr<nsIURI> uri;
    if (isWyciwyg)
    {
        nsAutoCString path;
        nsresult rv = aURI->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        uint32_t pathLength = path.Length();
        if (pathLength <= 2)
        {
            return NS_ERROR_FAILURE;
        }

        // Path is of the form "//123/http://foo/bar", with a variable number of digits.
        // To figure out where the "real" URL starts, search path for a '/', starting at 
        // the third character.
        int32_t slashIndex = path.FindChar('/', 2);
        if (slashIndex == kNotFound)
        {
            return NS_ERROR_FAILURE;
        }

        // Get the charset of the original URI so we can pass it to our fixed up URI.
        nsAutoCString charset;
        aURI->GetOriginCharset(charset);

        rv = NS_NewURI(getter_AddRefs(uri),
                   Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                   charset.get());
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
        // clone the URI so zapping user:pass doesn't change the original
        nsresult rv = aURI->Clone(getter_AddRefs(uri));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // hide user:pass unless overridden by pref
    if (Preferences::GetBool("browser.fixup.hide_user_pass", true))
    {
        uri->SetUserPass(EmptyCString());
    }

    // return the fixed-up URI
    *aReturn = uri;
    NS_ADDREF(*aReturn);
    return NS_OK;
}

/* nsIURI createFixupURI (in nsAUTF8String aURIText, in unsigned long aFixupFlags); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const nsACString& aStringURI, uint32_t aFixupFlags, nsIURI **aURI)
{
    NS_ENSURE_ARG(!aStringURI.IsEmpty());
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;
    *aURI = nullptr;

    nsAutoCString uriString(aStringURI);
    uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString scheme;
    ioService->ExtractScheme(aStringURI, scheme);
    
    // View-source is a pseudo scheme. We're interested in fixing up the stuff
    // after it. The easiest way to do that is to call this method again with the
    // "view-source:" lopped off and then prepend it again afterwards.

    if (scheme.LowerCaseEqualsLiteral("view-source"))
    {
        nsCOMPtr<nsIURI> uri;
        uint32_t newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;

        rv =  CreateFixupURI(Substring(uriString,
                                       sizeof("view-source:") - 1,
                                       uriString.Length() -
                                         (sizeof("view-source:") - 1)),
                             newFixupFlags, getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
        nsAutoCString spec;
        uri->GetSpec(spec);
        uriString.Assign(NS_LITERAL_CSTRING("view-source:") + spec);
    }
    else {
        // Check for if it is a file URL
        FileURIFixup(uriString, aURI);
        if(*aURI)
            return NS_OK;

#if defined(XP_WIN) || defined(XP_OS2)
        // Not a file URL, so translate '\' to '/' for convenience in the common protocols
        // e.g. catch
        //
        //   http:\\broken.com\address
        //   http:\\broken.com/blah
        //   broken.com\blah
        //
        // Code will also do partial fix up the following urls
        //
        //   http:\\broken.com\address/somewhere\image.jpg (stops at first forward slash)
        //   http:\\broken.com\blah?arg=somearg\foo.jpg (stops at question mark)
        //   http:\\broken.com#odd\ref (stops at hash)
        //  
        if (scheme.IsEmpty() ||
            scheme.LowerCaseEqualsLiteral("http") ||
            scheme.LowerCaseEqualsLiteral("https") ||
            scheme.LowerCaseEqualsLiteral("ftp"))
        {
            // Walk the string replacing backslashes with forward slashes until
            // the end is reached, or a question mark, or a hash, or a forward
            // slash. The forward slash test is to stop before trampling over
            // URIs which legitimately contain a mix of both forward and
            // backward slashes.
            nsAutoCString::iterator start;
            nsAutoCString::iterator end;
            uriString.BeginWriting(start);
            uriString.EndWriting(end);
            while (start != end) {
                if (*start == '?' || *start == '#' || *start == '/')
                    break;
                if (*start == '\\')
                    *start = '/';
                ++start;
            }
        }
#endif
    }

    // For these protocols, use system charset instead of the default UTF-8,
    // if the URI is non ASCII.
    bool bAsciiURI = IsASCII(uriString);
    bool useUTF8 = (aFixupFlags & FIXUP_FLAG_USE_UTF8) ||
                   Preferences::GetBool("browser.fixup.use-utf8", false);
    bool bUseNonDefaultCharsetForURI =
                        !bAsciiURI && !useUTF8 &&
                        (scheme.IsEmpty() ||
                         scheme.LowerCaseEqualsLiteral("http") ||
                         scheme.LowerCaseEqualsLiteral("https") ||
                         scheme.LowerCaseEqualsLiteral("ftp") ||
                         scheme.LowerCaseEqualsLiteral("file"));

    // Now we need to check whether "scheme" is something we don't
    // really know about.
    nsCOMPtr<nsIProtocolHandler> ourHandler, extHandler;
    
    ioService->GetProtocolHandler(scheme.get(), getter_AddRefs(ourHandler));
    extHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default");
    
    if (ourHandler != extHandler || !PossiblyHostPortUrl(uriString)) {
        // Just try to create an URL out of it
        rv = NS_NewURI(aURI, uriString,
                       bUseNonDefaultCharsetForURI ? GetCharsetForUrlBar() : nullptr);

        if (!*aURI && rv != NS_ERROR_MALFORMED_URI) {
            return rv;
        }
    }
    
    if (*aURI) {
        if (aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI)
            MakeAlternateURI(*aURI);
        return NS_OK;
    }

    // See if it is a keyword
    // Test whether keywords need to be fixed up
    bool fixupKeywords = false;
    if (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP) {
        nsresult rv = Preferences::GetBool("keyword.enabled", &fixupKeywords);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
        if (fixupKeywords)
        {
            KeywordURIFixup(uriString, aURI);
            if(*aURI)
                return NS_OK;
        }
    }

    // Prune duff protocol schemes
    //
    //   ://totallybroken.url.com
    //   //shorthand.url.com
    //
    if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("://")))
    {
        uriString = StringTail(uriString, uriString.Length() - 3);
    }
    else if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("//")))
    {
        uriString = StringTail(uriString, uriString.Length() - 2);
    }

    // Add ftp:// or http:// to front of url if it has no spec
    //
    // Should fix:
    //
    //   no-scheme.com
    //   ftp.no-scheme.com
    //   ftp4.no-scheme.com
    //   no-scheme.com/query?foo=http://www.foo.com
    //
    int32_t schemeDelim = uriString.Find("://",0);
    int32_t firstDelim = uriString.FindCharInSet("/:");
    if (schemeDelim <= 0 ||
        (firstDelim != -1 && schemeDelim > firstDelim)) {
        // find host name
        int32_t hostPos = uriString.FindCharInSet("/:?#");
        if (hostPos == -1) 
            hostPos = uriString.Length();

        // extract host name
        nsAutoCString hostSpec;
        uriString.Left(hostSpec, hostPos);

        // insert url spec corresponding to host name
        if (IsLikelyFTP(hostSpec))
            uriString.Assign(NS_LITERAL_CSTRING("ftp://") + uriString);
        else 
            uriString.Assign(NS_LITERAL_CSTRING("http://") + uriString);

        // For ftp & http, we want to use system charset.
        if (!bAsciiURI && !useUTF8)
          bUseNonDefaultCharsetForURI = true;
    } // end if checkprotocol

    rv = NS_NewURI(aURI, uriString, bUseNonDefaultCharsetForURI ? GetCharsetForUrlBar() : nullptr);

    // Did the caller want us to try an alternative URI?
    // If so, attempt to fixup http://foo into http://www.foo.com

    if (*aURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        MakeAlternateURI(*aURI);
    }

    // If we still haven't been able to construct a valid URI, try to force a
    // keyword match.  This catches search strings with '.' or ':' in them.
    if (!*aURI && fixupKeywords)
    {
        KeywordToURI(aStringURI, aURI);
        if(*aURI)
            return NS_OK;
    }

    return rv;
}

NS_IMETHODIMP nsDefaultURIFixup::KeywordToURI(const nsACString& aKeyword,
                                              nsIURI **aURI)
{
    *aURI = nullptr;
    NS_ENSURE_STATE(Preferences::GetRootBranch());

    // Strip leading "?" and leading/trailing spaces from aKeyword
    nsAutoCString keyword(aKeyword);
    if (StringBeginsWith(keyword, NS_LITERAL_CSTRING("?"))) {
        keyword.Cut(0, 1);
    }
    keyword.Trim(" ");

    nsAdoptingCString url = Preferences::GetLocalizedCString("keyword.URL");
    if (!url) {
        // Fall back to a non-localized pref, for backwards compat
        url = Preferences::GetCString("keyword.URL");
    }

    // If the pref is set and non-empty, use it.
    if (!url.IsEmpty()) {
        // Escape keyword, then prepend URL
        nsAutoCString spec;
        if (!NS_Escape(keyword, spec, url_XPAlphas)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        spec.Insert(url, 0);

        nsresult rv = NS_NewURI(aURI, spec);
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
        if (obsSvc) {
            obsSvc->NotifyObservers(*aURI,
                                    "defaultURIFixup-using-keyword-pref",
                                    nullptr);
        }
        return NS_OK;
    }

#ifdef MOZ_TOOLKIT_SEARCH
    // Try falling back to the search service's default search engine
    nsCOMPtr<nsIBrowserSearchService> searchSvc = do_GetService("@mozilla.org/browser/search-service;1");
    if (searchSvc) {
        nsCOMPtr<nsISearchEngine> defaultEngine;
        searchSvc->GetOriginalDefaultEngine(getter_AddRefs(defaultEngine));
        if (defaultEngine) {
            nsCOMPtr<nsISearchSubmission> submission;
            // We want to allow default search plugins to specify alternate
            // parameters that are specific to keyword searches. For the moment,
            // do this by first looking for a magic
            // "application/x-moz-keywordsearch" submission type. In the future,
            // we should instead use a solution that relies on bug 587780.
            defaultEngine->GetSubmission(NS_ConvertUTF8toUTF16(keyword),
                                         NS_LITERAL_STRING("application/x-moz-keywordsearch"),
                                         getter_AddRefs(submission));
            // If getting the special x-moz-keywordsearch submission type failed,
            // fall back to the default response type.
            if (!submission) {
                defaultEngine->GetSubmission(NS_ConvertUTF8toUTF16(keyword),
                                             EmptyString(),
                                             getter_AddRefs(submission));
            }

            if (submission) {
                // The submission depends on POST data (i.e. the search engine's
                // "method" is POST), we can't use this engine for keyword
                // searches
                nsCOMPtr<nsIInputStream> postData;
                submission->GetPostData(getter_AddRefs(postData));
                if (postData) {
                    return NS_ERROR_NOT_AVAILABLE;
                }

                return submission->GetUri(aURI);
            }
        }
    }
#endif

    // out of options
    return NS_ERROR_NOT_AVAILABLE;
}

bool nsDefaultURIFixup::MakeAlternateURI(nsIURI *aURI)
{
    if (!Preferences::GetRootBranch())
    {
        return false;
    }
    if (!Preferences::GetBool("browser.fixup.alternate.enabled", true))
    {
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
        if (*iter == '.')
            numDots++;
        ++iter;
    }


    // Get the prefix and suffix to stick onto the new hostname. By default these
    // are www. & .com but they could be any other value, e.g. www. & .org

    nsAutoCString prefix("www.");
    nsAdoptingCString prefPrefix =
        Preferences::GetCString("browser.fixup.alternate.prefix");
    if (prefPrefix)
    {
        prefix.Assign(prefPrefix);
    }

    nsAutoCString suffix(".com");
    nsAdoptingCString prefSuffix =
        Preferences::GetCString("browser.fixup.alternate.suffix");
    if (prefSuffix)
    {
        suffix.Assign(prefSuffix);
    }
    
    if (numDots == 0)
    {
        newHost.Assign(prefix);
        newHost.Append(oldHost);
        newHost.Append(suffix);
    }
    else if (numDots == 1)
    {
        if (!prefix.IsEmpty() &&
                oldHost.EqualsIgnoreCase(prefix.get(), prefix.Length())) {
            newHost.Assign(oldHost);
            newHost.Append(suffix);
        }
        else if (!suffix.IsEmpty()) {
            newHost.Assign(prefix);
            newHost.Append(oldHost);
        }
        else
        {
            // Do nothing
            return false;
        }
    }
    else
    {
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

/**
 * Check if the host name starts with ftp\d*\. and it's not directly followed
 * by the tld.
 */
bool nsDefaultURIFixup::IsLikelyFTP(const nsCString &aHostSpec)
{
    bool likelyFTP = false;
    if (aHostSpec.EqualsIgnoreCase("ftp", 3)) {
        nsACString::const_iterator iter;
        nsACString::const_iterator end;
        aHostSpec.BeginReading(iter);
        aHostSpec.EndReading(end);
        iter.advance(3); // move past the "ftp" part

        while (iter != end)
        {
            if (*iter == '.') {
                // now make sure the name has at least one more dot in it
                ++iter;
                while (iter != end)
                {
                    if (*iter == '.') {
                        likelyFTP = true;
                        break;
                    }
                    ++iter;
                }
                break;
            }
            else if (!nsCRT::IsAsciiDigit(*iter)) {
                break;
            }
            ++iter;
        }
    }
    return likelyFTP;
}

nsresult nsDefaultURIFixup::FileURIFixup(const nsACString& aStringURI, 
                                         nsIURI** aURI)
{
    nsAutoCString uriSpecOut;

    nsresult rv = ConvertFileToStringURI(aStringURI, uriSpecOut);
    if (NS_SUCCEEDED(rv))
    {
        // if this is file url, uriSpecOut is already in FS charset
        if(NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nullptr)))
            return NS_OK;
    } 
    return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(const nsACString& aIn,
                                                   nsCString& aOut)
{
    bool attemptFixup = false;

#if defined(XP_WIN) || defined(XP_OS2)
    // Check for \ in the url-string or just a drive (PC)
    if(kNotFound != aIn.FindChar('\\') ||
       (aIn.Length() == 2 && (aIn.Last() == ':' || aIn.Last() == '|')))
    {
        attemptFixup = true;
    }
#elif defined(XP_UNIX)
    // Check if it starts with / (UNIX)
    if(aIn.First() == '/')
    {
        attemptFixup = true;
    }
#else
    // Do nothing (All others for now) 
#endif

    if (attemptFixup)
    {
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
        // in non ascii.(see bug 87127) Since it is too risky to make interface change right
        // now, we decide not to do so now.
        // Therefore, the aIn we receive here maybe already in damage form
        // (e.g. treat every bytes as ISO-8859-1 and cast up to PRUnichar
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
          rv = NS_NewNativeLocalFile(NS_LossyConvertUTF16toASCII(in), false, getter_AddRefs(filePath));
        }
        else {
          // input is unicode
          rv = NS_NewLocalFile(in, false, getter_AddRefs(filePath));
        }

        if (NS_SUCCEEDED(rv))
        {
            NS_GetURLSpecFromFile(filePath, aOut);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

bool nsDefaultURIFixup::PossiblyHostPortUrl(const nsACString &aUrl)
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

    while (iter != iterEnd)
    {
        uint32_t chunkSize = 0;
        // Parse a chunk of the address
        while (iter != iterEnd &&
               (*iter == '-' ||
                nsCRT::IsAsciiAlpha(*iter) ||
                nsCRT::IsAsciiDigit(*iter)))
        {
            ++chunkSize;
            ++iter;
        }
        if (chunkSize == 0 || iter == iterEnd)
        {
            return false;
        }
        if (*iter == ':')
        {
            // Go onto checking the for the digits
            break;
        }
        if (*iter != '.')
        {
            // Whatever it is, it ain't a hostname!
            return false;
        }
        ++iter;
    }
    if (iter == iterEnd)
    {
        // No point continuing since there is no colon
        return false;
    }
    ++iter;

    // Count the number of digits after the colon and before the
    // next forward slash (or end of string)

    uint32_t digitCount = 0;
    while (iter != iterEnd && digitCount <= 5)
    {
        if (nsCRT::IsAsciiDigit(*iter))
        {
            digitCount++;
        }
        else if (*iter == '/')
        {
            break;
        }
        else
        {
            // Whatever it is, it ain't a port!
            return false;
        }
        ++iter;
    }
    if (digitCount == 0 || digitCount > 5)
    {
        // No digits or more digits than a port would have.
        return false;
    }

    // Yes, it's possibly a host:port url
    return true;
}

bool nsDefaultURIFixup::PossiblyByteExpandedFileName(const nsAString& aIn)
{
    // XXXXX HACK XXXXX : please don't copy this code.
    // There are cases where aIn contains the locale byte chars padded to short
    // (thus the name "ByteExpanded"); whereas other cases 
    // have proper Unicode code points.
    // This is a temporary fix.  Please refer to 58866, 86948

    nsReadingIterator<PRUnichar> iter;
    nsReadingIterator<PRUnichar> iterEnd;
    aIn.BeginReading(iter);
    aIn.EndReading(iterEnd);
    while (iter != iterEnd)
    {
        if (*iter >= 0x0080 && *iter <= 0x00FF)
            return true;
        ++iter;
    }
    return false;
}

const char * nsDefaultURIFixup::GetFileSystemCharset()
{
  if (mFsCharset.IsEmpty())
  {
    nsresult rv;
    nsAutoCString charset;
    nsCOMPtr<nsIPlatformCharset> plat(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
      rv = plat->GetCharset(kPlatformCharsetSel_FileName, charset);

    if (charset.IsEmpty())
      mFsCharset.AssignLiteral("ISO-8859-1");
    else
      mFsCharset.Assign(charset);
  }

  return mFsCharset.get();
}

const char * nsDefaultURIFixup::GetCharsetForUrlBar()
{
  const char *charset = GetFileSystemCharset();
  return charset;
}

nsresult nsDefaultURIFixup::KeywordURIFixup(const nsACString & aURIString, 
                                            nsIURI** aURI)
{
    // These are keyword formatted strings
    // "what is mozilla"
    // "what is mozilla?"
    // "docshell site:mozilla.org" - has no dot/colon in the first space-separated substring
    // "?mozilla" - anything that begins with a question mark
    // "?site:mozilla.org docshell"
    // Things that have a quote before the first dot/colon

    // These are not keyword formatted strings
    // "www.blah.com" - first space-separated substring contains a dot, doesn't start with "?"
    // "www.blah.com stuff"
    // "nonQualifiedHost:80" - first space-separated substring contains a colon, doesn't start with "?"
    // "nonQualifiedHost:80 args"
    // "nonQualifiedHost?"
    // "nonQualifiedHost?args"
    // "nonQualifiedHost?some args"

    // Note: uint32_t(kNotFound) is greater than any actual location
    // in practice.  So if we cast all locations to uint32_t, then a <
    // b guarantees that either b is kNotFound and a is found, or both
    // are found and a found before b.
    uint32_t dotLoc   = uint32_t(aURIString.FindChar('.'));
    uint32_t colonLoc = uint32_t(aURIString.FindChar(':'));
    uint32_t spaceLoc = uint32_t(aURIString.FindChar(' '));
    if (spaceLoc == 0) {
        // Treat this as not found
        spaceLoc = uint32_t(kNotFound);
    }
    uint32_t qMarkLoc = uint32_t(aURIString.FindChar('?'));
    uint32_t quoteLoc = std::min(uint32_t(aURIString.FindChar('"')),
                               uint32_t(aURIString.FindChar('\'')));

    if (((spaceLoc < dotLoc || quoteLoc < dotLoc) &&
         (spaceLoc < colonLoc || quoteLoc < colonLoc) &&
         (spaceLoc < qMarkLoc || quoteLoc < qMarkLoc)) ||
        qMarkLoc == 0)
    {
        KeywordToURI(aURIString, aURI);
    }

    if(*aURI)
        return NS_OK;

    return NS_ERROR_FAILURE;
}


nsresult NS_NewURIFixup(nsIURIFixup **aURIFixup)
{
    nsDefaultURIFixup *fixup = new nsDefaultURIFixup;
    if (fixup == nullptr)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return fixup->QueryInterface(NS_GET_IID(nsIURIFixup), (void **) aURIFixup);
}

