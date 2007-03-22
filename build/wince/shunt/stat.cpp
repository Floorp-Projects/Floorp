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
#include "mozce_defs.h"
#include "time_conversions.h"

extern "C" {
#if 0
}
#endif


MOZCE_SHUNT_API int mozce_stat(const char* inPath, struct mozce_stat* outStats)
{
    MOZCE_PRECHECK
        
#ifdef DEBUG
        mozce_printf("mozce_stat called\n");
#endif
    
    int retval = -1;
    
    if(NULL != outStats)
    {
        memset(outStats, 0, sizeof(struct stat));
        
        if(NULL != inPath)
        {
            WCHAR wPath[MAX_PATH];
            
            int convRes = a2w_buffer(inPath, -1, wPath, sizeof(wPath) / sizeof(WCHAR));
            if(0 != convRes)
            {
                HANDLE readHandle;
                WIN32_FIND_DATA findData;
                readHandle = FindFirstFileW(wPath, &findData); 
                
                if (readHandle != INVALID_HANDLE_VALUE && readHandle != NULL)
                {
                    
                    retval = 0;
                    outStats->st_size = findData.nFileSizeLow;
                    
                    if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        outStats->st_mode = _S_IFDIR;
                    }
                    else
                    {
                        outStats->st_mode = _S_IFREG;
                    }
                    
                    
                    FILETIME_2_time_t(outStats->st_ctime, findData.ftCreationTime);
                    FILETIME_2_time_t(outStats->st_atime, findData.ftLastAccessTime);
                    FILETIME_2_time_t(outStats->st_mtime, findData.ftLastWriteTime);
                    
                    outStats->st_mode |= _S_IREAD;
                    if(0 == (FILE_ATTRIBUTE_READONLY & findData.dwFileAttributes))
                    {
                        outStats->st_mode |= _S_IWRITE;
                    }
                    if(FILE_ATTRIBUTE_DIRECTORY & findData.dwFileAttributes)
                    {
                        outStats->st_mode |= _S_IFDIR;
                    }
                    else
                    {
                        outStats->st_mode |= _S_IFREG;
                    }
                    
                }
                else
                {	
                    // From Errno.
                    extern int mozce_errno;
                    mozce_errno = ENOENT;
                }
            }
        }
    }
    
    return retval;
}


#if 0
{
#endif
} /* extern "C" */

