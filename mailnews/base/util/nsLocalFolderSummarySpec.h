/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsLocalFolderSummarySpec_H
#define _nsLocalFolderSummarySpec_H

#include "msgCore.h"
#include "nsFileSpec.h"

// Class to name a summary file for a local mail folder,
// given a full folder file spec. For windows, this just means tacking .msf on the end.
// For Unix, it might mean something like putting a '.' on the front and .msgsummary on the end.
// Note this class expects the invoking code to fully specify the folder path. 
// This class does NOT prepend the local folder directory, or put .sbd on the containing
// directory names.
class NS_MSG_BASE nsLocalFolderSummarySpec : public nsFileSpec
{
public:
			virtual ~nsLocalFolderSummarySpec();
			nsLocalFolderSummarySpec();
			nsLocalFolderSummarySpec(const char *folderPath, PRBool create = PR_FALSE);
			nsLocalFolderSummarySpec(const nsFileSpec& inFolderPath);
			nsLocalFolderSummarySpec(const nsFilePath &inFolderPath, PRBool create = PR_FALSE);
			void SetFolderName(const char *folderPath);

protected:
	void	CreateSummaryFileName();

};

#endif
