/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h" // for pre-compiled headers...
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgDBCID.h"

// include files for components this factory creates...
#include "nsMailDatabase.h"
#include "nsNewsDatabase.h"
#include "nsImapMailDatabase.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgRetentionSettings)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDownloadSettings)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDBService)

// The list of components we register
static const nsModuleComponentInfo msgDB_components[] = {
    { "Mail DB", NS_MAILDB_CID, nsnull, nsMailDatabaseConstructor },
    { "News DB", NS_NEWSDB_CID, nsnull, nsNewsDatabaseConstructor },
    { "Imap DB", NS_IMAPDB_CID, nsnull, nsImapMailDatabaseConstructor },
    { "Msg Retention Settings", NS_MSG_RETENTIONSETTINGS_CID,
      NS_MSG_RETENTIONSETTINGS_CONTRACTID, nsMsgRetentionSettingsConstructor },
    { "Msg Download Settings", NS_MSG_DOWNLOADSETTINGS_CID,
      NS_MSG_DOWNLOADSETTINGS_CONTRACTID, nsMsgDownloadSettingsConstructor },
   { "Msg DB Service", NS_MSGDB_SERVICE_CID, NS_MSGDB_SERVICE_CONTRACTID, nsMsgDBServiceConstructor }
};

PR_STATIC_CALLBACK(void)
msgDBModuleDtor(nsIModule* self)
{
    nsMsgDatabase::CleanupCache();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsMsgDBModule, msgDB_components, msgDBModuleDtor)



