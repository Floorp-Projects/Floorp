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

#include "nsNewsSummarySpec.h"
#include "plstr.h"
#include "nsString.h"

MOZ_DECL_CTOR_COUNTER(nsNewsSummarySpec)

nsNewsSummarySpec::~nsNewsSummarySpec()
{
	MOZ_COUNT_DTOR(nsNewsSummarySpec);
}

nsNewsSummarySpec::nsNewsSummarySpec()
{
	MOZ_COUNT_CTOR(nsNewsSummarySpec);
}

nsNewsSummarySpec::nsNewsSummarySpec(const char *folderPath)
  : nsFileSpec(folderPath)
{
	MOZ_COUNT_CTOR(nsNewsSummarySpec);
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFileSpec& inFolderPath)
: nsFileSpec(inFolderPath)
{
	MOZ_COUNT_CTOR(nsNewsSummarySpec);
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFilePath &inFolderPath) : nsFileSpec(inFolderPath)
{
	MOZ_COUNT_CTOR(nsNewsSummarySpec);
	CreateSummaryFileName();
}

void nsNewsSummarySpec::SetFolderName(const char *folderPath)
{
	*this = folderPath;
}

void nsNewsSummarySpec::CreateSummaryFileName()
{
	char *leafName = GetLeafName();

    nsCAutoString fullLeafName((const char*)leafName);

	// Append .msf (message summary file) 
	fullLeafName += ".msf";	

	SetLeafName(fullLeafName.GetBuffer());
	PL_strfree(leafName);
}
