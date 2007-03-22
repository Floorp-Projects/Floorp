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

int w2a_buffer(const unsigned short* inWideString, int inWideChars, char* outACPString, int inACPChars)
{
    int retval = 0;

    /*
    **  Start off by terminating the out argument if appropriate.
    */
    if(NULL != outACPString && 0 != inACPChars)
    {
        *outACPString = '\0';
    }

    /*
    **  Sanity check arguments.
    */
    if(NULL != inWideString && 0 != inWideChars && (0 == inACPChars || NULL != outACPString))
    {
        /*
        **  Attempt the conversion.
        */
        retval = WideCharToMultiByte(
                                     CP_ACP,
                                     0,
                                     inWideString,
                                     inWideChars,
                                     outACPString,
                                     inACPChars,
                                     NULL,
                                     NULL
                                     );
    }

    return retval;
}


char* w2a_malloc(const unsigned short* inWideString, int inWideChars, int* outACPChars)
{
    LPSTR retval = NULL;

    /*
    **  Initialize any out arguments.
    */
    if(NULL != outACPChars)
    {
        *outACPChars = 0;
    }

    /*
    **  Initialize the wide char length if requested.
    **  We do this here to avoid doing it twice in calls to w2a_buffer.
    */
    if(-1 == inWideChars)
    {
        if(NULL != inWideString)
        {
            /*
            **  Plus one so the terminating character is included.
            */
            inWideChars = (int)wcslen(inWideString) + 1;
        }
        else
        {
            inWideChars = 0;
        }
    }

    /*
    **  Sanity check arguments.
    */
    if(NULL != inWideString && 0 != inWideChars)
    {
        int charsRequired = 0;

        /*
        **  Determine the size of buffer required for the conversion.
        */
        charsRequired = w2a_buffer(inWideString, inWideChars, NULL, 0);
        if(0 != charsRequired)
        {
            LPSTR heapBuffer = NULL;

            heapBuffer = (LPSTR)malloc((size_t)charsRequired * sizeof(CHAR));
            if(NULL != heapBuffer)
            {
                int acpChars = 0;

                /*
                **  Real thing this time.
                */
                acpChars = w2a_buffer(inWideString, inWideChars, heapBuffer, charsRequired);
                if(0 != acpChars)
                {
                    retval = heapBuffer;
                    if(NULL != outACPChars)
                    {
                        *outACPChars = acpChars;
                    }
                }
                else
                {
                    /*
                    **  Something wrong.
                    **  Clean up.
                    */
                    free(heapBuffer);
                }
            }
        }
    }

    return retval;
}
