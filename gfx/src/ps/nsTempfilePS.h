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


#ifndef nsTempfilePS_h__
#define nsTempfilePS_h__

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"

class nsILocalFile;

class nsTempfilePS
{
public:
    nsTempfilePS();
    ~nsTempfilePS();

    /**
     * Create a temporary file and return an nsILocalFile object for
     * accessing it.
     *
     * @param aFileName  After a successful return, this will be filled in
     *                   with an nsILocalFile object for a newly-created
     *                   temporary file. There are no open file descriptors
     *                   to the file.
     * @return NS_OK on success, or a suitable error value.
     */
    nsresult CreateTempFile(nsILocalFile** aFile);

    /**
     * Like |CreateTempFile(nsILocalFile**)|, but also initialize a stdio
     * file handle for the file.
     *
     * @param aFileName  After a successful return, this will be filled in
     *                   with an nsILocalFile object for a newly-created
     *                   temporary file.
     * @param aHandle    After a successful return, this will be filled in
     *                   with a file handle that may be used to access the
     *                   file, opened according to the specified mode.
     * @param aMode      Stdio mode string for opening the temporary file.
     *                   See fopen(3).
     * @return NS_OK on success, or a suitable error value.
     */
    nsresult CreateTempFile(nsILocalFile** aFile,
            FILE **aHandle, const char *aMode);

private:
    nsCOMPtr<nsIFile> mTempDir;
    PRUint32 mCount;
};

#endif	/* nsTempfilePS_h__ */
