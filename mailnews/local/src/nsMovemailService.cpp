/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Adam D. Moss <adam@gimp.org>
 * Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(DEBUG_adam) || defined(DEBUG_seth)
#define MOVEMAIL_DEBUG
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
#include "nsIFolder.h"

#include "nsILocalFile.h"
#include "nsFileStream.h"
#include "nsIFileSpec.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIPref.h"

#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsMsgFolderFlags.h"

#define PREF_MAIL_ROOT_MOVEMAIL "mail.root.movemail"

static NS_DEFINE_CID(kPrefCID,           NS_PREF_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsMovemailService::nsMovemailService()
{
    NS_INIT_REFCNT();
#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "*** MURRR, new nsMovemailService\n");
#endif
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

#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "*** WOOWOOWOO check\n");
#endif

    return rv;
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

    // Create nsFileSpec and nsILocalFile for the spool file
    nsFileSpec spoollocspec(spoolnameStr);
    nsCOMPtr<nsILocalFile> spoollocfile;
    rv = NS_FileSpecToIFile(&spoollocspec, getter_AddRefs(spoollocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;
    // Create nsFileSpec and nsILocalFile for the spool.mozlock file
    nsFileSpec tmplocspec(mozlockstr.get());
    nsCOMPtr<nsILocalFile> tmplocfile;
    rv = NS_FileSpecToIFile(&tmplocspec, getter_AddRefs(tmplocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;
    // Create nsFileSpec and nsILocalFile for the spool.lock file
    nsFileSpec locklocspec(lockstr.get());
    nsCOMPtr<nsILocalFile> locklocfile;
    rv = NS_FileSpecToIFile(&locklocspec, getter_AddRefs(locklocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;
    // THOUGHT: hmm, perhaps use MakeUnique to generate us a unique mozlock?
    // ... perhaps not, MakeUnique implementation looks racey -- use mktemp()?

    // step 1: create SPOOLNAME.mozlock
#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "\n ...... maker(%s) ......\n",
            mozlockstr.get());
#endif
    rv = tmplocfile->Create(nsIFile::NORMAL_FILE_TYPE, 0666);
    if ( (NS_FAILED(rv) &&
          rv != NS_ERROR_FILE_ALREADY_EXISTS) ||
         !tmplocfile) {
        // can't create our .mozlock file... game over already
#ifdef MOVEMAIL_DEBUG
        fprintf(stderr, "\n cannot create blah.mozlock file! \n");
#endif
        return PR_FALSE;
    }

    // step 2: hard-link .mozlock file to .lock file (this wackiness
    //         is necessary for non-racey locking on NFS-mounted spool dirs)
    // n.b. XPCOM utilities don't support hard-linking yet, so we
    // skip out to <unistd.h> and the POSIX interface for link()
    int link_result = 0;
    int retry_count = 0;
    
    do {
        link_result =
            link(mozlockstr.get(),lockstr.get());

        retry_count++;

#ifdef MOVEMAIL_DEBUG
        fprintf(stderr, "[try#%d] ", retry_count);
#endif

        if ((seconds > 0) &&
            (link_result == -1)) {
            // pause 1sec, waiting for .lock to go away
            PRIntervalTime sleepTime = 1000; // 1 second
            PR_Sleep(sleepTime);
        }
    } while ((link_result == -1) && (retry_count < seconds));
#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "<<link result: %d>>", link_result);
#endif

    // step 3: remove .mozlock file, in any case
    rv = tmplocfile->Remove(PR_FALSE /* non-recursive */);
#ifdef MOVEMAIL_DEBUG
    if (NS_FAILED(rv)) {
        // Could not delete our .mozlock file... very unusual, but
        // not fatal.
        fprintf(stderr,
                "\nBizarre, could not delete our .mozlock file.  Oh well.\n");
    }
#endif

#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "::got to the end! %s\n",
            (link_result == 0) ? "GOT LOCK" : "DID NOT GET LOCK");
#endif

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
#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "<<Attempting lockfile removal: %s.lock>>",
            spoolnameStr);
#endif

    nsCAutoString lockstr(spoolnameStr);
    lockstr.Append(".lock");

    nsresult rv;

    // Create nsFileSpec and nsILocalFile for the spool.lock file
    nsFileSpec locklocspec(lockstr.get());
    nsCOMPtr<nsILocalFile> locklocfile;
    rv = NS_FileSpecToIFile(&locklocspec, getter_AddRefs(locklocfile));
    if (NS_FAILED(rv))
        return PR_FALSE;

    // Delete the file
    rv = locklocfile->Remove(PR_FALSE /* non-recursive */);
    if (NS_FAILED(rv))
        return PR_FALSE;

#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, " LOCK YIELDING WAS SUCCESSFUL.\n");
#endif

    // Success.
    return PR_TRUE;
}


nsInputFileStream * Probe_SpoolFilePath(const char *pathStr,
                                        PRBool *found_but_locked)
{
    *found_but_locked = PR_FALSE;
    if (!pathStr) return nsnull;
    nsInputFileStream * rtnStream = nsnull;
    nsFileSpec * filespec = new nsFileSpec(pathStr);

    if (!filespec)
        return nsnull;

    if ((!filespec->Failed()) &&
        filespec->IsFile()) {
#ifdef MOVEMAIL_DEBUG
        fprintf(stderr, "##%s@(%s)##", filespec->GetLeafName(),
                pathStr);
#endif
        
        // before opening (or truncating) the spool file
        // we need to successfully obtain its lock to ensure
        // exclusive access (conforming spool-molestors allowing).
        if (!ObtainSpoolLock(pathStr, 5 /* seconds of retrying */ )) {
            *found_but_locked = PR_TRUE;
        }
        else {    // got lock okay
            // open the file for reading
            rtnStream = new nsInputFileStream (*filespec);
            
            if (rtnStream) {
                if (rtnStream->failed() ||
                    !rtnStream->is_open()) {
                    delete rtnStream;
                    rtnStream = nsnull;
                }
            }
        }
    }
    
    delete filespec;
    return rtnStream;
}


nsresult
nsMovemailService::GetNewMail(nsIMsgWindow *aMsgWindow,
                              nsIUrlListener *aUrlListener,
                              nsIMsgFolder *aMsgFolder,
                              nsIMovemailIncomingServer *movemailServer,
                              nsIURI ** aURL)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIMsgIncomingServer> in_server =
        do_QueryInterface(movemailServer);
    nsCAutoString wholeboxname;

    if (in_server) {   
        in_server->SetServerBusy(PR_TRUE);

        // FUN STUFF starts here
        {
            // we now quest for the mail spool file...
            nsInputFileStream * spoolfile = nsnull;
            PRBool found_spool_but_it_is_locked = PR_FALSE;

            // If $(MAIL) is set then we have things easy.
            char * wholeboxname_from_env = PR_GetEnv("MAIL");
            if (wholeboxname_from_env) {
                wholeboxname.Assign(wholeboxname_from_env);
                spoolfile = Probe_SpoolFilePath(wholeboxname_from_env,
                                                &found_spool_but_it_is_locked);
            } else {
                // Otherwise try to build the mailbox path from the
                // username and a number of guessed spool directory
                // paths.
                char * boxfilename;
                boxfilename = PR_GetEnv("USER"); // UNIXy
                if (!boxfilename) {
                    boxfilename = PR_GetEnv("USERNAME"); // WIN32 (!)
                }

                if (boxfilename) {
                    // We have the username which is probably also the
                    //  mailbox file name, so now try to find the mailbox
                    //  in a number of likely places.

                    wholeboxname = "/var/spool/mail/";
                    wholeboxname += boxfilename;
                    spoolfile = Probe_SpoolFilePath(wholeboxname.get(),
                                                    &found_spool_but_it_is_locked);
                    
                    if ((!spoolfile) && (!found_spool_but_it_is_locked)) {
                        wholeboxname = "/usr/spool/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath(wholeboxname.get(),
                                                        &found_spool_but_it_is_locked);
                    }

                    if ((!spoolfile) && (!found_spool_but_it_is_locked)) {
                        wholeboxname = "/var/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath(wholeboxname.get(),
                                                        &found_spool_but_it_is_locked);
                    }

                    if ((!spoolfile) && (!found_spool_but_it_is_locked)) {
                        wholeboxname = "/usr/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath(wholeboxname.get(),
                                                        &found_spool_but_it_is_locked);
                    }
                }
            }             
            
            if (!spoolfile) {
#ifdef MOVEMAIL_DEBUG
                fprintf(stderr,
                        "Nope, no luck getting spool file name at all.\n");
#endif
                return NS_ERROR_FAILURE;
            }
            
            if (spoolfile &&
                found_spool_but_it_is_locked) {
#ifdef MOVEMAIL_DEBUG
                fprintf(stderr,
                        "Well, got the spool file but not the lock.\n");
#endif
                return NS_ERROR_FAILURE;
            }
            
#define READBUFSIZE 4096
            char * buffer = (char*)PR_CALLOC(READBUFSIZE);
            if (!buffer)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else {
                if (!spoolfile->failed()) {
                    nsIOFileStream* outFileStream;
                    nsParseNewMailState *newMailParser;
                        
                    nsCOMPtr<nsIFileSpec> mailDirectory;
                    rv = in_server->GetLocalPath(getter_AddRefs(mailDirectory));
                    if (NS_FAILED(rv))
                        goto freebuff_and_unlock;
                        
                    nsFileSpec fileSpec;
                    mailDirectory->GetFileSpec(&fileSpec);
                    fileSpec += "Inbox";
                    outFileStream =
                        new nsIOFileStream(fileSpec /*, PR_CREATE_FILE */);
                    if (outFileStream) {
                        outFileStream->seek(fileSpec.GetFileSize());
                    }
                    else {
                        rv = NS_ERROR_UNEXPECTED;
                        goto freebuff_and_unlock;
                    }
                        
                    // create a new mail parser
                    newMailParser = new nsParseNewMailState;
                    if (newMailParser == nsnull) {
                        rv = NS_ERROR_OUT_OF_MEMORY;
                        goto freebuff_and_unlock;
                    }
                        
                    nsCOMPtr <nsIMsgParseMailMsgState> iParseMailMsgState = NS_STATIC_CAST(nsIMsgParseMailMsgState*, newMailParser);
                    nsCOMPtr <nsIFolder> serverFolder;
                    rv =
                        in_server->GetRootFolder(getter_AddRefs(serverFolder));

                    if (NS_FAILED(rv))
                        goto freebuff_and_unlock;
                    
	            nsCOMPtr<nsIMsgFolder> inbox;
	            if(NS_SUCCEEDED(rv))
	            {
		      nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(serverFolder);
		      if(rootMsgFolder)
		      {
			      PRUint32 numFolders;
			      rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1,
                                                         &numFolders,
                                                         getter_AddRefs(inbox));
                      }
                    }
                    rv = newMailParser->Init(serverFolder, inbox, 
                                               fileSpec, outFileStream);
                    if (NS_FAILED(rv))
                        goto freebuff_and_unlock;
                        
                        
                    // MIDDLE of the FUN : consume the mailbox data.
                    {
                        int numlines = 0;

                        // main parsing loop
                        while (!spoolfile->eof() &&
                               !spoolfile->failed() &&
                               spoolfile->is_open())
                            {
                                spoolfile->readline(buffer,
                                                    READBUFSIZE-(1+PL_strlen(MSG_LINEBREAK)));

                                // 'From' lines delimit messages
                                if ((numlines>0) &&
                                    nsCRT::strncmp(buffer, "From ", 5) == 0) {
                                    numlines = 0;
                                }

                                // If first string is empty and we're now at EOF
                                // (or, alternatively, we got a 'From' line and
                                // then the file mysteriously ended) then abort
                                // parsing.
                                if (numlines == 0 &&
                                    nsCRT::strlen(buffer) == 0 &&
                                    spoolfile->eof()) {
#ifdef MOVEMAIL_DEBUG
                                    fprintf(stderr, "*** Utterly empty spool file\n");
#endif
                                    break;   // end the parsing loop right here
                                }

                                PL_strcpy(&buffer[PL_strlen(buffer)], MSG_LINEBREAK);

                                //     fprintf(stderr, "%d: %s", numlines, buffer);

                                newMailParser->HandleLine(buffer, PL_strlen(buffer));
                                *outFileStream << buffer;

                                numlines++;

                                if (numlines == 1 &&
                                    !spoolfile->eof()) {
                                    PL_strcpy(buffer, "X-Mozilla-Status: 8000"
                                              MSG_LINEBREAK);
                                    newMailParser->HandleLine(buffer,
                                                                PL_strlen(buffer));
                                    *outFileStream << buffer;
                                    PL_strcpy(buffer, "X-Mozilla-Status2: 00000000"
                                              MSG_LINEBREAK);
                                    newMailParser->HandleLine(buffer,
                                                                PL_strlen(buffer));
                                    *outFileStream << buffer;
                                            
                                }
                            }
                    }
                        
                        
                    // END
                    outFileStream->flush();	// try this.
                    newMailParser->OnStopRequest(nsnull, nsnull, NS_OK);
                    if (outFileStream->is_open())
                      outFileStream->close();
                    delete outFileStream;
                    outFileStream = 0;

                    // truncate the spool file here.
                    nsFileSpec * filespecForTrunc =
                        new nsFileSpec(wholeboxname.get());
                    if (filespecForTrunc) {
                        filespecForTrunc->Truncate(0);
                        delete filespecForTrunc;
                        filespecForTrunc = nsnull;
                    }
                    if (spoolfile->is_open())
                      spoolfile->close();
                    delete spoolfile;
                }
           
            freebuff_and_unlock:
                PR_Free(buffer);
            }

            YieldSpoolLock(wholeboxname.get());
        }

        in_server->SetServerBusy(PR_FALSE);

    } else {
#ifdef MOVEMAIL_DEBUG
        fprintf(stderr, "*** NONONO get:noserv\n");
#endif
    }

