/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam D. Moss <adam@gimp.org>
 *   Seth Spitzer <sspitzer@netscape.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include <unistd.h>    // for link(), used in spool-file locking

#include "prenv.h"
#include "nspr.h"

#include "msgCore.h"    // precompiled header...

#include "nsMovemailService.h"
#include "nsIMovemailService.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMovemailIncomingServer.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"
#include "nsParseMailbox.h"
#include "nsIMsgFolder.h"
#include "nsIPrompt.h"

#include "nsILocalFile.h"
#include "nsFileStream.h"
#include "nsIFileSpec.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsMsgUtils.h"

#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsMsgFolderFlags.h"

#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"

#include "prlog.h"
#if defined(PR_LOGGING)
//
// export NSPR_LOG_MODULES=Movemail:5
//
static PRLogModuleInfo *gMovemailLog = nsnull;
#define LOG(args) PR_LOG(gMovemailLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

#define PREF_MAIL_ROOT_MOVEMAIL "mail.root.movemail"            // old - for backward compatibility only
#define PREF_MAIL_ROOT_MOVEMAIL_REL "mail.root.movemail-rel"

const char * gDefaultSpoolPaths[] = {
    "/var/spool/mail/",
    "/usr/spool/mail/",
    "/var/mail/",
    "/usr/mail/"
};
#define NUM_DEFAULT_SPOOL_PATHS (sizeof(gDefaultSpoolPaths)/sizeof(gDefaultSpoolPaths[0]))

nsMovemailService::nsMovemailService()
{
#if defined(PR_LOGGING)
    if (!gMovemailLog)
        gMovemailLog = PR_NewLogModule("Movemail");
#endif
    LOG(("nsMovemailService created: 0x%x\n", this));
    mStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_CONTRACTID);
}

nsMovemailService::~nsMovemailService()
{}


NS_IMPL_ISUPPORTS2(nsMovemailService,
                   nsIMovemailService,
                   nsIMsgProtocolInfo)


NS_IMETHODIMP
nsMovemailService::CheckForNewMail(nsIUrlListener * aUrlListener,
                                   nsIMsgFolder *inbox,
                                   nsIMovemailIncomingServer *movemailServer,
                                   nsIURI ** aURL)
{
    nsresult rv = NS_OK;
    LOG(("nsMovemailService::CheckForNewMail\n"));
    return rv;
}


void
nsMovemailService::Error(PRInt32 errorCode,
                         const PRUnichar **params,
                         PRUint32 length)
{
    if (!mStringService) return;
    if (!mMsgWindow) return;

    nsCOMPtr<nsIPrompt> dialog;
    nsresult rv = mMsgWindow->GetPromptDialog(getter_AddRefs(dialog));
    if (NS_FAILED(rv))
        return;

    nsXPIDLString errStr;

    // Format the error string if necessary
    if (params) {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = mStringService->GetBundle(getter_AddRefs(bundle));
        if (NS_SUCCEEDED(rv))
            bundle->FormatStringFromID(errorCode, params, length,
                                       getter_Copies(errStr));
    }
    else {
        mStringService->GetStringByID(errorCode, getter_Copies(errStr));
    }

    if (!errStr.IsEmpty()) {
        dialog->Alert(nsnull, errStr.get());
    }
}


