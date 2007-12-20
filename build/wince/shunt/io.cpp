/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 28-January-2003
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

#include "mozce_internal.h"

extern "C" {
#if 0
}
#endif


MOZCE_SHUNT_API int chmod(const char* inFilename, int inMode)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("chmod called\n");
#endif
    
    int retval = -1;
    
    if(NULL != inFilename)
    {
        unsigned short buffer[MAX_PATH];
        
        int convRes = a2w_buffer(inFilename, -1, buffer, sizeof(buffer) / sizeof(unsigned short));
        if(0 != convRes)
        {
            DWORD attribs = 0;
            
            attribs = GetFileAttributesW(buffer);
            if(0 != attribs)
            {
                if(0 != (_S_IWRITE & inMode))
                {
                    attribs |= FILE_ATTRIBUTE_READONLY;
                }
                else
                {
                    attribs &= ~FILE_ATTRIBUTE_READONLY;
                }
                
                BOOL setRes = SetFileAttributesW(buffer, attribs);
                if(FALSE != setRes)
                {
                    retval = 0;
                }
            }
        }
    }
    
    return retval;
}


MOZCE_SHUNT_API int isatty(int inHandle)
{
    MOZCE_PRECHECK

#ifdef DEBUG
    mozce_printf("-- isatty called\n");
#endif
    
    int retval = 0;
    
    return retval;
}



/*
 * Our static protocols entries.
 */
static struct protoent sProtos[] = {
    { "tcp",    NULL,   IPPROTO_TCP },
    { "udp",    NULL,   IPPROTO_UDP },
    { "ip",     NULL,   IPPROTO_IP },
    { "icmp",   NULL,   IPPROTO_ICMP },
    { "ggp",    NULL,   IPPROTO_GGP },
    { "pup",    NULL,   IPPROTO_PUP },
    { "idp",    NULL,   IPPROTO_IDP },
    { "nd",     NULL,   IPPROTO_ND },
    { "raw",    NULL,   IPPROTO_RAW }
};

#define MAX_PROTOS (sizeof(sProtos) / sizeof(struct protoent))

/*
 * Wingetprotobyname
 *
 * As getprotobyname
 */
MOZCE_SHUNT_API struct protoent* getprotobyname(const char* inName)
{
    struct protoent* retval = NULL;

    if(NULL != inName)
    {
        unsigned uLoop;

        for(uLoop = 0; uLoop < MAX_PROTOS; uLoop++)
        {
            if(0 == _stricmp(inName, sProtos[uLoop].p_name))
            {
                retval = &sProtos[uLoop];
                break;
            }
        }
    }

    return retval;
}

/*
 * Wingetprotobynumber
 *
 * As getprotobynumber
 */
MOZCE_SHUNT_API struct protoent* getprotobynumber(int inNumber)
{
    struct protoent* retval = NULL;
    unsigned uLoop;
    
    for(uLoop = 0; uLoop < MAX_PROTOS; uLoop++)
    {
        if(inNumber == sProtos[uLoop].p_proto)
        {
            retval = &sProtos[uLoop];
            break;
        }
    }

    return retval;

}
#if 0
{
#endif
} /* extern "C" */

