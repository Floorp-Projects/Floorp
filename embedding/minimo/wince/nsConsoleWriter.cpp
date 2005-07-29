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
 * The Original Code is the Mozilla Toolkit.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "prio.h"
#include "prprf.h"
#include "prtime.h"
#include "prenv.h"

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsNativeCharsetUtils.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsIConsoleService.h"
#include "nsIConsoleMessage.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

void
WriteConsoleLog()
{
  nsresult rv;

  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_OS_CURRENT_PROCESS_DIR, getter_AddRefs(file));
  
  if (!file)
    return;

  file->Append(NS_LITERAL_STRING("jsconsole.log"));

  nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file);
               
  PRFileDesc *fdesc;
  rv = lfile->OpenNSPRFileDesc(PR_WRONLY | PR_APPEND | PR_CREATE_FILE, 0660, &fdesc);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIConsoleService> csrv
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!csrv) {
    PR_Close(fdesc);
    return;
  }

  nsIConsoleMessage** messages;
  PRUint32 mcount;

  rv = csrv->GetMessageArray(&messages, &mcount);
  if (NS_FAILED(rv)) {
    PR_Close(fdesc);
    return;
  }

  if (mcount) {
    PRExplodedTime etime;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &etime);
    char datetime[512];
    PR_FormatTimeUSEnglish(datetime, sizeof(datetime),
                           "%Y-%m-%d %H:%M:%S", &etime);

    PR_fprintf(fdesc, NS_LINEBREAK
                     "*** Console log: %s ***" NS_LINEBREAK,
               datetime);
  }

  // From this point on, we have to release all the messages, and free
  // the memory allocated for the messages array. XPCOM arrays suck.

  nsXPIDLString msg;
  nsCAutoString nativemsg;

  for (PRUint32 i = 0; i < mcount; ++i) {
    rv = messages[i]->GetMessage(getter_Copies(msg));
    if (NS_SUCCEEDED(rv)) {
      NS_CopyUnicodeToNative(msg, nativemsg);
      PR_fprintf(fdesc, "%s" NS_LINEBREAK, nativemsg.get());
    }
    NS_IF_RELEASE(messages[i]);
  }

  PR_Close(fdesc);
  NS_Free(messages);
}
