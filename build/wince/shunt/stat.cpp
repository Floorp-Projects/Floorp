/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2003 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

#include "mozce_internal.h"
#include "mozce_defs.h"
#include "time_conversions.h"

extern "C" {
#if 0
}
#endif


MOZCE_SHUNT_API int mozce_stat(const char* inPath, struct stat* outStats)
{
#ifdef DEBUG
    printf("mozce_stat called\n");
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
                HANDLE readHandle = NULL;
                
                readHandle = CreateFileW(
                    wPath,
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                if(NULL != readHandle)
                {
                    BOOL bRes = FALSE;
                    BY_HANDLE_FILE_INFORMATION fileInfo;
                    
                    bRes = GetFileInformationByHandle(readHandle, &fileInfo);
                    CloseHandle(readHandle);
                    readHandle = NULL;

                    if(FALSE != bRes)
                    {
                        FILETIME_2_time_t(outStats->st_ctime, fileInfo.ftCreationTime);
                        FILETIME_2_time_t(outStats->st_atime, fileInfo.ftLastAccessTime);
                        FILETIME_2_time_t(outStats->st_mtime, fileInfo.ftLastWriteTime);
                        
                        outStats->st_size = (long)fileInfo.nFileSizeLow;
                        
                        outStats->st_mode |= _S_IREAD;
                        if(0 == (FILE_ATTRIBUTE_READONLY & fileInfo.dwFileAttributes))
                        {
                            outStats->st_mode |= _S_IWRITE;
                        }
                        if(FILE_ATTRIBUTE_DIRECTORY & fileInfo.dwFileAttributes)
                        {
                            outStats->st_mode |= _S_IFDIR;
                        }
                        else
                        {
                            outStats->st_mode |= _S_IFREG;
                        }
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