PRBool ObtainSpoolLock(const char *spoolnameStr,
                       int seconds /* number of seconds to retry */)
{
    // How to lock:
    // step 1: create SPOOLNAME.mozlock
    //        1a: can remove it if it already exists (probably crash-droppings)
    // step 2: hard-link SPOOLNAME.mozlock to SPOOLNAME.lock for NFS atomicity
    //        2a: if SPOOLNAME.lock is >60sec old then nuke it from orbit
    //        2b: repeat step 2 until retry-count expired or hard-link succeeds
    // step 3: remove SPOOLNAME.mozlock
    // step 4: If step 2 hard-link failed, fail hard; we do not hold the lock
    // DONE.
    //
    // (step 2a not yet implemented)
    
    nsCAutoString mozlockstr(spoolnameStr);
    mozlockstr.Append(".mozlock");
    nsCAutoString lockstr(spoolnameStr);
    lockstr.Append(".lock");

    nsresult rv;

    // Create nsILocalFile for the spool.mozlock file
    nsCOMPtr<nsILocalFile> tmplocfile;
    rv = NS_NewNativeLocalFile(mozlockstr, PR_TRUE, getter_AddRefs(tmplocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;
    // THOUGHT: hmm, perhaps use MakeUnique to generate us a unique mozlock?
    // ... perhaps not, MakeUnique implementation looks racey -- use mktemp()?

    // step 1: create SPOOLNAME.mozlock
    rv = tmplocfile->Create(nsIFile::NORMAL_FILE_TYPE, 0666);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
        // can't create our .mozlock file... game over already
        LOG(("Failed to create file %s\n", mozlockstr.get()));
        return PR_FALSE;
    }

    // step 2: hard-link .mozlock file to .lock file (this wackiness
    //         is necessary for non-racey locking on NFS-mounted spool dirs)
    // n.b. XPCOM utilities don't support hard-linking yet, so we
    // skip out to <unistd.h> and the POSIX interface for link()
    int link_result = 0;
    int retry_count = 0;
    
    do {
        link_result = link(mozlockstr.get(),lockstr.get());

        retry_count++;
        LOG(("Attempt %d of %d to create lock file", retry_count, seconds));

        if (seconds > 0 && link_result == -1) {
            // pause 1sec, waiting for .lock to go away
            PRIntervalTime sleepTime = 1000; // 1 second
            PR_Sleep(sleepTime);
        }
    } while (link_result == -1 && retry_count < seconds);
    LOG(("Link result: %d", link_result));

    // step 3: remove .mozlock file, in any case
    rv = tmplocfile->Remove(PR_FALSE /* non-recursive */);
    if (NS_FAILED(rv)) {
        // Could not delete our .mozlock file... very unusual, but
        // not fatal.
        LOG(("Unable to delete %s", mozlockstr.get()));
    }

    // step 4: now we know whether we succeeded or failed
    if (link_result == 0)
        return PR_TRUE; // got the lock.
    else
        return PR_FALSE; // didn't.  :(
}


// Remove our mail-spool-file lock (n.b. we should only try this if
// we're the ones who made the lock in the first place!)
PRBool YieldSpoolLock(const char *spoolnameStr)
{
    LOG(("YieldSpoolLock(%s)", spoolnameStr));

    nsCAutoString lockstr(spoolnameStr);
    lockstr.Append(".lock");

    nsresult rv;

    // Create nsILocalFile for the spool.lock file
    nsCOMPtr<nsILocalFile> locklocfile;
    rv = NS_NewNativeLocalFile(lockstr, PR_TRUE, getter_AddRefs(locklocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;

    // Check if the lock file exists
    PRBool exists;
    rv = locklocfile->Exists(&exists);
    if (NS_FAILED(rv))
        return PR_FALSE;

    // Delete the file if it exists
    if (exists) {
        rv = locklocfile->Remove(PR_FALSE /* non-recursive */);
        if (NS_FAILED(rv))
            return PR_FALSE;
    }

    LOG(("YieldSpoolLock was successful."));

    // Success.
    return PR_TRUE;
}

static nsresult
LocateSpoolFile(nsACString & spoolPath)
{
    PRBool isFile;
    nsresult rv;

    nsCOMPtr<nsILocalFile> spoolFile;
    rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(spoolFile));
    NS_ENSURE_SUCCESS(rv, rv);

    char * mailEnv = PR_GetEnv("MAIL");
    char * userEnv = PR_GetEnv("USER");
    if (!userEnv)
        userEnv = PR_GetEnv("USERNAME");

    if (mailEnv) {
        rv = spoolFile->InitWithNativePath(nsDependentCString(mailEnv));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = spoolFile->IsFile(&isFile);
        if (NS_SUCCEEDED(rv) && isFile)
            spoolPath = mailEnv;
    }
    else if (userEnv) {
        // Try to build the mailbox path from the username and a number
        // of guessed spool directory paths.
        nsCAutoString tmpPath;
        PRUint32 i;
        for (i = 0; i < NUM_DEFAULT_SPOOL_PATHS; i++) {
            tmpPath = gDefaultSpoolPaths[i];
            tmpPath += userEnv;
            rv = spoolFile->InitWithNativePath(tmpPath);
            NS_ENSURE_SUCCESS(rv, rv);
            rv = spoolFile->IsFile(&isFile);
            if (NS_SUCCEEDED(rv) && isFile) {
                spoolPath = tmpPath;
                break;
            }
        }
    }

    return rv;
}

nsresult
nsMovemailService::GetNewMail(nsIMsgWindow *aMsgWindow,
                              nsIUrlListener *aUrlListener,
                              nsIMsgFolder *aMsgFolder,
                              nsIMovemailIncomingServer *movemailServer,
                              nsIURI ** aURL)
{
    LOG(("nsMovemailService::GetNewMail"));
    nsresult rv = NS_OK;

    nsCOMPtr<nsIMsgIncomingServer> in_server =
        do_QueryInterface(movemailServer);
    if (!in_server)
        return NS_MSG_INVALID_OR_MISSING_SERVER;
    mMsgWindow = aMsgWindow;

    // Attempt to locate the mail spool file
    nsCAutoString spoolPath;
    rv = LocateSpoolFile(spoolPath);
    if (NS_FAILED(rv) || spoolPath.IsEmpty()) {
        Error(MOVEMAIL_SPOOL_FILE_NOT_FOUND, nsnull, 0);
        return NS_ERROR_FAILURE;
    }

    // Create an input stream for the spool file
    nsCOMPtr<nsILocalFile> spoolFile;
    rv = NS_NewNativeLocalFile(spoolPath, PR_TRUE, getter_AddRefs(spoolFile));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIInputStream> spoolInputStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(spoolInputStream), spoolFile);
    if (NS_FAILED(rv)) {
        const PRUnichar *params[] = {
            NS_ConvertUTF8toUCS2(spoolPath).get()
        };
        Error(MOVEMAIL_CANT_OPEN_SPOOL_FILE, params, 1);
        return rv;
    }

    // Get a line input interface for the spool file
    nsCOMPtr<nsILineInputStream> lineInputStream =
        do_QueryInterface(spoolInputStream, &rv);
    if (!lineInputStream)
        return rv;

    nsCOMPtr<nsIFileSpec> mailDirectory;
    rv = in_server->GetLocalPath(getter_AddRefs(mailDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    nsFileSpec fileSpec;
    mailDirectory->GetFileSpec(&fileSpec);
    fileSpec += "Inbox";
    nsIOFileStream outFileStream(fileSpec);
    outFileStream.seek(fileSpec.GetFileSize());
    nsCOMPtr<nsIMsgFolder> serverFolder;
    nsCOMPtr<nsIMsgFolder> inbox;
    nsCOMPtr<nsIMsgFolder> rootMsgFolder;
        
    // create a new mail parser
    nsRefPtr<nsParseNewMailState> newMailParser = new nsParseNewMailState;
    if (newMailParser == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = in_server->GetRootFolder(getter_AddRefs(serverFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    rootMsgFolder = do_QueryInterface(serverFolder, &rv);
    if (!rootMsgFolder)
        return rv;
    PRUint32 numFolders;
    rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1,
                                           &numFolders,
                                           getter_AddRefs(inbox));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = newMailParser->Init(serverFolder, inbox, 
                             fileSpec, &outFileStream, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    in_server->SetServerBusy(PR_TRUE);

    // Try and obtain the lock for the spool file
    if (!ObtainSpoolLock(spoolPath.get(), 5)) {
        nsAutoString lockFile = NS_ConvertUTF8toUCS2(spoolPath);
        lockFile.AppendLiteral(".lock");
        const PRUnichar *params[] = {
            lockFile.get()
        };
        Error(MOVEMAIL_CANT_CREATE_LOCK, params, 1);
        return NS_ERROR_FAILURE;
    }
            
    // MIDDLE of the FUN : consume the mailbox data.
    PRBool isMore = PR_TRUE;
    nsCAutoString buffer;

    while (isMore &&
           NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore)))
    {

        // If first string is empty and we're now at EOF then abort parsing.
        if (buffer.IsEmpty() && !isMore) {
            LOG(("Empty spool file"));
            break;
        }

        buffer += MSG_LINEBREAK;

        newMailParser->HandleLine(buffer.BeginWriting(), buffer.Length());
        outFileStream << buffer.get();

        // 'From' lines delimit messages
        if (isMore && !strncmp(buffer.get(), "From ", 5)) {
            buffer.AssignLiteral("X-Mozilla-Status: 8000" MSG_LINEBREAK);
            newMailParser->HandleLine(buffer.BeginWriting(), buffer.Length());
            outFileStream << buffer.get();
            buffer.AssignLiteral("X-Mozilla-Status2: 00000000" MSG_LINEBREAK);
            newMailParser->HandleLine(buffer.BeginWriting(), buffer.Length());
            outFileStream << buffer.get();
        }
    }

    outFileStream.flush();
    newMailParser->OnStopRequest(nsnull, nsnull, NS_OK);
    newMailParser->SetDBFolderStream(nsnull); // stream is going away
    if (outFileStream.is_open())
        outFileStream.close();

    // Truncate the spool file
    rv = spoolFile->SetFileSize(0);
    if (NS_FAILED(rv)) {
        const PRUnichar *params[] = {
            NS_ConvertUTF8toUCS2(spoolPath).get()
        };
        Error(MOVEMAIL_CANT_TRUNCATE_SPOOL_FILE, params, 1);
    }

    if (!YieldSpoolLock(spoolPath.get())) {
        nsAutoString spoolLock = NS_ConvertUTF8toUCS2(spoolPath);
        spoolLock.AppendLiteral(".lock");
        const PRUnichar *params[] = {
            spoolLock.get()
        };
        Error(MOVEMAIL_CANT_DELETE_LOCK, params, 1);
    }

    in_server->SetServerBusy(PR_FALSE);

    LOG(("GetNewMail returning rv=%d", rv));
    return rv;
}


NS_IMETHODIMP
nsMovemailService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    NS_ENSURE_ARG(aPath);
    nsresult rv;
    
    nsFileSpec spec;
    rv = aPath->GetFileSpec(&spec);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> localFile;
    NS_FileSpecToIFile(&spec, getter_AddRefs(localFile));
    if (!localFile) return NS_ERROR_FAILURE;
    
    rv = NS_SetPersistentFile(PREF_MAIL_ROOT_MOVEMAIL_REL, PREF_MAIL_ROOT_MOVEMAIL, localFile);

    return rv;
}     

NS_IMETHODIMP
nsMovemailService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    PRBool havePref;
    nsCOMPtr<nsILocalFile> localFile;    
    rv = NS_GetPersistentFile(PREF_MAIL_ROOT_MOVEMAIL_REL,
                              PREF_MAIL_ROOT_MOVEMAIL,
                              NS_APP_MAIL_50_DIR,
                              havePref,
                              getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    
    if (!havePref || !exists) {
        rv = NS_SetPersistentFile(PREF_MAIL_ROOT_MOVEMAIL_REL, PREF_MAIL_ROOT_MOVEMAIL, localFile);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set root dir pref.");
    }
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}
    

NS_IMETHODIMP
nsMovemailService::GetServerIID(nsIID* *aServerIID)
{
    *aServerIID = new nsIID(NS_GET_IID(nsIMovemailIncomingServer));
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetRequiresUsername(PRBool *aRequiresUsername)
{
    NS_ENSURE_ARG_POINTER(aRequiresUsername);
    *aRequiresUsername = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetPreflightPrettyNameWithEmailAddress(PRBool *aPreflightPrettyNameWithEmailAddress)
{
    NS_ENSURE_ARG_POINTER(aPreflightPrettyNameWithEmailAddress);
    *aPreflightPrettyNameWithEmailAddress = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetCanLoginAtStartUp(PRBool *aCanLoginAtStartUp)
{
    NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
    *aCanLoginAtStartUp = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetCanDelete(PRBool *aCanDelete)
{
    NS_ENSURE_ARG_POINTER(aCanDelete);
    *aCanDelete = PR_TRUE;
    return NS_OK;
}  

NS_IMETHODIMP
nsMovemailService::GetCanGetMessages(PRBool *aCanGetMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetMessages);
    *aCanGetMessages = PR_TRUE;
    return NS_OK;
}  

NS_IMETHODIMP
nsMovemailService::GetCanGetIncomingMessages(PRBool *aCanGetIncomingMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetIncomingMessages);
    *aCanGetIncomingMessages = PR_TRUE;
    return NS_OK;
} 

NS_IMETHODIMP
nsMovemailService::GetCanDuplicate(PRBool *aCanDuplicate)
{
    NS_ENSURE_ARG_POINTER(aCanDuplicate);
    *aCanDuplicate = PR_FALSE;
    return NS_OK;
}  

NS_IMETHODIMP 
nsMovemailService::GetDefaultDoBiff(PRBool *aDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDoBiff);
    // by default, do biff for movemail
    *aDoBiff = PR_TRUE;    
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetDefaultServerPort(PRBool isSecure, PRInt32 *aDefaultPort)
{
    NS_ASSERTION(0, "This should probably never be called!");
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetShowComposeMsgLink(PRBool *showComposeMsgLink)
{
    NS_ENSURE_ARG_POINTER(showComposeMsgLink);
    *showComposeMsgLink = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetNeedToBuildSpecialFolderURIs(PRBool *needToBuildSpecialFolderURIs)
{
    NS_ENSURE_ARG_POINTER(needToBuildSpecialFolderURIs);
    *needToBuildSpecialFolderURIs = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailService::GetSpecialFoldersDeletionAllowed(PRBool *specialFoldersDeletionAllowed)
{
    NS_ENSURE_ARG_POINTER(specialFoldersDeletionAllowed);
    *specialFoldersDeletionAllowed = PR_TRUE;
    return NS_OK;
}
