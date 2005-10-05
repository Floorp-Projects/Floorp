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

int a2w_buffer(const char* inACPString, int inACPChars, unsigned short* outWideString, int inWideChars)
{
    int retval = 0;
    
    /*
    **  Start off by terminating the out argument if appropriate.
    */
    if(NULL != outWideString && 0 != inWideChars)
    {
        *outWideString = L'\0';
    }
    
    /*
    **  Sanity check arguments.
    */
    if(NULL != inACPString && 0 != inACPChars && (0 == inWideChars || NULL != outWideString))
    {
        /*
        **  Attempt the conversion.
        */
        retval = MultiByteToWideChar(
                                     CP_ACP,
                                     0,
                                     inACPString,
                                     inACPChars,
                                     outWideString,
                                     inWideChars
                                     );
    }
    
    return retval;
}


unsigned short* a2w_malloc(const char* inACPString, int inACPChars, int* outWideChars)
{
    LPWSTR retval = NULL;
    
    /*
    **  Initialize any out arguments.
    */
    if(NULL != outWideChars)
    {
        *outWideChars = 0;
    }
    
    /*
    **  Initialize the acp char length if requested.
    **  We do this here to avoid doing it twice in calls to a2w_buffer.
    */
    if(-1 == inACPChars)
    {
        if(NULL != inACPString)
        {
            /*
            **  Plus one so the terminating character is included.
            */
            inACPChars = (int)strlen(inACPString) + 1;
        }
        else
        {
            inACPChars = 0;
        }
    }
    
    /*
    **  Sanity check arguments.
    */
    if(NULL != inACPString && 0 != inACPChars)
    {
        int charsRequired = 0;
        
        /*
        **  Determine the size of buffer required for the conversion.
        */
        charsRequired = a2w_buffer(inACPString, inACPChars, NULL, 0);
        if(0 != charsRequired)
        {
            LPWSTR heapBuffer = NULL;
            
            heapBuffer = (LPWSTR)malloc((size_t)charsRequired * sizeof(WCHAR));
            if(NULL != heapBuffer)
            {
                int wideChars = 0;
                
                /*
                **  Real thing this time.
                */
                wideChars = a2w_buffer(inACPString, inACPChars, heapBuffer, charsRequired);
                if(0 != wideChars)
                {
                    retval = heapBuffer;
                    if(NULL != outWideChars)
                    {
                        *outWideChars = wideChars;
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
