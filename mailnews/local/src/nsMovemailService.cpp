/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 * Adam D. Moss <adam@gimp.org>
 */

#include "prenv.h"

#include "msgCore.h"    // precompiled header...

#include "nsMovemailService.h"
#include "nsIMovemailService.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMovemailIncomingServer.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"
#include "nsParseMailbox.h"
#include "nsIFolder.h"

#include "nsFileSpec.h"
#include "nsFileStream.h"

#include "nsIPref.h"

#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

#include "nsIFileLocator.h"
#include "nsFileLocations.h"

#define PREF_MAIL_ROOT_MOVEMAIL "mail.root.movemail"

static NS_DEFINE_CID(kPrefCID,           NS_PREF_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_IID(kIFileLocatorIID,   NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID,    NS_FILELOCATOR_CID);

nsMovemailService::nsMovemailService()
{
    NS_INIT_REFCNT();
    //fprintf(stderr, "*** MURRR, new nsMovemailService\n");
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

    //fprintf(stderr, "*** WOOWOOWOO check\n");

    return rv;
}


// one day we will do mail file locking right here.
nsInputFileStream * Probe_SpoolFilePath(const char *pathStr)
{
    if (!pathStr) return nsnull;
    nsInputFileStream * rtnStream = nsnull;
    nsFileSpec * filespec = new nsFileSpec(pathStr);

    if (!filespec)
        return nsnull;

    if (!filespec->Failed() &&
        filespec->IsFile())
        {
            //            fprintf(stderr, "##%s##", filespec->GetLeafName());

            rtnStream = new nsInputFileStream (*filespec);
            
            if (rtnStream) {
                if (rtnStream->failed() ||
                    !rtnStream->is_open()) {
                    delete rtnStream;
                    rtnStream = nsnull;
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

    nsCOMPtr<nsIMsgIncomingServer> in_server = do_QueryInterface(movemailServer);
    nsCAutoString wholeboxname;

    if (in_server) {   
        nsCOMPtr<nsIFileSpec> mailDirectory;

        in_server->SetServerBusy(PR_TRUE);
        rv = in_server->GetLocalPath(getter_AddRefs(mailDirectory));

        // FUN STUFF starts here
        {
            // we now quest for the mail spool file...
            nsInputFileStream * spoolfile = nsnull;

            // If $(MAIL) is set then we have things easy.
            char * wholeboxname_from_env = PR_GetEnv("MAIL");
            if (wholeboxname_from_env) {
		wholeboxname.Assign(wholeboxname_from_env);
                spoolfile = Probe_SpoolFilePath(wholeboxname_from_env);
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
                    spoolfile = Probe_SpoolFilePath((const char *)wholeboxname);
                    
                    if (!spoolfile) {
                        wholeboxname = "/usr/spool/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath((const char *)wholeboxname);
                    }

                    if (!spoolfile) {
                        wholeboxname = "/var/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath((const char *)wholeboxname);
                    }

                    if (!spoolfile) {
                        wholeboxname = "/usr/mail/";
                        wholeboxname += boxfilename;
                        spoolfile = Probe_SpoolFilePath((const char *)wholeboxname);
                    }
                }
            }
                                
            if (!spoolfile) {
                //fprintf(stderr, "Nope, no luck getting spool file name at all.\n");
                return NS_ERROR_FAILURE;
            }

#define READBUFSIZE 4096
            char * buffer = (char*)PR_CALLOC(READBUFSIZE);
            if (buffer) {
                if (!spoolfile->failed()) {
                    nsresult rv;
                    nsIOFileStream* m_outFileStream;
                    nsParseNewMailState *m_newMailParser;
                        
                    nsCOMPtr<nsIFileSpec> mailDirectory;
                    rv = in_server->GetLocalPath(getter_AddRefs(mailDirectory));
                    if (NS_FAILED(rv)) return rv;
                        
                    nsFileSpec fileSpec;
                    mailDirectory->GetFileSpec(&fileSpec);
                    fileSpec += "Inbox";
                    m_outFileStream = new nsIOFileStream(fileSpec /*, PR_CREATE_FILE */);
                    if (m_outFileStream)
                        m_outFileStream->seek(fileSpec.GetFileSize());
                    else
                        return NS_ERROR_UNEXPECTED;
                        
                    // create a new mail parser
                    m_newMailParser = new nsParseNewMailState;
                    if (m_newMailParser == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;
                        
                    nsCOMPtr <nsIFolder> serverFolder;
                    rv = in_server->GetRootFolder(getter_AddRefs(serverFolder));

                    if (NS_FAILED(rv))
                        return rv;
                        
                    rv = m_newMailParser->Init(serverFolder, fileSpec, m_outFileStream);
                    if (NS_FAILED(rv))
                        return rv;
                        
                        
                    // MIDDLE of the FUN : consume the mailbox data.
                    {
                        int numlines = 0;

                        while (!spoolfile->eof() &&
                               !spoolfile->failed() &&
                               spoolfile->is_open())
                            {
                                spoolfile->readline(buffer,
                                                    READBUFSIZE-(1+PL_strlen(MSG_LINEBREAK)));


                                PL_strcpy(&buffer[PL_strlen(buffer)], MSG_LINEBREAK);

                                //     fprintf(stderr, "%d: %s", numlines, buffer);

                                if ((numlines>0) &&
                                    nsCRT::strncmp(buffer, "From ", 5) == 0) {
                                    numlines = 0;
                                }

                                m_newMailParser->HandleLine(buffer, PL_strlen(buffer));
                                *m_outFileStream << buffer;

                                numlines++;

                                if (numlines == 1) {
                                    PL_strcpy(buffer, "X-Mozilla-Status: 8000"
                                              MSG_LINEBREAK);
                                    m_newMailParser->HandleLine(buffer,
                                                                PL_strlen(buffer));
                                    *m_outFileStream << buffer;
                                    PL_strcpy(buffer, "X-Mozilla-Status2: 00000000"
                                              MSG_LINEBREAK);
                                    m_newMailParser->HandleLine(buffer,
                                                                PL_strlen(buffer));
                                    *m_outFileStream << buffer;
                                            
                                }
                            }
                    }
                        
                        
                    // END
                    m_outFileStream->flush();	// try this.
                    m_newMailParser->OnStopRequest(nsnull, nsnull, NS_OK, nsnull);
                    delete m_outFileStream;
                    m_outFileStream = 0;
                    delete m_newMailParser;

                    // truncate the spool file here.
		    nsFileSpec * filespecForTrunc = new nsFileSpec((const char *)wholeboxname);
		    if (filespecForTrunc) {
			filespecForTrunc->Truncate(0);
			delete filespecForTrunc;
			filespecForTrunc = nsnull;
		    }
                    delete spoolfile;
                }
                        
                PR_Free(buffer);
            }
        }

        
        in_server->SetServerBusy(PR_FALSE);
    } else {
        //fprintf(stderr, "*** NONONO get:noserv\n");
    }

    //fprintf(stderr, "*** YEAHYEAHYEAH get\n");

    return rv;
}


NS_IMETHODIMP
nsMovemailService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_MOVEMAIL, aPath, PR_FALSE /* set default */);
    return rv;
}     

NS_IMETHODIMP
nsMovemailService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->GetFilePref(PREF_MAIL_ROOT_MOVEMAIL, aResult);
    if (NS_SUCCEEDED(rv)) return rv;

    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_MailDirectory50, aResult);
    if (NS_FAILED(rv)) return rv;    

    rv = SetDefaultLocalPath(*aResult);
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

        //fprintf(stderr, ">>> Yah, got asked for info\n");

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
nsMovemailService::GetCanDelete(PRBool *aCanDelete)
{
        NS_ENSURE_ARG_POINTER(aCanDelete);
        *aCanDelete = PR_FALSE;
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
nsMovemailService::GetDefaultCopiesAndFoldersPrefsToServer(PRBool *aDefaultCopiesAndFoldersPrefsToServer)
{
    NS_ENSURE_ARG_POINTER(aDefaultCopiesAndFoldersPrefsToServer);
    // when a "movemail" server is created (like "Local Folders")
	// the copies and folder prefs for the associated identity
    // point to folders on this server.
    *aDefaultCopiesAndFoldersPrefsToServer = PR_TRUE;
    return NS_OK;
}   

NS_IMETHODIMP
nsMovemailService::GetDefaultServerPort(PRInt32 *aDefaultPort)
{
    NS_ASSERTION(0, "This should probably never be called!");
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = -1;
    return NS_OK;
}

