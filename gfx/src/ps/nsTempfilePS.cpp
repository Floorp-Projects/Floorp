/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron@newsguy.com>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Kenneth Herron <kherron@newsguy.com>
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


#include "nsTempfilePS.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsPrintfCString.h"
#include "prtime.h"


nsTempfilePS::nsTempfilePS()
{
    nsresult rv;
    
    // Get the standard temporary directory.
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempDir));
    if (NS_FAILED(rv))
        return;

    // Grab some low-order bits from the current time for use in temporary
    // filenames.
    LL_L2UI(mCount, PR_Now());

    // Append an arbitrary subdirectory name to the temp dir...
    rv = mTempDir->Append(
            NS_ConvertASCIItoUCS2(nsPrintfCString("%lx.tmp", mCount++)));
    if (NS_FAILED(rv)) {
        mTempDir = nsnull;
        return;
    }
    // and try to create it as a private directory for our temp files. Note
    // CreateUnique() may adjust the subdirectory name we just appended.
    rv = mTempDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv))
        mTempDir = nsnull;
}

nsTempfilePS::~nsTempfilePS()
{
    if (nsnull != mTempDir)
        mTempDir->Remove(PR_TRUE);
}

nsresult
nsTempfilePS::CreateTempFile(nsILocalFile** aFile)
{
    NS_PRECONDITION(nsnull != aFile, "aFile argument is NULL");
    NS_ENSURE_TRUE(nsnull != mTempDir, NS_ERROR_NOT_INITIALIZED);

    // Get the temporary directory path
    nsresult rv;
    nsAutoString tmpdir;
    rv = mTempDir->GetPath(tmpdir);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create a new object for the temporary file
    nsCOMPtr<nsILocalFile> tmpfile;
    rv = NS_NewLocalFile(tmpdir, PR_FALSE, getter_AddRefs(tmpfile));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_POSTCONDITION(nsnull != tmpfile,
            "NS_NewLocalFile succeeded but tmpfile is invalid");

    rv = tmpfile->Append(
            NS_ConvertASCIItoUCS2(nsPrintfCString("%lx.tmp", mCount++)));
    NS_ENSURE_SUCCESS(rv, rv);

    // Create the temporary file. This may adjust the file's basename.
    rv = tmpfile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);

    *aFile = tmpfile.get();
    NS_ADDREF(*aFile);
    return NS_OK;
}


nsresult
nsTempfilePS::CreateTempFile(nsILocalFile** aFile,
        FILE **aHandle, const char *aMode)
{
    NS_PRECONDITION(nsnull != aHandle, "aHandle is invalid");
    NS_PRECONDITION(nsnull != aMode, "aMode is invalid");

    nsresult rv = CreateTempFile(aFile);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_POSTCONDITION(nsnull != *aFile,
            "CreateTempFile() succeeded but *aFile is invalid");

    rv = (*aFile)->OpenANSIFileDesc(aMode, aHandle);
    if (NS_FAILED(rv)) {
        (*aFile)->Remove(PR_FALSE);
        NS_RELEASE(*aFile);
    }
    return rv;
}
