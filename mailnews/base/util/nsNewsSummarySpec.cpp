/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNewsSummarySpec.h"
#include "plstr.h"
#include "nsString.h"

nsNewsSummarySpec::nsNewsSummarySpec()
{
}

nsNewsSummarySpec::nsNewsSummarySpec(const char *folderPath)
  : nsFileSpec(folderPath)
{
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFileSpec& inFolderPath)
: nsFileSpec(inFolderPath)
{
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFilePath &inFolderPath) : nsFileSpec(inFolderPath)
{
	CreateSummaryFileName();
}

void nsNewsSummarySpec::SetFolderName(const char *folderPath)
{
	*this = folderPath;
}

void nsNewsSummarySpec::	CreateSummaryFileName()
{
	char *leafName = GetLeafName();

	nsString fullLeafName(leafName);

	// Append .nsf (news summary file) 

	fullLeafName += ".nsf";				// news summary file
	char *cLeafName = fullLeafName.ToNewCString();
	SetLeafName(cLeafName);
	delete [] cLeafName;	// ###use nsCString when it's available!@
	PL_strfree(leafName);
}

