/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsCRT.h"

#include "nsIPrefService.h"
#include "nsIPlatformCharset.h"
#include "nsILocalFile.h"

#include "nsIURIFixup.h"
#include "nsDefaultURIFixup.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsDefaultURIFixup, nsIURIFixup)

nsDefaultURIFixup::nsDefaultURIFixup()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  // Try and get the pref service
  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
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

    PRBool isWyciwyg = PR_FALSE;
    aURI->SchemeIs("wyciwyg", &isWyciwyg);

    if (!isWyciwyg)
    {
        *aReturn = aURI;
        NS_ADDREF(*aReturn);
        return NS_OK;
    }

    nsCAutoString path;
    nsresult rv = aURI->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 pathLength = path.Length();
    if (pathLength <= 2)
    {
        return NS_ERROR_FAILURE;
    }

    // Path is of the form "//123/http://foo/bar", with a variable number of digits.
    // To figure out where the "real" URL starts, search path for a '/', starting at 
    // the third character.
    PRInt32 slashIndex = path.FindChar('/', 2);
    if (slashIndex == kNotFound)
    {
        return NS_ERROR_FAILURE;
    }

    // Get the charset of the original URI so we can pass it to our fixed up URI.
    nsCAutoString charset;
    aURI->GetOriginCharset(charset);

    rv = NS_NewURI(aReturn,
                   Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                   charset.get());
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

