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
}


nsDefaultURIFixup::~nsDefaultURIFixup()
{
  /* destructor code */
}


/* nsIURI createFixupURI (in wstring aURIText, in unsigned long aFixupFlags); */
NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const PRUnichar *aStringURI, PRUint32 aFixupFlags, nsIURI **aURI)
{
    NS_ENSURE_ARG_POINTER(aStringURI);
    NS_ENSURE_ARG_POINTER(aURI);

    nsresult rv;
    *aURI = nsnull;

    // Try and get the prefs service
    if (!mPrefs)
    {
        mPrefs = do_GetService(NS_PREF_CONTRACTID);
    }

    nsAutoString uriString(aStringURI);
    uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    // View-source is a pseudo scheme. We're interested in fixing up the stuff
    // after it. The easiest way to do that is to call this method again with the
    // "view-source:" lopped off and then prepend it again afterwards.

    if (uriString.EqualsIgnoreCase("view-source:", 12))
    {
        nsCOMPtr<nsIURI> uri;
        PRUint32 newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;

        nsAutoString tempString;
        tempString = Substring(uriString, 12, uriString.Length() - 12);
        rv =  CreateFixupURI(tempString.get(), newFixupFlags, getter_AddRefs(uri));
        if (NS_FAILED(rv) || !uri)
            return NS_ERROR_FAILURE;
        nsCAutoString spec;
        uri->GetSpec(spec);
        uriString.Assign(NS_LITERAL_STRING("view-source:") + NS_ConvertUTF8toUCS2(spec));
    }
    else {
        // Check for if it is a file URL
        FileURIFixup(uriString.get(), aURI);
        if(*aURI)
            return NS_OK;

#ifdef XP_PC
        // Not a file URL, so translate '\' to '/' for convenience in the common protocols
        if (uriString.FindChar(':') == -1 ||
            uriString.EqualsIgnoreCase("http:", 5) ||
            uriString.EqualsIgnoreCase("https:", 6) ||
            uriString.EqualsIgnoreCase("ftp:", 4))
        {
            uriString.ReplaceChar(PRUnichar('\\'), PRUnichar('/'));
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

    // Just try to create an URL out of it
    rv = NS_NewURI(aURI, uriString, bUseNonDefaultCharsetForURI ? GetCharsetForUrlBar() : nsnull);
    if (rv == NS_ERROR_UNKNOWN_PROTOCOL)
    {
        return rv;
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
        if (mPrefs)
        {
            NS_ENSURE_SUCCESS(mPrefs->GetBoolPref("keyword.enabled", &fixupKeywords), NS_ERROR_FAILURE);
        }
        if (fixupKeywords)
        {
            KeywordURIFixup(uriString.get(), aURI);
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

    if (aURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        MakeAlternateURI(*aURI);
    }

    return rv;
}


PRBool nsDefaultURIFixup::MakeAlternateURI(nsIURI *aURI)
{
    PRBool makeAlternate = PR_TRUE;
    if (!mPrefs)
    {
        return PR_FALSE;
    }
    mPrefs->GetBoolPref("browser.fixup.alternate.enabled", &makeAlternate);
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
    rv = mPrefs->GetCharPref("browser.fixup.alternate.prefix", getter_Copies(prefPrefix));
    if (NS_SUCCEEDED(rv))
    {
        prefix.Assign(prefPrefix);
    }

    nsCAutoString suffix(".com");
    nsXPIDLCString prefSuffix;
    rv = mPrefs->GetCharPref("browser.fixup.alternate.suffix", getter_Copies(prefSuffix));
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

nsresult nsDefaultURIFixup::FileURIFixup(const PRUnichar* aStringURI, 
                                         nsIURI** aURI)
{
    nsAutoString uriSpecIn(aStringURI);
    nsCAutoString uriSpecOut;

    nsresult rv = ConvertFileToStringURI(uriSpecIn, uriSpecOut);
    if (NS_SUCCEEDED(rv))
    {
        // if this is file url, uriSpecOut is already in FS charset
        if(NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nsnull)))
            return NS_OK;
    } 
    return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(nsString& aIn,
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
    // Check if it starts with / or \ (UNIX)
    const PRUnichar * up = aIn.get();
    if((PRUnichar('/') == *up) || (PRUnichar('\\') == *up))
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

PRBool nsDefaultURIFixup::PossiblyByteExpandedFileName(nsString& aIn)
{
  // XXXXX HACK XXXXX : please don't copy this code.
  // There are cases where aIn contains the locale byte chars padded to short
  // (thus the name "ByteExpanded"); whereas other cases 
  // have proper Unicode code points.
  // This is a temporary fix.  Please refer to 58866, 86948
  const PRUnichar* uniChar = aIn.get();
  for (PRUint32 i = 0; i < aIn.Length(); i++)  {
    if ((uniChar[i] >= 0x0080) && (uniChar[i] <= 0x00FF))  {
      return PR_TRUE;
    }
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

nsresult nsDefaultURIFixup::KeywordURIFixup(const PRUnichar* aStringURI, 
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

    nsAutoString uriString(aStringURI);
    if(uriString.FindChar('.') == -1 && uriString.FindChar(':') == -1)
    {
        PRInt32 qMarkLoc = uriString.FindChar('?');
        PRInt32 spaceLoc = uriString.FindChar(' ');

        PRBool keyword = PR_FALSE;
        if(qMarkLoc == 0)
        keyword = PR_TRUE;
        else if((spaceLoc > 0) && ((qMarkLoc == -1) || (spaceLoc < qMarkLoc)))
        keyword = PR_TRUE;

        if(keyword)
        {
            nsCAutoString keywordSpec("keyword:");
            char *utf8Spec = ToNewUTF8String(uriString);
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

