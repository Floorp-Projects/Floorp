/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include "ipcIService.h"
#include "ipcIDConnectService.h"
#include "ipcCID.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"

#include "nsXPCOMCID.h"
#include "nsILocalFile.h"

#include "nsString.h"
#include "prmem.h"

#if defined( XP_WIN ) || defined( XP_OS2 )
#define TEST_PATH "c:"
#else
#define TEST_PATH "/tmp"
#endif

#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf("*** %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nsnull;
static PRBool gKeepRunning = PR_TRUE;

static ipcIService *gIpcServ = nsnull;

static nsresult DoTest()
{
  nsresult rv;

  nsCOMPtr<ipcIDConnectService> dcon = do_GetService("@mozilla.org/ipc/dconnect-service;1", &rv);
  RETURN_IF_FAILED(rv, "getting dconnect service");

  PRUint32 remoteClientID = 1;

  nsCOMPtr<nsIFile> file;
  rv = dcon->CreateInstanceByContractID(remoteClientID,
                                        NS_LOCAL_FILE_CONTRACTID,
                                        NS_GET_IID(nsIFile),
                                        getter_AddRefs(file)); 
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> sup = do_QueryInterface(file, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  printf("*** calling QueryInterface\n");
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString path;
  path.AssignLiteral(TEST_PATH);

  printf("*** calling InitWithNativePath\n");
  rv = localFile->InitWithPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString buf;
  rv = file->GetPath(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!buf.Equals(path))
  {
    NS_ConvertUTF16toUTF8 temp(buf);
    printf("*** GetPath erroneously returned [%s]\n", temp.get());
    return NS_ERROR_FAILURE;
  }

  PRBool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv))
  {
    printf("*** Exists test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }
  printf("File exists? [%d]\n", exists);

  nsCOMPtr<nsIFile> clone;
  rv = file->Clone(getter_AddRefs(clone));
  if (NS_FAILED(rv))
  {
    printf("*** Clone test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  nsAutoString node;
  node.AssignLiteral("hello.txt");

  rv = clone->Append(node);
  if (NS_FAILED(rv))
  {
    printf("*** Append test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  PRBool match;
  rv = file->Equals(clone, &match);
  if (NS_FAILED(rv))
  {
    printf("*** Equals test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }
  printf("Files are equals? [%d]\n", match);

  // now test passing null for interface pointer

  rv = clone->Exists(&exists);
  if (NS_FAILED(rv))
  {
    printf("*** Exists test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }
  if (!exists)
  {
    rv = clone->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv))
    {
      printf("*** Create test failed [rv=%x]\n", rv);
      return NS_ERROR_FAILURE;
    }
  }

  rv = clone->MoveTo(nsnull, NS_LITERAL_STRING("helloworld.txt"));
  if (NS_FAILED(rv))
  {
    printf("*** MoveTo test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  // now test passing local objects to a remote object

  nsCOMPtr<nsILocalFile> myLocalFile;
  rv = NS_NewLocalFile(path, PR_TRUE, getter_AddRefs(myLocalFile));
  if (NS_FAILED(rv))
  {
    printf("*** NS_NewLocalFile failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }

  rv = file->Equals(myLocalFile, &match);
  if (NS_FAILED(rv))
  {
    printf("*** second Equals test failed [rv=%x]\n", rv);
    return NS_ERROR_FAILURE;
  }
  printf("Files are equals? [%d]\n", match);

  printf("*** DoTest completed successfully :-)\n");
  return NS_OK;
}

int main(int argc, char **argv)
{
  nsresult rv;

  PRBool serverMode = PR_FALSE;
  if (argc > 1)
  {
    if (strcmp(argv[1], "-server") == 0)
    {
      serverMode = PR_TRUE;
    }
    else
    {
      printf("usage: %s [-server]\n", argv[0]);
      return -1;
    }
  }

  {
    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    if (registrar)
        registrar->AutoRegister(nsnull);

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eqs =
             do_GetService(kEventQueueServiceCID, &rv);
    RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

    rv = eqs->CreateMonitoredThreadEventQueue();
    RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    RETURN_IF_FAILED(rv, "GetThreadEventQueue");

    nsCOMPtr<ipcIService> ipcServ(do_GetService(IPC_SERVICE_CONTRACTID, &rv));
    RETURN_IF_FAILED(rv, "do_GetService(ipcServ)");
    NS_ADDREF(gIpcServ = ipcServ);

    if (!serverMode)
    {
      rv = DoTest();
      RETURN_IF_FAILED(rv, "DoTest()");
    }
    else
    {
      gIpcServ->AddName("DConnectServer");
    }

    PLEvent *ev;
    while (gKeepRunning)
    {
      gEventQ->WaitForEvent(&ev);
      gEventQ->HandleEvent(ev);
    }

    NS_RELEASE(gIpcServ);

    printf("*** processing remaining events\n");

    // process any remaining events
    while (NS_SUCCEEDED(gEventQ->GetEvent(&ev)) && ev)
        gEventQ->HandleEvent(ev);

    printf("*** done\n");
  } // this scopes the nsCOMPtrs

  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
}