#ifdef MOVEMAIL_DEBUG
    fprintf(stderr, "*** YEAHYEAHYEAH get, %s\n",
            NS_FAILED(rv) ? "MISERABLE FAILURE" :
            "UNPRECEDENTED SUCCESS");
#endif

    return rv;
}


NS_IMETHODIMP
nsMovemailService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_MOVEMAIL, aPath, PR_FALSE /* set default */);
    return rv;
}     

NS_IMETHODIMP
nsMovemailService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref = PR_FALSE;
    nsCOMPtr<nsILocalFile> prefLocal;
    nsCOMPtr<nsIFile> localFile;
    rv = prefs->GetFileXPref(PREF_MAIL_ROOT_MOVEMAIL, getter_AddRefs(prefLocal));
    if (NS_SUCCEEDED(rv)) {
        localFile = prefLocal;
        havePref = PR_TRUE;
    }
    if (!localFile) {
        rv = NS_GetSpecialDirectory(NS_APP_MAIL_50_DIR, getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        havePref = PR_FALSE;
    }
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsXPIDLCString pathBuf;
    rv = localFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpec(getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    outSpec->SetNativePath(pathBuf);
    
    if (!havePref || !exists)
        rv = SetDefaultLocalPath(outSpec);
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
    return rv;
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

#ifdef MOVEMAIL_DEBUG
        fprintf(stderr, ">>> Yah, got asked for info\n");
#endif

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
