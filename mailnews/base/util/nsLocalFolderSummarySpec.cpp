/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLocalFolderSummarySpec.h"
#include "plstr.h"
#include "nsString.h"
#include "nsReadableUtils.h"

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

	fullLeafName.AppendLiteral(".msf");				// message summary file
	char *cLeafName = ToNewCString(fullLeafName);
	SetLeafName(cLeafName);
    nsMemory::Free(cLeafName);
	PL_strfree(leafName);
}