/* nsIURI createFixupURI (in wstring aURIText, in unsigned long aFixupFlags); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const nsAString& aStringURI, PRUint32 aFixupFlags, nsIURI **aURI)
{
    NS_ENSURE_ARG(!aStringURI.IsEmpty());
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;
    *aURI = nsnull;

    nsAutoString uriString(aStringURI);
    uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

    // View-source is a pseudo scheme. We're interested in fixing up the stuff
    // after it. The easiest way to do that is to call this method again with the
    // "view-source:" lopped off and then prepend it again afterwards.

    if (uriString.EqualsIgnoreCase("view-source:", 12))
    {
        nsCOMPtr<nsIURI> uri;
        PRUint32 newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;

        rv =  CreateFixupURI(Substring(uriString, 12, uriString.Length() - 12),
                             newFixupFlags, getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
        nsCAutoString spec;
        uri->GetSpec(spec);
        uriString.Assign(NS_LITERAL_STRING("view-source:") + NS_ConvertUTF8toUCS2(spec));
    }
    else {
        // Check for if it is a file URL
        FileURIFixup(uriString, aURI);
        if(*aURI)
            return NS_OK;

#ifdef XP_PC
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
        if (uriString.FindChar(':') == -1 ||
            uriString.EqualsIgnoreCase("http:", 5) ||
            uriString.EqualsIgnoreCase("https:", 6) ||
            uriString.EqualsIgnoreCase("ftp:", 4))
        {
            // Walk the string replacing backslashes with forward slashes until
            // the end is reached, or a question mark, or a hash, or a forward
            // slash. The forward slash test is to stop before trampling over
            // URIs which legitimately contain a mix of both forward and
            // backward slashes.
            nsAFlatString::iterator start;
            nsAFlatString::iterator end;
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
    PRBool bAsciiURI = IsASCII(uriString);
    PRBool bUseNonDefaultCharsetForURI =
                        !bAsciiURI &&
                        (uriString.FindChar(':') == kNotFound ||
                        uriString.EqualsIgnoreCase("http:", 5) ||
                        uriString.EqualsIgnoreCase("https:", 6) ||
                        uriString.EqualsIgnoreCase("ftp:", 4) ||
                        uriString.EqualsIgnoreCase("file:", 5));

    // Check the scheme...
    NS_LossyConvertUCS2toASCII asciiURI(uriString);
    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString scheme;
    ioService->ExtractScheme(asciiURI, scheme);
    nsCOMPtr<nsIProtocolHandler> ourHandler, extHandler;
    
    ioService->GetProtocolHandler(scheme.get(), getter_AddRefs(ourHandler));
    extHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default");

    if (ourHandler != extHandler || !PossiblyHostPortUrl(uriString)) {
        // Just try to create an URL out of it
        rv = NS_NewURI(aURI, uriString,
                       bUseNonDefaultCharsetForURI ? GetCharsetForUrlBar() : nsnull);
    }

    if (*aURI) {
        if (aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI)
            MakeAlternateURI(*aURI);
        return NS_OK;
    }

    // See if it is a keyword
    // Test whether keywords need to be fixed up
    if (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP) {
        PRBool fixupKeywords = PR_FALSE;
        if (mPrefBranch)
        {
            NS_ENSURE_SUCCESS(mPrefBranch->GetBoolPref("keyword.enabled", &fixupKeywords), NS_ERROR_FAILURE);
        }
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
    if (uriString.EqualsIgnoreCase("://", 3))
    {
        nsAutoString newUriString;
        uriString.Mid(newUriString, 3, uriString.Length() - 3);
        uriString = newUriString;
    }
    else if (uriString.EqualsIgnoreCase("//", 2))
    {
        nsAutoString newUriString;
        uriString.Mid(newUriString, 2, uriString.Length() - 2);
        uriString = newUriString;
    }

    // Add ftp:// or http:// to front of url if it has no spec
    //
    // Should fix:
    //
    //   no-scheme.com
    //   ftp.no-scheme.com
    //   ftp4.no-scheme,com
    //   no-scheme.com/query?foo=http://www.foo.com
    //
    PRInt32 schemeDelim = uriString.Find("://",0);
    PRInt32 firstDelim = uriString.FindCharInSet("/:");
    if (schemeDelim <= 0 ||
        (firstDelim != -1 && schemeDelim > firstDelim)) {
        // find host name
        PRInt32 hostPos = uriString.FindCharInSet("/:?#");
        if (hostPos == -1) 
            hostPos = uriString.Length();

        // extract host name
        nsAutoString hostSpec;
        uriString.Left(hostSpec, hostPos);

        // insert url spec corresponding to host name
        if (hostSpec.EqualsIgnoreCase("ftp", 3)) 
            uriString.Assign(NS_LITERAL_STRING("ftp://") + uriString);
        else 
            uriString.Assign(NS_LITERAL_STRING("http://") + uriString);

        // For ftp & http, we want to use system charset.
        if (!bAsciiURI)
          bUseNonDefaultCharsetForURI = PR_TRUE;
    } // end if checkprotocol

    rv = NS_NewURI(aURI, uriString, bUseNonDefaultCharsetForURI ? GetCharsetForUrlBar() : nsnull);

    // Did the caller want us to try an alternative URI?
    // If so, attempt to fixup http://foo into http://www.foo.com

    if (*aURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        MakeAlternateURI(*aURI);
    }

    return rv;
}


PRBool nsDefaultURIFixup::MakeAlternateURI(nsIURI *aURI)
{
    if (!mPrefBranch)
    {
        return PR_FALSE;
    }
    PRBool makeAlternate = PR_TRUE;
    mPrefBranch->GetBoolPref("browser.fixup.alternate.enabled", &makeAlternate);
    if (!makeAlternate)
    {
        return PR_FALSE;
    }

    // Code only works for http. Not for any other protocol including https!
    PRBool isHttp = PR_FALSE;
    aURI->SchemeIs("http", &isHttp);
    if (!isHttp) {
        return PR_FALSE;
    }

    // Security - URLs with user / password info should NOT be fixed up
    nsCAutoString userpass;
    aURI->GetUserPass(userpass);
    if (userpass.Length() > 0) {
        return PR_FALSE;
    }

    nsCAutoString oldHost;
    nsCAutoString newHost;
    aURI->GetHost(oldHost);

    // Count the dots
    PRInt32 numDots = 0;
    nsReadingIterator<char> iter;
    nsReadingIterator<char> iterEnd;
    oldHost.BeginReading(iter);
    oldHost.EndReading(iterEnd);
    while (iter != iterEnd) {
        if (*iter == '.')
            numDots++;
        ++iter;
    }


    nsresult rv;

    // Get the prefix and suffix to stick onto the new hostname. By default these
    // are www. & .com but they could be any other value, e.g. www. & .org

    nsCAutoString prefix("www.");
    nsXPIDLCString prefPrefix;
    rv = mPrefBranch->GetCharPref("browser.fixup.alternate.prefix", getter_Copies(prefPrefix));
    if (NS_SUCCEEDED(rv))
    {
        prefix.Assign(prefPrefix);
    }

    nsCAutoString suffix(".com");
    nsXPIDLCString prefSuffix;
    rv = mPrefBranch->GetCharPref("browser.fixup.alternate.suffix", getter_Copies(prefSuffix));
    if (NS_SUCCEEDED(rv))
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
        if (prefix.Length() > 0 &&
                oldHost.EqualsIgnoreCase(prefix.get(), prefix.Length())) {
            newHost.Assign(oldHost);
            newHost.Append(suffix);
        }
        else if (suffix.Length() > 0) {
            newHost.Assign(prefix);
            newHost.Append(oldHost);
        }
        else
        {
            // Do nothing
            return PR_FALSE;
        }
    }
    else
    {
        // Do nothing
        return PR_FALSE;
    }

    if (newHost.IsEmpty()) {
        return PR_FALSE;
    }

    // Assign the new host string over the old one
    aURI->SetHost(newHost);
    return PR_TRUE;
}

nsresult nsDefaultURIFixup::FileURIFixup(const nsAString& aStringURI, 
                                         nsIURI** aURI)
{
    nsCAutoString uriSpecOut;

    nsresult rv = ConvertFileToStringURI(aStringURI, uriSpecOut);
    if (NS_SUCCEEDED(rv))
    {
        // if this is file url, uriSpecOut is already in FS charset
        if(NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nsnull)))
            return NS_OK;
    } 
    return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(const nsAString& aIn,
                                                   nsCString& aOut)
{
    PRBool attemptFixup = PR_FALSE;

#ifdef XP_PC
    // Check for \ in the url-string or just a drive (PC)
    if(kNotFound != aIn.FindChar(PRUnichar('\\')) || ((aIn.Length() == 2 ) && (aIn.Last() == PRUnichar(':') || aIn.Last() == PRUnichar('|'))))
    {
        attemptFixup = PR_TRUE;
    }
#elif XP_UNIX
    // Check if it starts with / (UNIX)
    if(aIn.First() == PRUnichar('/'))
    {
        attemptFixup = PR_TRUE;
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

        nsCOMPtr<nsILocalFile> filePath;
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
        if (PossiblyByteExpandedFileName(aIn)) {
          // removes high byte
          rv = NS_NewNativeLocalFile(NS_LossyConvertUCS2toASCII(aIn), PR_FALSE, getter_AddRefs(filePath));
        }
        else {
          // input is unicode
          rv = NS_NewLocalFile(aIn, PR_FALSE, getter_AddRefs(filePath));
        }

        if (NS_SUCCEEDED(rv))
        {
            NS_GetURLSpecFromFile(filePath, aOut);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

PRBool nsDefaultURIFixup::PossiblyHostPortUrl(const nsAString &aUrl)
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
    // seperated by dots.
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

    nsReadingIterator<PRUnichar> iterBegin;
    nsReadingIterator<PRUnichar> iterEnd;
    aUrl.BeginReading(iterBegin);
    aUrl.EndReading(iterEnd);
    nsReadingIterator<PRUnichar> iter = iterBegin;

    while (iter != iterEnd)
    {
        PRUint32 chunkSize = 0;
        // Parse a chunk of the address
        while (iter != iterEnd &&
               (*iter == PRUnichar('-') ||
                nsCRT::IsAsciiAlpha(*iter) ||
                nsCRT::IsAsciiDigit(*iter)))
        {
            ++chunkSize;
            ++iter;
        }
        if (chunkSize == 0 || iter == iterEnd)
        {
            return PR_FALSE;
        }
        if (*iter == PRUnichar(':'))
        {
            // Go onto checking the for the digits
            break;
        }
        if (*iter != PRUnichar('.'))
        {
            // Whatever it is, it ain't a hostname!
            return PR_FALSE;
        }
        ++iter;
    }
    if (iter == iterEnd)
    {
        // No point continuing since there is no colon
        return PR_FALSE;
    }
    ++iter;

    // Count the number of digits after the colon and before the
    // next forward slash (or end of string)

    PRUint32 digitCount = 0;
    while (iter != iterEnd && digitCount <= 5)
    {
        if (nsCRT::IsAsciiDigit(*iter))
        {
            digitCount++;
        }
        else if (*iter == PRUnichar('/'))
        {
            break;
        }
        else
        {
            // Whatever it is, it ain't a port!
            return PR_FALSE;
        }
        ++iter;
    }
    if (digitCount == 0 || digitCount > 5)
    {
        // No digits or more digits than a port would have.
        return PR_FALSE;
    }

    // Yes, it's possibly a host:port url
    return PR_TRUE;
}

PRBool nsDefaultURIFixup::PossiblyByteExpandedFileName(const nsAString& aIn)
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
            return PR_TRUE;
        ++iter;
    }
    return PR_FALSE;
}

const char * nsDefaultURIFixup::GetFileSystemCharset()
{
  if (mFsCharset.IsEmpty())
  {
    nsresult rv;
    nsAutoString charset;
    nsCOMPtr<nsIPlatformCharset> plat(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
      rv = plat->GetCharset(kPlatformCharsetSel_FileName, charset);

    if (charset.IsEmpty())
      mFsCharset.Assign(NS_LITERAL_CSTRING("ISO-8859-1"));
    else
      mFsCharset.Assign(NS_LossyConvertUCS2toASCII(charset));
  }

  return mFsCharset.get();
}

const char * nsDefaultURIFixup::GetCharsetForUrlBar()
{
  const char *charset = GetFileSystemCharset();
#ifdef XP_MAC
  // check for "x-mac-" prefix
  if ((strlen(charset) >= 6) && charset[0] == 'x' && charset[2] == 'm')
  {
    if (!strcmp("x-mac-roman", charset))
      return "ISO-8859-1";
    // we can do more x-mac-xxxx mapping here
    // or somewhere in intl code like nsIPlatformCharset.
  }
#endif
  return charset;
}

nsresult nsDefaultURIFixup::KeywordURIFixup(const nsAString & aURIString, 
                                            nsIURI** aURI)
{
    // These are keyword formatted strings
    // "what is mozilla"
    // "what is mozilla?"
    // "?mozilla"
    // "?What is mozilla"

    // These are not keyword formatted strings
    // "www.blah.com" - anything with a dot in it 
    // "nonQualifiedHost:80" - anything with a colon in it
    // "nonQualifiedHost?"
    // "nonQualifiedHost?args"
    // "nonQualifiedHost?some args"

    if(aURIString.FindChar('.') == -1 && aURIString.FindChar(':') == -1)
    {
        PRInt32 qMarkLoc = aURIString.FindChar('?');
        PRInt32 spaceLoc = aURIString.FindChar(' ');

        PRBool keyword = PR_FALSE;
        if(qMarkLoc == 0)
        keyword = PR_TRUE;
        else if((spaceLoc > 0) && ((qMarkLoc == -1) || (spaceLoc < qMarkLoc)))
        keyword = PR_TRUE;

        if(keyword)
        {
            nsCAutoString keywordSpec("keyword:");
            char *utf8Spec = ToNewUTF8String(aURIString);
            if(utf8Spec)
            {
                char* escapedUTF8Spec = nsEscape(utf8Spec, url_Path);
                if(escapedUTF8Spec) 
                {
                    keywordSpec.Append(escapedUTF8Spec);
                    NS_NewURI(aURI, keywordSpec.get(), nsnull);
                    nsMemory::Free(escapedUTF8Spec);
                } // escapedUTF8Spec
                nsMemory::Free(utf8Spec);
            } // utf8Spec
        } // keyword 
    } // FindChar

    if(*aURI)
        return NS_OK;

    return NS_ERROR_FAILURE;
}


nsresult NS_NewURIFixup(nsIURIFixup **aURIFixup)
{
    nsDefaultURIFixup *fixup = new nsDefaultURIFixup;
    if (fixup == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return fixup->QueryInterface(NS_GET_IID(nsIURIFixup), (void **) aURIFixup);
}

