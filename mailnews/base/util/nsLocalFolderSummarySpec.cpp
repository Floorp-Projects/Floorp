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

#include "nsLocalFolderSummarySpec.h"
#include "plstr.h"
#include "nsString.h"

MOZ_DECL_CTOR_COUNTER(nsLocalFolderSummarySpec)

nsLocalFolderSummarySpec::~nsLocalFolderSummarySpec()
{
	MOZ_COUNT_DTOR(nsLocalFolderSummarySpec);

}

nsLocalFolderSummarySpec::nsLocalFolderSummarySpec()
{
	MOZ_COUNT_CTOR(nsLocalFolderSummarySpec);
}

nsLocalFolderSummarySpec::nsLocalFolderSummarySpec(const char *folderPath, PRBool create)
  : nsFileSpec(folderPath, create)
{
	MOZ_COUNT_CTOR(nsLocalFolderSummarySpec);
	CreateSummaryFileName();
}

nsLocalFolderSummarySpec::nsLocalFolderSummarySpec(const nsFileSpec& inFolderPath)
: nsFileSpec(inFolderPath)
{
	MOZ_COUNT_CTOR(nsLocalFolderSummarySpec);
	CreateSummaryFileName();
}

nsLocalFolderSummarySpec::nsLocalFolderSummarySpec(const nsFilePath &inFolderPath, PRBool create) : nsFileSpec(inFolderPath, create)
{
	MOZ_COUNT_CTOR(nsLocalFolderSummarySpec);
	CreateSummaryFileName();
}

void nsLocalFolderSummarySpec::SetFolderName(const char *folderPath)
{
	*this = folderPath;
}

void nsLocalFolderSummarySpec::	CreateSummaryFileName()
{
	char *leafName = GetLeafName();

    // STRING USE WARNING: perhaps |fullLeafName| should just be an |nsCString|

	nsString fullLeafName; fullLeafName.AssignWithConversion(leafName);

	// Append .msf (msg summary file) this is what windows will want.
	// Mac and Unix can decide for themselves.

	fullLeafName.AppendWithConversion(".msf");				// message summary file
	char *cLeafName = fullLeafName.ToNewCString();
	SetLeafName(cLeafName);
    nsMemory::Free(cLeafName);
	PL_strfree(leafName);
}

