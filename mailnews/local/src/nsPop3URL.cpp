/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsIURL.h"
#include "nsPop3URL.h"
#include "nsPop3Protocol.h"
#include "nsINetService.h"  /* XXX: NS_FALSE */

#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

nsresult NS_NewPopUrl(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (aInstancePtrResult)
	{
		nsPop3URL * popUrl = new nsPop3URL(); 
		if (popUrl)
			return popUrl->QueryInterface(nsIPop3URL::GetIID(), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

nsPop3URL::nsPop3URL()
{
}
 
nsPop3URL::~nsPop3URL()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsPop3URL, nsMsgMailNewsUrl, nsIPop3URL)  
  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIPop3URL specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3URL::SetPop3Sink(nsIPop3Sink* aPop3Sink)
{
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
        m_pop3Sink = dont_QueryInterface(aPop3Sink);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetPop3Sink(nsIPop3Sink** aPop3Sink) const
{
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
	{
        *aPop3Sink = m_pop3Sink;
		NS_IF_ADDREF(*aPop3Sink);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////

// XXX recode to use nsString api's
// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete

nsresult nsPop3URL::ParseUrl(const nsString& aSpec)
{
    // XXX hack!
    char* cSpec = aSpec.ToNewCString();


    NS_LOCK_INSTANCE();

    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_file);
    PR_FREEIF(m_ref);
    PR_FREEIF(m_search);
    m_port = POP3_PORT;

    // Strip out reference and search info
    char* ref = strpbrk(cSpec, "#?");
    if (nsnull != ref) {
        char* search = nsnull;
        if ('#' == *ref) {
            search = PL_strchr(ref + 1, '?');
            if (nsnull != search) {
                *search++ = '\0';
            }

            PRIntn hashLen = PL_strlen(ref + 1);
            if (0 != hashLen) {
                m_ref = (char*) PR_Malloc(hashLen + 1);
                PL_strcpy(m_ref, ref + 1);
            }      
        }
        else {
            search = ref + 1;
        }

        if (nsnull != search) {
            // The rest is the search
            PRIntn searchLen = PL_strlen(search);
            if (0 != searchLen) {
                m_search = (char*) PR_Malloc(searchLen + 1);
                PL_strcpy(m_search, search);
            }      
        }

        // XXX Terminate string at start of reference or search
        *ref = '\0';
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    char* cp = nsnull;
    char* ap = cSpec;
    char ch;
    while (0 != (ch = *ap)) {
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - cSpec >= 2)) {
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

    // absolute spec

    PR_FREEIF(m_spec);
    PRInt32 slen = aSpec.Length();
    m_spec = (char *) PR_Malloc(slen + 1);
    aSpec.ToCString(m_spec, slen+1);

    // get protocol first
    PRInt32 plen = cp - cSpec;
    m_protocol = (char*) PR_Malloc(plen + 1);
    PL_strncpy(m_protocol, cSpec, plen);
    m_protocol[plen] = 0;
    cp++;                               // eat : in protocol

    // skip over one, two or three slashes
    if (*cp == '/') {
        cp++;
        if (*cp == '/') {
            cp++;
            if (*cp == '/') {
                cp++;
            }
        }
    } 
	else 
	{
        delete [] cSpec;

        NS_UNLOCK_INSTANCE();
        return NS_ERROR_ILLEGAL_VALUE;
    }


#if defined(XP_UNIX) || defined (XP_MAC)
    // Always leave the top level slash for absolute file paths under Mac and UNIX.
    // The code above sometimes results in stripping all of slashes
    // off. This only happens when a previously stripped url is asked to be
    // parsed again. Under Win32 this is not a problem since file urls begin
    // with a drive letter not a slash. This problem show's itself when 
    // nested documents such as iframes within iframes are parsed.

    if (PL_strcmp(m_protocol, "file") == 0) {
        if (*cp != '/') {
            cp--;
        }
    }
#endif /* XP_UNIX */

    const char* cp0 = cp;
    if ((PL_strcmp(m_protocol, "resource") == 0) ||
        (PL_strcmp(m_protocol, "file") == 0)) {
        // resource/file url's do not have host names.
        // The remainder of the string is the file name
        PRInt32 flen = PL_strlen(cp);
        m_file = (char*) PR_Malloc(flen + 1);
        PL_strcpy(m_file, cp);
  
#ifdef NS_WIN32
        if (PL_strcmp(m_protocol, "file") == 0) {
            // If the filename starts with a "x|" where is an single
            // character then we assume it's a drive name and change the
            // vertical bar back to a ":"
            if ((flen >= 2) && (m_file[1] == '|')) {
                m_file[1] = ':';
            }
        }
#endif /* NS_WIN32 */
    } else {
        // Host name follows protocol for http style urls
        cp = PL_strpbrk(cp, "/:");
  
        if (nsnull == cp) {
            // There is only a host name
            PRInt32 hlen = PL_strlen(cp0);
            m_host = (char*) PR_Malloc(hlen + 1);
            PL_strcpy(m_host, cp0);
        }
        else {
            PRInt32 hlen = cp - cp0;
            m_host = (char*) PR_Malloc(hlen + 1);
            PL_strncpy(m_host, cp0, hlen);        
            m_host[hlen] = 0;

            if (':' == *cp) {
                // We have a port number
                cp0 = cp+1;
                cp = PL_strchr(cp, '/');
                m_port = strtol(cp0, (char **)nsnull, 10);
            }
        }

        if (nsnull == cp) {
            // There is no file name
            // Set filename to "/"
            m_file = (char*) PR_Malloc(2);
            m_file[0] = '/';
            m_file[1] = 0;
        }
        else {
            // The rest is the file name
            PRInt32 flen = PL_strlen(cp);
            m_file = (char*) PR_Malloc(flen + 1);
            PL_strcpy(m_file, cp);
        }
    }

//printf("protocol='%s' host='%s' file='%s'\n", m_protocol, m_host, m_file);
    delete [] cSpec;

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void nsPop3URL::ReconstructSpec(void)
{
    PR_FREEIF(m_spec);

    char portBuffer[10];
    if (-1 != m_port) {
        PR_snprintf(portBuffer, 10, ":%d", m_port);
    }
    else {
        portBuffer[0] = '\0';
    }

    PRInt32 plen = PL_strlen(m_protocol) + PL_strlen(m_host) +
        PL_strlen(portBuffer) + PL_strlen(m_file) + 4;
    if (m_ref) {
        plen += 1 + PL_strlen(m_ref);
    }
    if (m_search) {
        plen += 1 + PL_strlen(m_search);
    }

    m_spec = (char *) PR_Malloc(plen + 1);
    PR_snprintf(m_spec, plen, "%s://%s%s%s", 
                m_protocol, ((nsnull != m_host) ? m_host : ""), portBuffer,
                m_file);

    if (m_ref) {
        PL_strcat(m_spec, "#");
        PL_strcat(m_spec, m_ref);
    }
    if (m_search) {
        PL_strcat(m_spec, "?");
        PL_strcat(m_spec, m_search);
    }
}
