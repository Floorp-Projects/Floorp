/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Cheshire <cheshire@netscape.com>
 *    Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "DNSUtils.h"

#if( defined( __GNUC__ ) )
  #define   debugf( ARGS... ) ((void)0)
#elif( defined( __MWERKS__ ) )
  #define   debugf( ... )
#else
  #define debugf 1 ? ((void)0) : (void)
#endif

char *ConvertDomainLabelToCStringWithEscape(const domainlabel *const label, char *ptr, char esc)
{
    const mDNSu8 *      src = label->c;                         // Domain label we're reading
    const mDNSu8        len = *src++;                           // Read length of this (non-null) label
    const mDNSu8 *const end = src + len;                            // Work out where the label ends
    if (len > MAX_DOMAIN_LABEL) return(mDNSNULL);                   // If illegal label, abort
    while (src < end)                                           // While we have characters in the label
    {
        mDNSu8 c = *src++;
        if (esc)
        {
            if (c == '.' || c == esc)                           // If character is a dot,
                *ptr++ = esc;                                   // Output escape character
            else if (c <= ' ')                                  // If non-printing ascii,
            {                                                   // Output decimal escape sequence
                *ptr++ = esc;
                *ptr++ = (char)  ('0' + (c / 100)     );
                *ptr++ = (char)  ('0' + (c /  10) % 10);
                c      = (mDNSu8)('0' + (c      ) % 10);
            }
        }
        *ptr++ = (char)c;                                       // Copy the character
    }
    *ptr = 0;                                                   // Null-terminate the string
    return(ptr);                                                // and return
}


char *ConvertDomainNameToCStringWithEscape(const domainname *const name, char *ptr, char esc)
{
    const mDNSu8 *src         = name->c;                                // Domain name we're reading
    const mDNSu8 *const max   = name->c + MAX_DOMAIN_NAME;          // Maximum that's valid
    
    if (*src == 0) *ptr++ = '.';                                    // Special case: For root, just write a dot

    while (*src)                                                    // While more characters in the domain name
    {
        if (src + 1 + *src >= max) return(mDNSNULL);
        ptr = ConvertDomainLabelToCStringWithEscape((const domainlabel *)src, ptr, esc);
        if (!ptr) return(mDNSNULL);
        src += 1 + *src;
        *ptr++ = '.';                                               // Write the dot after the label
    }

    *ptr++ = 0;                                                     // Null-terminate the string
    return(ptr);                                                    // and return
}


const mDNSu8 *DNS_SkipDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end)
{
    mDNSu16 total = 0;

    if (ptr < (mDNSu8*)msg || ptr >= end)
        { debugf("skipDomainName: Illegal ptr not within packet boundaries"); return(mDNSNULL); }

    while (1)                       // Read sequence of labels
    {
        const mDNSu8 len = *ptr++;  // Read length of this label
        if (len == 0) return(ptr);  // If length is zero, that means this name is complete
        switch (len & 0xC0)
        {
            case 0x00:  if (ptr + len >= end)                   // Remember: expect at least one more byte for the root label
                            { debugf("skipDomainName: Malformed domain name (overruns packet end)"); return(mDNSNULL); }
                        if (total + 1 + len >= MAX_DOMAIN_NAME) // Remember: expect at least one more byte for the root label
                            { debugf("skipDomainName: Malformed domain name (more than 255 characters)"); return(mDNSNULL); }
                        ptr += len;
                        total += 1 + len;
                        break;

            case 0x40:  debugf("skipDomainName: Extended EDNS0 label types 0x%X not supported", len); return(mDNSNULL);
            case 0x80:  debugf("skipDomainName: Illegal label length 0x%X", len); return(mDNSNULL);
            case 0xC0:  return(ptr+1);
        }
    }
}

const mDNSu8 *DNS_SkipQuestion(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end)
{
    ptr = DNS_SkipDomainName(msg, ptr, end);
    if (!ptr) { debugf("skipQuestion: Malformed domain name in DNS question section"); return(mDNSNULL); }
    if (ptr+4 > end) { debugf("skipQuestion: Malformed DNS question section -- no query type and class!"); return(mDNSNULL); }
    return(ptr+4);
}

// Routine to fetch an FQDN from the DNS message, following compression pointers if necessary.
const mDNSu8 *DNS_GetDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end, domainname *const name)
{
    const mDNSu8 *nextbyte = mDNSNULL;                  // Record where we got to before we started following pointers
    mDNSu8       *np = name->c;                         // Name pointer
    const mDNSu8 *const limit = np + MAX_DOMAIN_NAME;   // Limit so we don't overrun buffer

    if (ptr < (mDNSu8*)msg || ptr >= end)
        { debugf("getDomainName: Illegal ptr not within packet boundaries"); return(mDNSNULL); }

    *np = 0;                        // Tentatively place the root label here (may be overwritten if we have more labels)

    while (1)                       // Read sequence of labels
    {
        const mDNSu8 len = *ptr++;  // Read length of this label
        if (len == 0) break;        // If length is zero, that means this name is complete
        switch (len & 0xC0)
        {
            int i;
            mDNSu16 offset;

            case 0x00:  if (ptr + len >= end)       // Remember: expect at least one more byte for the root label
                            { debugf("getDomainName: Malformed domain name (overruns packet end)"); return(mDNSNULL); }
                        if (np + 1 + len >= limit)  // Remember: expect at least one more byte for the root label
                            { debugf("getDomainName: Malformed domain name (more than 255 characters)"); return(mDNSNULL); }
                        *np++ = len;
                        for (i=0; i<len; i++) *np++ = *ptr++;
                        *np = 0;    // Tentatively place the root label here (may be overwritten if we have more labels)
                        break;

            case 0x40:  debugf("getDomainName: Extended EDNS0 label types 0x%X not supported in name %##s", len, name->c);
                        return(mDNSNULL);

            case 0x80:  debugf("getDomainName: Illegal label length 0x%X in domain name %##s", len, name->c); return(mDNSNULL);

            case 0xC0:  offset = (mDNSu16)((((mDNSu16)(len & 0x3F)) << 8) | *ptr++);
                        if (!nextbyte) nextbyte = ptr;  // Record where we got to before we started following pointers
                        ptr = (mDNSu8 *)msg + offset;
                        if (ptr < (mDNSu8*)msg || ptr >= end)
                            { debugf("getDomainName: Illegal compression pointer not within packet boundaries"); return(mDNSNULL); }
                        if (*ptr & 0xC0)
                            { debugf("getDomainName: Compression pointer must point to real label"); return(mDNSNULL); }
                        break;
        }
    }
    
    if (nextbyte)
        return(nextbyte);
    else
        return(ptr);
}
