/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h" // for pre-compiled headers...
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgDBCID.h"

// include files for components this factory creates...
#include "nsMailDatabase.h"
#include "nsNewsDatabase.h"
#include "nsImapMailDatabase.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);
static NS_DEFINE_CID(kCMsgRetentionSettings, NS_MSG_RETENTIONSETTINGS_CID);
static NS_DEFINE_CID(kCMsgDownloadSettings, NS_MSG_DOWNLOADSETTINGS_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgRetentionSettings)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDownloadSettings)

// The list of components we register
static nsModuleComponentInfo msgDB_components[] = {
    { "Mail DB", NS_MAILDB_CID, nsnull, nsMailDatabaseConstructor },
    { "News DB", NS_NEWSDB_CID, nsnull, nsNewsDatabaseConstructor },
    { "Imap DB", NS_IMAPDB_CID, nsnull, nsImapMailDatabaseConstructor },
    { "Msg Retention Settings", NS_MSG_RETENTIONSETTINGS_CID,
      NS_MSG_RETENTIONSETTINGS_CONTRACTID, nsMsgRetentionSettingsConstructor },
    { "Msg Download Settings", NS_MSG_DOWNLOADSETTINGS_CID,
      NS_MSG_DOWNLOADSETTINGS_CONTRACTID, nsMsgDownloadSettingsConstructor }
};

PR_STATIC_CALLBACK(void)
msgDBModuleDtor(nsIModule* self)
{
    nsMsgDatabase::CleanupCache();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsMsgDBModule, msgDB_components, msgDBModuleDtor)



