/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"    // precompiled header...

#include "prprf.h"

#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCOMPtr.h"
#include "nsAutoLock.h"
#include "nsMemory.h"
#include "nsIStringBundle.h"
#include "nsEscape.h"

#include "nsMsgFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgKeyArray.h"
#include "nsMsgDatabase.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIdentity.h"
#include "nsMsgBaseCID.h"
#include "nsMsgUtils.h" // for NS_MsgHashIfNecessary()
#include "nsMsgI18N.h"

#include "nsIPref.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"

#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIDocShell.h"
#include "nsIMsgWindow.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#define PREF_MAIL_WARN_FILTER_CHANGED "mail.warn_filter_changed"

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgFolderListenerManagerCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

nsrefcnt nsMsgFolder::gInstanceCount	= 0;

PRUnichar *nsMsgFolder::kInboxName = 0;
PRUnichar *nsMsgFolder::kTrashName = 0;
PRUnichar *nsMsgFolder::kSentName = 0;
PRUnichar *nsMsgFolder::kDraftsName = 0;
PRUnichar *nsMsgFolder::kTemplatesName = 0;
PRUnichar *nsMsgFolder::kUnsentName = 0;

nsIAtom * nsMsgFolder::kTotalMessagesAtom	= nsnull;
nsIAtom * nsMsgFolder::kBiffStateAtom	= nsnull;
nsIAtom * nsMsgFolder::kNewMessagesAtom	= nsnull;
nsIAtom * nsMsgFolder::kNumNewBiffMessagesAtom	= nsnull;
nsIAtom * nsMsgFolder::kTotalUnreadMessagesAtom	= nsnull;
nsIAtom * nsMsgFolder::kFlaggedAtom	= nsnull;
nsIAtom * nsMsgFolder::kStatusAtom	= nsnull;
nsIAtom * nsMsgFolder::kNameAtom	= nsnull;
nsIAtom * nsMsgFolder::kSynchronizeAtom = nsnull;
nsIAtom * nsMsgFolder::kOpenAtom = nsnull;

#ifdef MSG_FASTER_URI_PARSING
nsCOMPtr<nsIURL> nsMsgFolder::mParsingURL;
PRBool nsMsgFolder::mParsingURLInUse=PR_FALSE;
#endif

nsMsgFolder::nsMsgFolder(void)
  : nsRDFResource(),
    mFlags(0),
    mNumUnreadMessages(-1),
    mNumTotalMessages(-1),
  	mNotifyCountChanges(PR_TRUE),
    mExpungedBytes(0),
    mInitializedFromCache(PR_FALSE),
    mBiffState(nsMsgBiffState_NoMail),
    mNumNewBiffMessages(0),
    mHaveParsedURI(PR_FALSE),
    mIsServerIsValid(PR_FALSE),
    mIsServer(PR_FALSE),
    mDeleteIsMoveToTrash(PR_TRUE),
    mBaseMessageURI(nsnull)
	{
//  NS_INIT_REFCNT(); done by superclass

  mSemaphoreHolder = NULL;

	mNumPendingUnreadMessages = 0;
	mNumPendingTotalMessages  = 0;
	NS_NewISupportsArray(getter_AddRefs(mSubFolders));

	mIsCachable = PR_TRUE;

	mListeners = new nsVoidArray();
	
  if (gInstanceCount == 0) {
    kBiffStateAtom           = NS_NewAtom("BiffState");
    kNewMessagesAtom         = NS_NewAtom("NewMessages");
    kNumNewBiffMessagesAtom  = NS_NewAtom("NumNewBiffMessages");
    kNameAtom          = NS_NewAtom("Name");
    kTotalUnreadMessagesAtom = NS_NewAtom("TotalUnreadMessages");
    kTotalMessagesAtom       = NS_NewAtom("TotalMessages");
    kStatusAtom              = NS_NewAtom("Status");
    kFlaggedAtom             = NS_NewAtom("Flagged");
    kSynchronizeAtom         = NS_NewAtom("Synchronize");
    kOpenAtom                = NS_NewAtom("open");

    initializeStrings();

#ifdef MSG_FASTER_URI_PARSING
    mParsingURL = do_CreateInstance(kStandardUrlCID);
#endif
  }
  
  gInstanceCount++;
}

nsMsgFolder::~nsMsgFolder(void)
{
  nsresult rv;
  
	if(mSubFolders)
	{
		PRUint32 count;
    rv = mSubFolders->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

		for (int i = count - 1; i >= 0; i--)
			mSubFolders->RemoveElementAt(i);
	}

    delete mListeners;

	if(mBaseMessageURI)
		nsCRT::free(mBaseMessageURI);

    gInstanceCount--;
    if (gInstanceCount <= 0) {
      NS_IF_RELEASE(kTotalMessagesAtom);
      NS_IF_RELEASE(kBiffStateAtom);
      NS_IF_RELEASE(kNewMessagesAtom);
      NS_IF_RELEASE(kNumNewBiffMessagesAtom);
      NS_IF_RELEASE(kTotalUnreadMessagesAtom);
      NS_IF_RELEASE(kFlaggedAtom);
      NS_IF_RELEASE(kStatusAtom);
      NS_IF_RELEASE(kNameAtom);
      NS_IF_RELEASE(kSynchronizeAtom);
	  NS_IF_RELEASE(kOpenAtom);

      CRTFREEIF(kInboxName);
      CRTFREEIF(kTrashName);
      CRTFREEIF(kSentName);
      CRTFREEIF(kDraftsName);
      CRTFREEIF(kTemplatesName);
      CRTFREEIF(kUnsentName);

#ifdef MSG_FASTER_URI_PARSING
      mParsingURL = nsnull;
#endif
    }
}

NS_IMPL_ISUPPORTS_INHERITED4(nsMsgFolder, nsRDFResource,
                             nsIMsgFolder,
                             nsIFolder,
                             nsISupportsWeakReference,
                             nsISerializable)

nsresult
nsMsgFolder::initializeStrings()
{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(kStringBundleServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                     getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);
    
    bundle->GetStringFromName(NS_LITERAL_STRING("inboxFolderName").get(),
                              &kInboxName);
    bundle->GetStringFromName(NS_LITERAL_STRING("trashFolderName").get(),
                              &kTrashName);
    bundle->GetStringFromName(NS_LITERAL_STRING("sentFolderName").get(),
                              &kSentName);
    bundle->GetStringFromName(NS_LITERAL_STRING("draftsFolderName").get(),
                              &kDraftsName);
    bundle->GetStringFromName(NS_LITERAL_STRING("templatesFolderName").get(),
                              &kTemplatesName);
    bundle->GetStringFromName(NS_LITERAL_STRING("unsentFolderName").get(),
                              &kUnsentName);
    return NS_OK;
}
  
NS_IMETHODIMP
nsMsgFolder::Init(const char* aURI)
{
  // for now, just initialize everything during Init()

  nsresult rv;

  rv = nsRDFResource::Init(aURI);
  if(NS_FAILED(rv))
	  return rv;

  rv = CreateBaseMessageURI(aURI);

  return NS_OK;
}

nsresult nsMsgFolder::CreateBaseMessageURI(const char *aURI)
{
	//Each folder needs to implement this.
	return NS_OK;	
}

NS_IMETHODIMP nsMsgFolder::Shutdown(PRBool shutdownChildren)
{
	return NS_OK;
}

  // nsISerializable methods:
NS_IMETHODIMP
nsMsgFolder::Read(nsIObjectInputStream *aStream)
{
  NS_NOTREACHED("nsMsgFolder::Read");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::Write(nsIObjectOutputStream *aStream)
{
  NS_NOTREACHED("nsMsgFolder::Write");
  return NS_ERROR_NOT_IMPLEMENTED;
}

  // nsICollection methods:
NS_IMETHODIMP
nsMsgFolder::Count(PRUint32 *result) {
  return mSubFolders->Count(result);
}

NS_IMETHODIMP
nsMsgFolder::GetElementAt(PRUint32 i, nsISupports* *result) {
  return mSubFolders->GetElementAt(i, result);
}

NS_IMETHODIMP
nsMsgFolder::QueryElementAt(PRUint32 i, const nsIID & iid, void * *result) {
  return mSubFolders->QueryElementAt(i, iid, result);
}

NS_IMETHODIMP
nsMsgFolder::SetElementAt(PRUint32 i, nsISupports* value) {
  return mSubFolders->SetElementAt(i, value);
}

NS_IMETHODIMP
nsMsgFolder::AppendElement(nsISupports *aElement) {
  return mSubFolders->AppendElement(aElement);
}

NS_IMETHODIMP
nsMsgFolder::RemoveElement(nsISupports *aElement) {
  return mSubFolders->RemoveElement(aElement);
}

NS_IMETHODIMP
nsMsgFolder::Enumerate(nsIEnumerator* *result) {
  // nsMsgFolders only have subfolders, no message elements
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP
nsMsgFolder::Clear(void) {
  return mSubFolders->Clear();
}

NS_IMETHODIMP
nsMsgFolder::GetURI(char* *name) {
  return nsRDFResource::GetValue(name);
}


////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

static nsresult
nsFilterBy(nsISupportsArray* array, nsArrayFilter filter, void* data,
           nsISupportsArray* *result)
{
  nsCOMPtr<nsISupportsArray> f;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(f));
  if (NS_FAILED(rv)) return rv;
  PRUint32 cnt;
  rv = array->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs(array->ElementAt(i));
    if (filter(element, data)) {
      rv = f->AppendElement(element);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  *result = f;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMsgFolder::AddUnique(nsISupports* element)
{
  // XXX fix this
  return mSubFolders->AppendElement(element);
}

// I'm assuming this means "Replace Subfolder"?
NS_IMETHODIMP
nsMsgFolder::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PRBool success=PR_FALSE;
  PRInt32 location = mSubFolders->IndexOf(element);
  if (location>0)
    success = mSubFolders->ReplaceElementAt(newElement, location);
  
  return success ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsMsgFolder::GetSubFolders(nsIEnumerator* *result)
{
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP 
nsMsgFolder::FindSubFolder(const char *subFolderName, nsIFolder **aFolder)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  
	if(NS_FAILED(rv)) 
		return rv;

  // XXX use necko here
	nsCAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(subFolderName);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri.get(), getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;
	if (aFolder)
	{
		*aFolder = folder;
		NS_ADDREF(*aFolder);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsMsgFolder::GetHasSubFolders(PRBool *_retval)
{
  PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  *_retval = (cnt > 0);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::AddFolderListener(nsIFolderListener * listener)
{
	mListeners->AppendElement(listener);
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::RemoveFolderListener(nsIFolderListener * listener)
{

  mListeners->RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::SetParent(nsIFolder *aParent)
{
	mParent = getter_AddRefs(NS_GetWeakReference(aParent));

    if (aParent) {
      nsresult rv;
      nsCOMPtr<nsIMsgFolder> parentMsgFolder =
        do_QueryInterface(aParent, &rv);
      
      if (NS_SUCCEEDED(rv)) {

        // servers do not have parents, so we must not be a server
        mIsServer = PR_FALSE;
        mIsServerIsValid = PR_TRUE;
        
        // also set the server itself while we're here.
        
        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = parentMsgFolder->GetServer(getter_AddRefs(server));
        if (NS_SUCCEEDED(rv) && server)
          mServer = getter_AddRefs(NS_GetWeakReference(server));
      }
  }
  
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetParent(nsIFolder **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  nsCOMPtr<nsIFolder> parent = do_QueryReferent(mParent);
  
	*aParent = parent;
	NS_IF_ADDREF(*aParent);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  // XXX should this return an empty enumeration?
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolder::StartFolderLoading(void)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::EndFolderLoading(void)
{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::UpdateFolder(nsIMsgWindow *)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgFolder::GetFolderURL(char **url)
{
	if(*url)
	{
		*url = NULL;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;	
}


NS_IMETHODIMP nsMsgFolder::GetServer(nsIMsgIncomingServer ** aServer)
{
  NS_ENSURE_ARG_POINTER(aServer);

  nsresult rv;

  // short circut the server if we have it.
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(mServer, &rv);
  if (NS_FAILED(rv) || !server) {
    // try again after parsing the URI
    rv = parseURI(PR_TRUE);
    server = do_QueryReferent(mServer);
  }

  *aServer = server;
  NS_IF_ADDREF(*aServer);
  
	return NS_OK;
}

#ifdef MSG_FASTER_URI_PARSING
class nsMsgAutoBool {
public:
  nsMsgAutoBool() : mValue(nsnull) {}
  void autoReset(PRBool *aValue) { mValue = aValue; }
  ~nsMsgAutoBool() { if (mValue) *mValue = PR_FALSE; }
private:
  PRBool *mValue;
};
#endif

nsresult
nsMsgFolder::parseURI(PRBool needServer)
{
  nsresult rv;
  nsCOMPtr<nsIURL> url;

#ifdef MSG_FASTER_URI_PARSING
  nsMsgAutoBool parsingUrlState;
  if (mParsingURLInUse) {
    url = do_CreateInstance(kStandardUrlCID, &rv);
  }

  else {
    url = mParsingURL;
    mParsingURLInUse = PR_TRUE;
    parsingUrlState.autoReset(&mParsingURLInUse);
  }
  
#else
  rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull,
                                          NS_GET_IID(nsIURL),
                                          (void **)getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;
#endif
  
  rv = url->SetSpec(mURI);
  if (NS_FAILED(rv)) return rv;

  //
  // pull some info out of the URI
  //
  
  // empty path tells us it's a server.
  if (!mIsServerIsValid) {
    nsXPIDLCString path;
    rv = url->GetPath(getter_Copies(path));
    if (NS_SUCCEEDED(rv)) {
      if (!nsCRT::strcmp(path, "/"))
        mIsServer = PR_TRUE;
      else
        mIsServer = PR_FALSE;
    }
    mIsServerIsValid = PR_TRUE;
  }

  // grab the name off the leaf of the server
  if (mName.IsEmpty()) {
    // mName:
    // the name is the trailing directory in the path
    nsXPIDLCString fileName;
    rv = url->GetFileName(getter_Copies(fileName));
    if (NS_SUCCEEDED(rv) && (const char*)fileName != nsnull) {
      // XXX conversion to unicode here? is fileName in UTF8?
		// yes, let's say it is in utf8

      nsXPIDLCString result;
      rv = nsStdUnescape((char*)fileName.get(), getter_Copies(result));
      mName.AssignWithConversion(result);
    }
  }

  // grab the server by parsing the URI and looking it up
  // in the account manager...
  // But avoid this extra work by first asking the parent, if any

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(mServer, &rv);
  if (NS_FAILED(rv) || !server) {
    
    // first try asking the parent instead of the URI
    nsCOMPtr<nsIFolder> parent;
    rv = GetParent(getter_AddRefs(parent));
    
    if (NS_SUCCEEDED(rv) && parent) {
      nsCOMPtr<nsIMsgFolder> parentMsgFolder =
        do_QueryInterface(parent, &rv);

      if (NS_SUCCEEDED(rv))
        rv = parentMsgFolder->GetServer(getter_AddRefs(server));
    }

    // no parent. do the extra work of asking
    if (!server && needServer) {
      // Get username and hostname so we can get the server
      nsXPIDLCString userName;
      rv = url->GetPreHost(getter_Copies(userName));
      if (NS_SUCCEEDED(rv) && (const char*)userName)
        nsUnescape(NS_CONST_CAST(char*,(const char*)userName));
      
      nsXPIDLCString hostName;
      rv = url->GetHost(getter_Copies(hostName));
      if (NS_SUCCEEDED(rv) && (const char*)hostName)
        nsUnescape(NS_CONST_CAST(char*,(const char*)hostName));
      
      // turn it back into a server:
      
      nsCOMPtr<nsIMsgAccountManager> accountManager = 
               do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_alecf
      // this is a failing case, and it would be nice if this
      // was never called
      // (we should ALWAYS handle this case, but try to design
      // the code so we have a parent wherever possible)
      printf("No parent->");
#endif
      rv = accountManager->FindServer(userName,
                                      hostName,
                                      GetIncomingServerType(),
                                      getter_AddRefs(server));
      
      if (NS_FAILED(rv)) return rv;
      
    }

    mServer = getter_AddRefs(NS_GetWeakReference(server));

  } /* !mServer */
    
  // now try to find the local path for this folder
  if (server) {
    
    nsXPIDLCString urlPath;
    url->GetFilePath(getter_Copies(urlPath));

    nsXPIDLCString result;
    rv = nsStdUnescape((char*)urlPath.get(), getter_Copies(result));

    // transform the filepath from the URI, such as
    // "/folder1/folder2/foldern"
    // to
    // "folder1.sbd/folder2.sbd/foldern"
    // (remove leading / and add .sbd to first n-1 folders)
    // to be appended onto the server's path
      
    nsCAutoString newPath;
	NS_MsgCreatePathStringFromFolderURI(result, newPath);

    // now append munged path onto server path
    nsCOMPtr<nsIFileSpec> serverPath;
    rv = server->GetLocalPath(getter_AddRefs(serverPath));
    if (NS_FAILED(rv)) return rv;

    if (serverPath) {
      rv = serverPath->AppendRelativeUnixPath(newPath.get());
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to append to the serverPath");
      if (NS_FAILED(rv)) {
      	mPath = nsnull;
      	return rv;
      }
      mPath = serverPath;
    }

    // URI is completely parsed when we've attempted to get the server
    mHaveParsedURI=PR_TRUE;
  }
    
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetIsServer(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // make sure we've parsed the URI
  if (!mIsServerIsValid) {
    nsresult rv = parseURI();
    if (NS_FAILED(rv) || !mIsServerIsValid)
      return NS_ERROR_FAILURE;
  }
    
  *aResult = mIsServer;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetNoSelect(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetCanSubscribe(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  // by default, you can't subscribe. 
  // if otherwise, override it.
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetCanFileMessages(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
//varada - checking folder flag to see if it is the "Unsent Messages"
//and if so return FALSE
  if (mFlags & MSG_FOLDER_FLAG_QUEUE)
  {
	  *aResult = PR_FALSE;
	  return NS_OK;
  }

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
  
  // by default, you can't file messages into servers, only to folders
  // if otherwise, override it.
  *aResult = !isServer;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetCanCreateSubfolders(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  //Checking folder flag to see if it is the "Unsent Messages"
  //and if so return FALSE
  if (mFlags & MSG_FOLDER_FLAG_QUEUE)
  {
	  *aResult = PR_FALSE;
	  return NS_OK;
  }

  // by default, you can create subfolders on server and folders
  // if otherwise, override it.
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetCanRename(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
  	
  // by default, you can't rename servers, only folders
  // if otherwise, override it.
  if (isServer) {
	*aResult = PR_FALSE;
  }
  //
  // check if the folder is a special folder
  // (Trash, Drafts, Unsent Messages, Inbox, Sent, Templates)
  // if it is, don't allow the user to rename it
  // (which includes dnd moving it with in the same server)
  //
  // this errors on the side of caution.  we'll return false a lot
  // more often if we use flags,  
  // instead of checking if the folder really is being used as a 
  // special folder by looking at the "copies and folders" prefs on the
  // identities.
  //
  // one day...
  else if (mFlags & MSG_FOLDER_FLAG_TRASH ||
      mFlags & MSG_FOLDER_FLAG_DRAFTS ||
      mFlags & MSG_FOLDER_FLAG_QUEUE ||
      mFlags & MSG_FOLDER_FLAG_INBOX ||
      mFlags & MSG_FOLDER_FLAG_SENTMAIL ||
      mFlags & MSG_FOLDER_FLAG_TEMPLATES) {
        *aResult = PR_FALSE;
  }
  else {
	*aResult = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetCanCompact(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);
  *aResult = !isServer;   //servers cannot be compacted --> 4.x
  return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetPrettyName(PRUnichar ** name)
{
  return GetName(name);
}

NS_IMETHODIMP nsMsgFolder::SetPrettyName(const PRUnichar *name)
{
  return SetName(name);
}

NS_IMETHODIMP nsMsgFolder::GetName(PRUnichar **name)
{
  NS_ENSURE_ARG_POINTER(name);

  nsresult rv;
  if (!mHaveParsedURI && mName.IsEmpty()) {
    rv = parseURI();
    if (NS_FAILED(rv)) return rv;
  }
  
  // if it's a server, just forward the call
  if (mIsServer) {
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server)
      return server->GetPrettyName(name);
  }

  *name = ToNewUnicode(mName);
  
  if (!(*name)) return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetName(const PRUnichar * name)
{
  // override the URI-generated name
	if (!mName.EqualsWithConversion(name))
	{
	  mName = name;
  
  // old/new value doesn't matter here
	  NotifyUnicharPropertyChanged(kNameAtom, name, name);
	}
	return NS_OK;
}

//For default, just return name
NS_IMETHODIMP nsMsgFolder::GetAbbreviatedName(PRUnichar * *aAbbreviatedName)
{
	return GetName(aAbbreviatedName);
}

NS_IMETHODIMP nsMsgFolder::GetChildNamed(const char *name, nsISupports ** aChild)
{
	NS_ASSERTION(aChild, "NULL child");
	nsresult rv;
	// will return nsnull if we can't find it
	*aChild = nsnull;

	nsCOMPtr<nsIMsgFolder> folder;

	PRUint32 count;
	rv = mSubFolders->Count(&count);
	if (NS_FAILED(rv)) return rv;

	nsString uniName;
	ConvertToUnicode(nsMsgI18NFileSystemCharset(), name, uniName);

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			nsXPIDLString folderName;

			rv = folder->GetName(getter_Copies(folderName));
			// case-insensitive compare is probably LCD across OS filesystems
			if (NS_SUCCEEDED(rv) && Compare(folderName, uniName,
                                      nsCaseInsensitiveStringComparator()) == 0)
			{
				*aChild = folder;
				NS_ADDREF(*aChild);
				return NS_OK;
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetChildWithURI(const char *uri, PRBool deep, PRBool caseInsensitive, nsIMsgFolder ** child)
{
	NS_ASSERTION(child, "NULL child");
	nsresult rv;
	// will return nsnull if we can't find it
	*child = nsnull;

	nsCOMPtr <nsIEnumerator> aEnumerator;

	rv = GetSubFolders(getter_AddRefs(aEnumerator));
	if(NS_FAILED(rv)) 
		return rv;

	nsCOMPtr<nsISupports> aItem;

	rv = aEnumerator->First();
	if (!NS_SUCCEEDED(rv))
		return NS_OK;	// it's OK, there are no sub-folders.

	while(NS_SUCCEEDED(rv))
	{
		rv = aEnumerator->CurrentItem(getter_AddRefs(aItem));
		if (NS_FAILED(rv)) break;
		nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(aItem);
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(aItem);
		if(folderResource  && folder)
		{
			const char *folderURI;
			rv = folderResource->GetValueConst(&folderURI);
			if(NS_FAILED(rv)) return rv;
            PRBool equal;
            if (caseInsensitive) 
              equal = (folderURI && nsCRT::strcasecmp(folderURI, uri)==0);  
            else
              equal = (folderURI && nsCRT::strcmp(folderURI, uri)==0);
			if (equal)
			{
				*child = folder;
				NS_ADDREF(*child);
				return NS_OK;
			}
			else if(deep)
			{
				rv = folder->GetChildWithURI(uri, deep, caseInsensitive, child);
				if(NS_FAILED(rv))
					return rv;

				if(*child)
					return NS_OK;
			}
		}
		rv = aEnumerator->Next();
		if (!NS_SUCCEEDED(rv))
		{
			rv = NS_OK;
			break;
		}
	}

  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetPrettiestName(PRUnichar **name)
{
  if (NS_SUCCEEDED(GetPrettyName(name)))
    return NS_OK;
  return GetName(name);
}

static PRBool
nsCanBeInFolderPane(nsISupports* element, void* data)
{
#ifdef HAVE_PANE
  nsIMsgFolder* subFolder = NS_STATIC_CAST(nsIMsgFolder*, element);
  return subFolder->CanBeInFolderPane(); 
#else
  return PR_TRUE;
#endif
}

NS_IMETHODIMP
nsMsgFolder::GetVisibleSubFolders(nsIEnumerator* *result) 
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> vFolders;
  rv = nsFilterBy(mSubFolders, nsCanBeInFolderPane, nsnull, getter_AddRefs(vFolders));
  if (NS_FAILED(rv)) return rv;
  rv = vFolders->Enumerate(result);
  return rv;
}

#ifdef HAVE_ADMINURL
NS_IMETHODIMP nsMsgFolder::GetAdminUrl(MWContext *context, MSG_AdminURLType type)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::HaveAdminUrl(MSG_AdminURLType type, PRBool *haveAdminUrl)
{
	if(haveAdminUrl)
	{
		*haveAdminUrl = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}
#endif

NS_IMETHODIMP nsMsgFolder::GetDeleteIsMoveToTrash(PRBool *deleteIsMoveToTrash)
{
	if(deleteIsMoveToTrash)
	{
		*deleteIsMoveToTrash = mDeleteIsMoveToTrash;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetShowDeletedMessages(PRBool *showDeletedMessages)
{
	if(showDeletedMessages)
	{
		*showDeletedMessages = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgFolder::ForceDBClosed ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::Delete ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::DeleteSubFolders(nsISupportsArray *folders,
                                            nsIMsgWindow *msgWindow)
{
	nsresult rv;

	PRUint32 count;
	rv = folders->Count(&count);
	nsCOMPtr<nsIMsgFolder> folder;
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(folders->ElementAt(i));
		folder = do_QueryInterface(supports);
		if(folder)	
			PropagateDelete(folder, PR_TRUE, msgWindow);
	}
	return rv;

}

NS_IMETHODIMP nsMsgFolder::CreateStorageIfMissing(nsIUrlListener* /* urlListener */)
{
  NS_ASSERTION(PR_FALSE, "needs to be overridden");
	nsresult status = NS_OK;

  return status;
}


NS_IMETHODIMP nsMsgFolder::PropagateDelete(nsIMsgFolder *folder, PRBool deleteStorage, nsIMsgWindow *msgWindow)
{
	nsresult status = NS_OK;

	nsCOMPtr<nsIMsgFolder> child;

	// first, find the folder we're looking to delete
	PRUint32 cnt;
	nsresult rv = mSubFolders->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	for (PRUint32 i = 0; i < cnt; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		child = do_QueryInterface(supports, &status);
		if(NS_SUCCEEDED(status))
		{
			if (folder == child.get())
			{
        //Remove self as parent
        child->SetParent(nsnull);
				// maybe delete disk storage for it, and its subfolders
				status = child->RecursiveDelete(deleteStorage, msgWindow);	

				if (status == NS_OK) 
				{

					//Remove from list of subfolders.
					mSubFolders->RemoveElement(supports);
					nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
					nsCOMPtr<nsISupports> folderSupports;
					rv = QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(folderSupports));
					if(childSupports && NS_SUCCEEDED(rv))
						NotifyItemDeleted(folderSupports, childSupports, "folderView");
					break;
				}
        else
          {  // setting parent back if we failed
            child->SetParent(this);
          }
			}
			else
			{
				status = child->PropagateDelete (folder, deleteStorage, msgWindow);
			}
		}
	}
	
	return status;
}

NS_IMETHODIMP nsMsgFolder::RecursiveDelete(PRBool deleteStorage, nsIMsgWindow *msgWindow)
{
	// If deleteStorage is PR_TRUE, recursively deletes disk storage for this folder
	// and all its subfolders.
	// Regardless of deleteStorage, always unlinks them from the children lists and
	// frees memory for the subfolders but NOT for _this_

	nsresult status = NS_OK;
	
	PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  while (cnt > 0)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(0));
		nsCOMPtr<nsIMsgFolder> child(do_QueryInterface(supports, &status));

		if(NS_SUCCEEDED(status))
		{
			child->SetParent(nsnull);
			status = child->RecursiveDelete(deleteStorage,msgWindow);  // recur
      if (NS_SUCCEEDED(status))
        {
          mSubFolders->RemoveElement(supports);  // unlink it from this's child list
          nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
          nsCOMPtr<nsISupports> folderSupports;
          rv = QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(folderSupports));
          if(childSupports && NS_SUCCEEDED(rv))
            NotifyItemDeleted(folderSupports, childSupports, "folderView");
        }
      else
        { // setting parent back if we failed for some reason
          child->SetParent(this);
        }
		}
		cnt--;
	}

	// now delete the disk storage for _this_
    if (deleteStorage && (status == NS_OK))
        status = Delete();
	return status;
}

NS_IMETHODIMP nsMsgFolder::CreateSubfolder(const PRUnichar *folderName, nsIMsgWindow *msgWindow )
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgFolder::AddSubfolder(nsAutoString *folderName,
                                   nsIMsgFolder** newFolder)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aCompactOfflineArray)
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::Rename(const PRUnichar *name, nsIMsgWindow *msgWindow)
{
    nsresult status = NS_OK;
	nsAutoString unicharString(name);
	status = SetName((PRUnichar *) unicharString.get());
	//After doing a SetName we need to make sure that broadcasting this message causes a
	//new sort to happen.
#ifdef HAVE_MASTER
	if (m_master)
		m_master->BroadcastFolderChanged(this);
#endif
	return status;

}

NS_IMETHODIMP nsMsgFolder::RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::ContainsChildNamed(const char *name, PRBool* containsChild)
{
	nsCOMPtr<nsISupports> child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(GetChildNamed(name, getter_AddRefs(child))))
		{
			*containsChild = child != nsnull;
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}



NS_IMETHODIMP nsMsgFolder::IsAncestorOf(nsIMsgFolder *child, PRBool *isAncestor)
{
	if(!isAncestor)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 count;
  rv = mSubFolders->Count(&count);
  if (NS_FAILED(rv)) return rv;

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (folder.get() == child )
			{
				*isAncestor = PR_TRUE;
			}
			else
				folder->IsAncestorOf(child, isAncestor);

		}
		if(*isAncestor)
			return NS_OK;
    }
	*isAncestor = PR_FALSE;
	return rv;

}


NS_IMETHODIMP nsMsgFolder::GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder,
                                                       char **name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	/* only try 256 times */
	for (int count = 0; (count < 256); count++)
	{
		PRUint32 prefixSize = PL_strlen(prefix);

		//allocate string big enough for prefix, 256, and '\0'
		char *uniqueName = (char*)PR_MALLOC(prefixSize + 4);
		if(!uniqueName)
			return NS_ERROR_OUT_OF_MEMORY;
		PR_snprintf(uniqueName, prefixSize + 4, "%s%d",prefix,count);
		PRBool containsChild;
		PRBool otherContainsChild = PR_FALSE;

		ContainsChildNamed(uniqueName, &containsChild);
		if(otherFolder)
		{
			((nsIMsgFolder*)otherFolder)->ContainsChildNamed(uniqueName, &otherContainsChild);
		}

		if (!containsChild && !otherContainsChild)
		{
			*name = uniqueName;
			return NS_OK;
		}
		else
			PR_FREEIF(uniqueName);
	}
	*name = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UpdateSummaryTotals(PRBool /* force */)
{
	//We don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SummaryChanged()
{
	UpdateSummaryTotals(PR_FALSE);
#ifdef HAVE_MASTER
    if (mMaster)
        mMaster->BroadcastFolderChanged(this);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumUnread(PRBool deep, PRInt32 *numUnread)
{
	if(!numUnread)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRUint32 total = mNumUnreadMessages + mNumPendingUnreadMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) {

      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
        folder = do_QueryInterface(supports, &rv);
        if(NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          folder->GetNumUnread(deep, &num);
          if (num >= 0) // it's legal for counts to be negative if we don't know
            total += num;
        }
      }
    }
	}
  *numUnread = total;
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::GetTotalMessages(PRBool deep, PRInt32 *totalMessages)
{
	if(!totalMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRInt32 total = mNumTotalMessages + mNumPendingTotalMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) {

      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
        folder = do_QueryInterface(supports, &rv);
        if(NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          folder->GetTotalMessages (deep, &num);
          if (num >= 0) // it's legal for counts to be negative if we don't know
            total += num;
        }
			}
		}
	}
  *totalMessages = total;
  return NS_OK;
}

PRInt32 nsMsgFolder::GetNumPendingUnread() 
{
	return mNumPendingUnreadMessages;
}

PRInt32 nsMsgFolder::GetNumPendingTotalMessages() 
{
	return mNumPendingTotalMessages;
}

NS_IMETHODIMP nsMsgFolder::GetHasNewMessages(PRBool *hasNewMessages)
{
	//we don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetHasNewMessages(PRBool hasNewMessages)
{
	//we don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFirstNewMessage(nsIMsgDBHdr **firstNewMessage)
{
	//we don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearNewMessages()
{
	//we don't support this
	return NS_OK;
}
	
void nsMsgFolder::ChangeNumPendingUnread(PRInt32 delta)
{
	if(delta)
	{
		PRInt32 oldUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
		mNumPendingUnreadMessages += delta;
		PRInt32 newUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
    nsCOMPtr<nsIMsgDatabase> db;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
    if (NS_SUCCEEDED(rv) && folderInfo)
      folderInfo->SetImapUnreadPendingMessages(mNumPendingUnreadMessages);

		NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, newUnreadMessages);
	}
}

void nsMsgFolder::ChangeNumPendingTotalMessages(PRInt32 delta)
{
	if(delta)
	{
		PRInt32 oldTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;
		mNumPendingTotalMessages += delta;
		PRInt32 newTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;

    nsCOMPtr<nsIMsgDatabase> db;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
    if (NS_SUCCEEDED(rv) && folderInfo)
      folderInfo->SetImapTotalPendingMessages(mNumPendingTotalMessages);
		NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, newTotalMessages);
	}

}


NS_IMETHODIMP nsMsgFolder::SetPrefFlag()
{
  // *** Note: this method should only be called when we done with the folder
  // discovery. GetResource() may return a node which is not in the folder
  // tree hierarchy but in the rdf cache in case of the non-existing default
  // Sent, Drafts, and Templates folders. The resouce will be eventually
  // released when rdf service shutting downs. When we create the defaul
  // folders later on on the imap server, the subsequent GetResouce() of the
  // same uri will get us the cached rdf resouce which should have the folder
  // flag set appropriately.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  
	if(NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupportsArray> identities;
  rv = accountManager->GetIdentitiesForServer(server,
                                              getter_AddRefs(identities));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgIdentity> identity;

  rv = identities->QueryElementAt(0, NS_GET_IID(nsIMsgIdentity),
                                  (void **)getter_AddRefs(identity));

  if (NS_SUCCEEDED(rv) && identity)
  {
    nsXPIDLCString folderUri;
    nsCOMPtr<nsIRDFResource> res;
    nsCOMPtr<nsIMsgFolder> folder;
    identity->GetFccFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
    }
    identity->GetDraftFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
    }
    identity->GetStationeryFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgFolder::SetFlag(PRUint32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (!flagSet)
	{
		mFlags |= flag;
		OnFlagChange(flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearFlag(PRUint32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (flagSet)
	{
		mFlags &= ~flag;
		OnFlagChange (flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlag(PRUint32 flag, PRBool *_retval)
{
	*_retval = ((mFlags & flag) != 0);
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ToggleFlag(PRUint32 flag)
{
  mFlags ^= flag;
	OnFlagChange (flag);

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::OnFlagChange(PRUint32 flag)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgDatabase> db;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && folderInfo)
  {
#ifdef DEBUG_bienvenu
      nsXPIDLString name;
      rv = GetName(getter_Copies(name));
      NS_ASSERTION(nsCRT::strcmp(name, "Trash") || (mFlags & MSG_FOLDER_FLAG_TRASH), "lost trash flag");
#endif
      folderInfo->SetFlags((PRInt32) mFlags);
      if (db)
        db->Commit(nsMsgDBCommitType::kLargeCommit);
     
      if (flag & MSG_FOLDER_FLAG_OFFLINE) {
        PRBool newValue = mFlags & MSG_FOLDER_FLAG_OFFLINE;
        rv = NotifyBoolPropertyChanged(kSynchronizeAtom, !newValue, newValue);
        NS_ENSURE_SUCCESS(rv,rv);
      }
	  else if (flag & MSG_FOLDER_FLAG_ELIDED) {
        PRBool newValue = mFlags & MSG_FOLDER_FLAG_ELIDED;
        rv = NotifyBoolPropertyChanged(kOpenAtom, newValue, !newValue);
        NS_ENSURE_SUCCESS(rv,rv);
      }
  }
  folderInfo = nsnull;
  return rv;
}

NS_IMETHODIMP nsMsgFolder::SetFlags(PRUint32 aFlags)
{
  if (mFlags != aFlags)
  {
    mFlags = aFlags;
	OnFlagChange(mFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFoldersWithFlag(PRUint32 flags, PRUint32 resultsize, PRUint32 *numFolders, nsIMsgFolder **result)
{
	PRUint32 num = 0;
	if ((flags & mFlags) == flags) {
		if (result && (num < resultsize)) {
			result[num] = this;
			NS_IF_ADDREF(result[num]);
		}
		num++;
	}

	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder;
	PRUint32 cnt;

        // call GetSubFolders() to ensure that mSubFolders is initialized
        nsCOMPtr <nsIEnumerator> enumerator;
        rv = GetSubFolders(getter_AddRefs(enumerator));
        NS_ENSURE_SUCCESS(rv,rv);
	
  rv = mSubFolders->Count(&cnt);
  if (NS_SUCCEEDED(rv)) {
    for (PRUint32 i=0; i < cnt; i++) 
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
      if(NS_SUCCEEDED(rv) && folder)
      {
        // CAREFUL! if NULL is passed in for result then the caller
        // still wants the full count!  Otherwise, the result should be at most the
        // number that the caller asked for.
        PRUint32 numSubFolders;

        if (!result)
        {
          folder->GetFoldersWithFlag(flags, 0, &numSubFolders, NULL);
          num += numSubFolders;
        }
        else if (num < resultsize)
        {
          folder->GetFoldersWithFlag(flags, resultsize - num, &numSubFolders, result+num);
          num += numSubFolders;
        }
        else
        {
          break;
        }
      }
    }
  }
	*numFolders = num;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetExpansionArray(nsISupportsArray *expansionArray)
{
  // the application of flags in GetExpansionArray is subtly different
  // than in GetFoldersWithFlag 

	nsresult rv;
	PRUint32 cnt;
  rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			PRUint32 cnt2;
      rv = expansionArray->Count(&cnt2);
      if (NS_SUCCEEDED(rv)) {
        expansionArray->InsertElementAt(folder, cnt2);
        PRUint32 flags;
        folder->GetFlags(&flags);
        if (!(flags & MSG_FOLDER_FLAG_ELIDED))
          folder->GetExpansionArray(expansionArray);
      }
    }
  }

	return NS_OK;
}


#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::SetFlagInAllFolderPanes(PRUInt32 which)
{

}

#endif

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::EscapeMessageId(const char *messageId, const char **escapeMessageID)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetExpungedBytes(PRUint32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

  *count = 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetDeletable(PRBool *deletable)
{

	if(!deletable)
		return NS_ERROR_NULL_POINTER;

	*deletable = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRequiresCleanup(PRBool *requiredCleanup)
{
	if(!requiredCleanup)
		return NS_ERROR_NULL_POINTER;

	*requiredCleanup = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearRequiresCleanup()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ManyHeadersToDownload(PRBool *_retval)
{
	if (!_retval)
		return NS_ERROR_NULL_POINTER;
	*_retval = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetKnowsSearchNntpExtension(PRBool *knowsExtension)
{
	if(!knowsExtension)
		return NS_ERROR_NULL_POINTER;

	*knowsExtension = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetAllowsPosting(PRBool *allowsPosting)
{
	if(!allowsPosting)
		return NS_ERROR_NULL_POINTER;

	*allowsPosting = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetDisplayRecipients(PRBool *displayRecipients)
{
	nsresult rv;

	*displayRecipients = PR_FALSE;

	if (mFlags & MSG_FOLDER_FLAG_SENTMAIL && !(mFlags & MSG_FOLDER_FLAG_INBOX))
		*displayRecipients = PR_TRUE;
	else if (mFlags & MSG_FOLDER_FLAG_QUEUE)
		*displayRecipients = PR_TRUE;
	else 
	{
		// Only mail folders can be FCC folders
		if (mFlags & MSG_FOLDER_FLAG_MAIL || mFlags & MSG_FOLDER_FLAG_IMAPBOX) 
		{
			// There's one FCC folder for sent mail, and one for sent news
			nsIMsgFolder *fccFolders[2]; 
			int numFccFolders = 0;
#ifdef HAVE_MASTER
			m_master->GetFolderTree()->GetFoldersWithFlag (MSG_FOLDER_FLAG_SENTMAIL, fccFolders, 2, &numFccFolders);
#endif
			for (int i = 0; i < numFccFolders; i++)
			{
				PRBool isAncestor;
				if(NS_SUCCEEDED(rv = fccFolders[i]->IsAncestorOf(this, &isAncestor)))
				{
					if (isAncestor)
						*displayRecipients = PR_TRUE;
				}
				NS_RELEASE(fccFolders[i]);
			}
		}
	}
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::AcquireSemaphore(nsISupports *semHolder)
{
	nsresult rv = NS_OK;

	if (mSemaphoreHolder == NULL)
	{
		mSemaphoreHolder = semHolder;	//Don't AddRef due to ownership issues.
	}
	else
		rv = NS_MSG_FOLDER_BUSY;

	return rv;

}

NS_IMETHODIMP nsMsgFolder::ReleaseSemaphore(nsISupports *semHolder)
{
	if (!mSemaphoreHolder || mSemaphoreHolder == semHolder)
	{
		mSemaphoreHolder = NULL;
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::TestSemaphore(nsISupports *semHolder, PRBool *result)
{
	if(!result)
		return NS_ERROR_NULL_POINTER;

	*result = (mSemaphoreHolder == semHolder);

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetLocked(PRBool *isLocked)
{ 
	*isLocked =  mSemaphoreHolder != NULL;
	return  NS_OK;
}

#ifdef HAVE_PANE
    MWContext *GetFolderPaneContext();
#endif

#ifdef HAVE_MASTER
    MSG_Master  *GetMaster() {return m_master;}
#endif

NS_IMETHODIMP nsMsgFolder::GetRelativePathName(char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;

	*pathName = nsnull;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetSizeOnDisk(PRUint32 *size)
{
	if(!size)
		return NS_ERROR_NULL_POINTER;

	*size = 0;
	return NS_OK;

}

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::ShouldPerformOperationOffline(PRBool *performOffline)
{

}
#endif


#ifdef DOES_FOLDEROPERATIONS
NS_IMETHODIMP nsMsgFolder::DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo,
                                                       nsMsgKeyArray &keysToSave,
                                                       MSG_FolderInfo *dstFolder,
                                                       nsMsgDatabase *sourceDB)
{

}

NS_IMETHODIMP nsMsgFolder::UpdateMoveCopyStatus(MWContext *context, PRBool isMove, int32 curMsgCount, int32 totMessages)
{

}

#endif

NS_IMETHODIMP nsMsgFolder::RememberPassword(const char *password)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRememberedPassword(char ** password)
{

	if(!password)
		return NS_ERROR_NULL_POINTER;

	*password = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *needsAuthenticate)
{
	if(!needsAuthenticate)
		return NS_ERROR_NULL_POINTER;

	*needsAuthenticate = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetUsername(char **userName)
{
  NS_ENSURE_ARG_POINTER(userName);
  nsresult rv;
  nsCOMPtr <nsIMsgIncomingServer> server;
  
  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  if (server)
    return server->GetUsername(userName);

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsMsgFolder::GetHostname(char **hostName)
{
  NS_ENSURE_ARG_POINTER(hostName);

  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  if (server)
    return server->GetHostName(hostName);
  
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsMsgFolder::GetNewMessages(nsIMsgWindow *, nsIUrlListener * /* aListener */)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::GetBiffState(PRUint32 *aBiffState)
{
	if(!aBiffState)
		return NS_ERROR_NULL_POINTER;
	*aBiffState = mBiffState;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetBiffState(PRUint32 aBiffState)
{
	nsresult rv;

	// this optimization isn't working with filters...
	// so commenting out the optimization
	// need to figure out why and fix it !!!

	//if(mBiffState != aBiffState) 
	//{
		PRUint32 oldBiffState = mBiffState;
		mBiffState = aBiffState;
		nsCOMPtr<nsISupports> supports;

		// Get the server and notify it and not inbox.
		nsCOMPtr<nsIMsgIncomingServer> aServer;
		nsCOMPtr<nsIFolder> aRootFolder;

		rv = GetServer(getter_AddRefs(aServer));
		rv = aServer->GetRootFolder(getter_AddRefs(aRootFolder));
        if(NS_FAILED(rv))
            return rv;
		
		if(NS_SUCCEEDED(aRootFolder->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports))))
			(aRootFolder)->NotifyPropertyFlagChanged(supports, kBiffStateAtom, oldBiffState, mBiffState);
	//}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumNewMessages(PRInt32 *aNumNewMessages)
{
	if(!aNumNewMessages)
		return NS_ERROR_NULL_POINTER;

	*aNumNewMessages = mNumNewBiffMessages;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetNumNewMessages(PRInt32 aNumNewMessages)
{
	if(aNumNewMessages != mNumNewBiffMessages)
	{
		PRInt32 oldNumMessages = mNumNewBiffMessages;
		mNumNewBiffMessages = aNumNewMessages;

    nsCAutoString oldNumMessagesStr;
    oldNumMessagesStr.AppendInt(oldNumMessages);
		nsCAutoString newNumMessagesStr;
    newNumMessagesStr.AppendInt(aNumNewMessages);
		NotifyPropertyChanged(kNumNewBiffMessagesAtom, oldNumMessagesStr.get(), newNumMessagesStr.get());
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNewMessagesNotificationDescription(PRUnichar * *aDescription)
{
	nsresult rv;
	nsAutoString description;
	nsCOMPtr<nsIMsgIncomingServer> server;
	rv = GetServer(getter_AddRefs(server));
  
	if(NS_SUCCEEDED(rv))
	{
		if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
		{
			nsXPIDLString folderName;
			rv = GetPrettyName(getter_Copies(folderName));
			if (NS_SUCCEEDED(rv) && folderName)
				description.Assign(folderName);
		}

    // append the server name
    nsXPIDLString serverName;
		rv = server->GetPrettyName(getter_Copies(serverName));
		if(NS_SUCCEEDED(rv)) {
      // put this test here because we don't want to just put "folder name on"
      // in case the above failed
      if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
        description.Append(NS_LITERAL_STRING(" on "));
			description.Append(serverName);
    }
	}
	*aDescription = ToNewUnicode(description);
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRootFolder(nsIMsgFolder * *aRootFolder)
{
	if (!aRootFolder) return NS_ERROR_NULL_POINTER;

	nsresult rv;
	nsCOMPtr<nsIMsgIncomingServer> server;
	rv = GetServer(getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;
	NS_ASSERTION(server, "server is null");
	// if this happens, bail.
	if (!server) return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIFolder> aRoot;
	rv = server->GetRootFolder(getter_AddRefs(aRoot));

	if (NS_FAILED(rv)) return rv;
	if (!aRoot) return NS_ERROR_NULL_POINTER;

	return aRoot->QueryInterface(NS_GET_IID(nsIMsgFolder), (void**)aRootFolder);
}

NS_IMETHODIMP
nsMsgFolder::GetMsgDatabase(nsIMsgWindow *aMsgWindow,
                            nsIMsgDatabase** aMsgDatabase)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::GetPath(nsIFileSpec * *aPath)
{
  NS_ENSURE_ARG_POINTER(aPath);
  nsresult rv=NS_OK;

  if (!mPath)
    rv = parseURI(PR_TRUE);
  
  *aPath = mPath;
  NS_IF_ADDREF(*aPath);
  
  return rv;
}

NS_IMETHODIMP
nsMsgFolder::SetPath(nsIFileSpec  *aPath)
{
  // XXX - make a local copy!
  mPath = aPath;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::MarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
	PRUint32 count;
	nsresult rv;

	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;

	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
		nsCOMPtr<nsIMsgDBHdr> message = do_QueryInterface(msgSupports);

		if(message)
			rv = message->MarkRead(markRead);

		if(NS_FAILED(rv))
			return rv;

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged)
{
	PRUint32 count;
	nsresult rv;

	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;

	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
		nsCOMPtr<nsIMsgDBHdr> message = do_QueryInterface(msgSupports);

		if(message)
			rv = message->MarkFlagged(markFlagged);

		if(NS_FAILED(rv))
			return rv;

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag)
{
  // most folders don't do anything for this...
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::MarkAllMessagesRead(void)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::MarkThreadRead(nsIMsgThread *thread)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMsgFolder::CopyMessages(nsIMsgFolder* srcFolder,
                          nsISupportsArray *messages,
                          PRBool isMove,
                          nsIMsgWindow *window,
                          nsIMsgCopyServiceListener* listener,
						  PRBool isFolder,
                          PRBool allowUndo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::CopyFolder(nsIMsgFolder* srcFolder,
                          PRBool isMoveFolder,
                          nsIMsgWindow *window,
                          nsIMsgCopyServiceListener* listener)
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::CopyFileMessage(nsIFileSpec* fileSpec,
                             nsIMsgDBHdr* messageToReplace,
                             PRBool isDraftOrTemplate,
                             nsIMsgWindow *window,
                             nsIMsgCopyServiceListener* listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::MatchName(nsString *name, PRBool *matches)
{
	if (!matches)
		return NS_ERROR_NULL_POINTER;
	*matches = mName.EqualsIgnoreCase(*name);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::SetDeleteIsMoveToTrash(PRBool bVal)
{
  mDeleteIsMoveToTrash = bVal;
  return NS_OK;
}

nsresult
nsMsgFolder::NotifyPropertyChanged(nsIAtom *property,
                                   const char *oldValue, const char* newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports))))
	{
		PRInt32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			//Folderlistener's aren't refcounted.
			nsIFolderListener* listener =(nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemPropertyChanged(supports, property, oldValue, newValue);
		}

		//Notify listeners who listen to every folder
		nsresult rv;
		nsCOMPtr<nsIFolderListener> folderListenerManager = 
		         do_GetService(kMsgFolderListenerManagerCID, &rv); 
		if(NS_SUCCEEDED(rv))
			folderListenerManager->OnItemPropertyChanged(supports, property, oldValue, newValue);

	}

	return NS_OK;

}

nsresult
nsMsgFolder::NotifyUnicharPropertyChanged(nsIAtom *property,
                                          const PRUnichar* oldValue,
                                          const PRUnichar *newValue)
{
  nsresult rv;
  nsCOMPtr<nsISupports> supports;
  rv = QueryInterface(NS_GET_IID(nsISupports),
                      (void **)getter_AddRefs(supports));
  if (NS_FAILED(rv)) return rv;
  
  PRInt32 i;
  for (i=0; i<mListeners->Count(); i++) {
    // folderlisteners arent refcounted in the array
    nsIFolderListener* listener=(nsIFolderListener*)mListeners->ElementAt(i);
    listener->OnItemUnicharPropertyChanged(supports, property, oldValue, newValue);
  }

  // Notify listeners who listen to every folder
  nsCOMPtr<nsIFolderListener> folderListenerManager = 
           do_GetService(kMsgFolderListenerManagerCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = folderListenerManager->OnItemUnicharPropertyChanged(supports,
                                                   property,
                                                   oldValue,
                                                   newValue);
  return NS_OK;
}

nsresult nsMsgFolder::NotifyIntPropertyChanged(nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
	//Don't send off count notifications if they are turned off.
	if(!mNotifyCountChanges && ((property == kTotalMessagesAtom) ||( property ==  kTotalUnreadMessagesAtom)))
		return NS_OK;

	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports))))
	{
		PRInt32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			//Folderlistener's aren't refcounted.
			nsIFolderListener* listener =(nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemIntPropertyChanged(supports, property, oldValue, newValue);
		}

		//Notify listeners who listen to every folder
		nsresult rv;
		nsCOMPtr<nsIFolderListener> folderListenerManager = 
		         do_GetService(kMsgFolderListenerManagerCID, &rv); 
		if(NS_SUCCEEDED(rv))
			folderListenerManager->OnItemIntPropertyChanged(supports, property, oldValue, newValue);

	}

	return NS_OK;

}

nsresult
nsMsgFolder::NotifyBoolPropertyChanged(nsIAtom* property,
                                       PRBool oldValue, PRBool newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports))))
	{
		PRInt32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			//Folderlistener's aren't refcounted.
			nsIFolderListener* listener =(nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemBoolPropertyChanged(supports, property, oldValue, newValue);
		}

		//Notify listeners who listen to every folder
		nsresult rv;
		nsCOMPtr<nsIFolderListener> folderListenerManager = 
		         do_GetService(kMsgFolderListenerManagerCID, &rv); 
		if(NS_SUCCEEDED(rv))
			folderListenerManager->OnItemBoolPropertyChanged(supports, property, oldValue, newValue);

	}

	return NS_OK;

}

nsresult
nsMsgFolder::NotifyPropertyFlagChanged(nsISupports *item, nsIAtom *property,
                                       PRUint32 oldValue, PRUint32 newValue)
{
	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
	}

	//Notify listeners who listen to every folder
	nsresult rv;
	nsCOMPtr<nsIFolderListener> folderListenerManager = 
	         do_GetService(kMsgFolderListenerManagerCID, &rv); 
	if(NS_SUCCEEDED(rv))
		folderListenerManager->OnItemPropertyFlagChanged(item, property, oldValue, newValue);

	return NS_OK;
}

nsresult nsMsgFolder::NotifyItemAdded(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	static PRBool notify = PR_TRUE;

	if (!notify)
		return NS_OK;

	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemAdded(parentItem, item, viewString);
	}

	//Notify listeners who listen to every folder
	nsresult rv;
	nsCOMPtr<nsIFolderListener> folderListenerManager = 
	         do_GetService(kMsgFolderListenerManagerCID, &rv); 
	if(NS_SUCCEEDED(rv))
		folderListenerManager->OnItemAdded(parentItem, item, viewString);

	return NS_OK;

}

nsresult nsMsgFolder::NotifyItemDeleted(nsISupports *parentItem, nsISupports *item, const char* viewString)
{

	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemRemoved(parentItem, item, viewString);
	}
	//Notify listeners who listen to every folder
	nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager = 
           do_GetService(kMsgFolderListenerManagerCID, &rv); 
	if(NS_SUCCEEDED(rv))
		folderListenerManager->OnItemRemoved(parentItem, item, viewString);

	return NS_OK;

}


nsresult nsMsgFolder::NotifyFolderEvent(nsIAtom* aEvent)
{
	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemEvent(this, aEvent);
	}
	//Notify listeners who listen to every folder
	nsresult rv;
	nsCOMPtr<nsIFolderListener> folderListenerManager = 
	         do_GetService(kMsgFolderListenerManagerCID, &rv); 
	if(NS_SUCCEEDED(rv))
		folderListenerManager->OnItemEvent(this, aEvent);

	return NS_OK;
}

nsresult
nsGetMailFolderSeparator(nsString& result)
{
  result.Assign(NS_LITERAL_STRING(".sbd"));
  return NS_OK;
}


NS_IMETHODIMP
nsMsgFolder::RecursiveSetDeleteIsMoveToTrash(PRBool bVal)
{
  nsresult rv = NS_OK;
  if (mSubFolders)
  {
    PRUint32 cnt = 0, i;
    rv = mSubFolders->Count(&cnt);
    for (i=0; i < cnt; i++)
    {
      nsCOMPtr<nsISupports> aSupport;
      rv = GetElementAt(i, getter_AddRefs(aSupport));
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIMsgFolder> folder;
        folder = do_QueryInterface(aSupport);
        if (folder)
          folder->RecursiveSetDeleteIsMoveToTrash(bVal);
      }
    }
  }
  return SetDeleteIsMoveToTrash(bVal);
}

NS_IMETHODIMP
nsMsgFolder::GetFilterList(nsIMsgFilterList **aResult)
{
  nsresult rv;

  // only get filters for root folders in the base class
#ifdef NS_DEBUG
  PRBool isServer=PR_FALSE;
  rv = GetIsServer(&isServer);
  NS_ASSERTION(NS_SUCCEEDED(rv), "error getting serverness");
  NS_ASSERTION(isServer, "Getting filter for non-server?");
#endif

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(server, NS_ERROR_FAILURE);

  return server->GetFilterList(aResult);
}

/* void enableNotifications (in long notificationType, in boolean enable); */
NS_IMETHODIMP nsMsgFolder::EnableNotifications(PRInt32 notificationType, PRBool enable)
{
  if(notificationType == nsIMsgFolder::allMessageCountNotifications)
  {
    mNotifyCountChanges = enable;
    // start and stop db batching here. This is under the theory
    // that any time we want to enable and disable notifications,
    // we're probably doing something that should be batched.
    nsCOMPtr <nsIMsgDatabase> database;
    
    
    GetMsgDatabase(nsnull, getter_AddRefs(database));
    if(enable)
    {
      if (database)
        database->EndBatch();
      UpdateSummaryTotals(PR_TRUE);
    }
    else if (database)
      database->StartBatch();
    
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgFolder::GetMessageHeader(nsMsgKey msgKey, nsIMsgDBHdr **aMsgHdr)
{
  nsresult rv = NS_OK;
  if (aMsgHdr)
  {
    nsCOMPtr <nsIMsgDatabase> database;

    rv = GetMsgDatabase(nsnull, getter_AddRefs(database));
    if (NS_SUCCEEDED(rv) && database) // did we get a db back?
      rv = database->GetMsgHdrForKey(msgKey, aMsgHdr);
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

// this gets the deep sub-folders too, e.g., the children of the children
NS_IMETHODIMP nsMsgFolder::ListDescendents(nsISupportsArray *descendents)
{
  NS_ENSURE_ARG(descendents);
	PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 index = 0; index < cnt; index++)
	{
    nsresult rv;
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(index));
		nsCOMPtr<nsIMsgFolder> child(do_QueryInterface(supports, &rv));

    if (NS_SUCCEEDED(rv))
    {
      rv = descendents->AppendElement(supports);
		  if(NS_SUCCEEDED(rv))
		  {
			  rv = child->ListDescendents(descendents);  // recurse
		  }
    }
	}
  return rv;
} 

NS_IMETHODIMP nsMsgFolder::GetBaseMessageURI(char **baseMessageURI)
{
    NS_ENSURE_ARG_POINTER(baseMessageURI);

    if (mBaseMessageURI)
    {
      *baseMessageURI = nsCRT::strdup(mBaseMessageURI);
      return NS_OK;
    }
    else
      return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolder::GetUriForMsg(nsIMsgDBHdr *msgHdr, char **aURI)
{
  NS_ENSURE_ARG(msgHdr);
  NS_ENSURE_ARG(aURI);
  nsMsgKey msgKey;
  msgHdr->GetMessageKey(&msgKey);
  nsCAutoString uri;
  uri.Assign(mBaseMessageURI);

  // append a "#" followed by the message key.
  uri.Append('#');
  uri.AppendInt(msgKey);

  *aURI = ToNewCString(uri);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GenerateMessageURI(nsMsgKey msgKey, char **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsXPIDLCString baseURI;

  nsresult rv = GetBaseMessageURI(getter_Copies(baseURI));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCAutoString uri;
  uri.Assign(baseURI);

  // append a "#" followed by the message key.
  uri.Append('#');
  uri.AppendInt(msgKey);

  *aURI = ToNewCString(uri);
  if (! *aURI)
	  return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

nsresult
nsMsgFolder::GetBaseStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService =
         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                 getter_AddRefs(bundle));
  *aBundle = bundle;
  NS_IF_ADDREF(*aBundle);
  return rv;
}

nsresult //Do not use this routine if you have to call it very often because it creates a new bundle each time
nsMsgFolder::GetStringFromBundle(const char *msgName, PRUnichar **aResult)
{ 
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aResult);
  nsCOMPtr <nsIStringBundle> bundle;
  rv = GetBaseStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
    rv=bundle->GetStringFromName(NS_ConvertASCIItoUCS2(msgName).get(), aResult);
  return rv;
  
}

nsresult
nsMsgFolder::ThrowConfirmationPrompt(nsIMsgWindow *msgWindow, const PRUnichar *confirmString, PRBool *confirmed)
{
  nsresult rv=NS_OK;
  if (msgWindow) 
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && confirmString)
        dialog->Confirm(nsnull, confirmString, confirmed);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgFolder::GetStringWithFolderNameFromBundle(const char *msgName, PRUnichar **aResult)
{
  nsCOMPtr <nsIStringBundle> bundle;
  nsresult rv = GetBaseStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
  {
    nsXPIDLString folderName;
    GetName(getter_Copies(folderName));
    const PRUnichar *formatStrings[] =
    {
      folderName
    };
    rv = bundle->FormatStringFromName(NS_ConvertASCIItoUCS2(msgName).get(),
                                      formatStrings, 1, aResult);
  }
  return rv;
}

NS_IMETHODIMP nsMsgFolder::ConfirmFolderDeletionForFilter(nsIMsgWindow *msgWindow, PRBool *confirmed)
{
  nsXPIDLString confirmString;
  nsresult rv = GetStringWithFolderNameFromBundle("confirmFolderDeletionForFilter", getter_Copies(confirmString));
  if (NS_SUCCEEDED(rv) && confirmString)
    rv = ThrowConfirmationPrompt(msgWindow, confirmString.get(), confirmed);
  return rv;
}

NS_IMETHODIMP nsMsgFolder::ThrowAlertMsg(const char*msgName, nsIMsgWindow *msgWindow)
{
  nsXPIDLString alertString;
  nsresult rv = GetStringWithFolderNameFromBundle(msgName, getter_Copies(alertString));
  if (NS_SUCCEEDED(rv) && alertString && msgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && alertString)
        dialog->Alert(nsnull, alertString);
  }
  }
  return rv;
}

NS_IMETHODIMP nsMsgFolder::AlertFilterChanged(nsIMsgWindow *msgWindow)
{  //this is a different alert i.e  alert w/ checkbox.
  nsresult rv = NS_OK;
  PRBool checkBox=PR_FALSE;
  GetWarnFilterChanged(&checkBox);
  if (msgWindow && !checkBox) 
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    nsXPIDLString alertString;
    rv = GetStringFromBundle("alertFilterChanged", getter_Copies(alertString));
    nsXPIDLString alertCheckbox;
    rv = GetStringFromBundle("alertFilterCheckbox", getter_Copies(alertCheckbox));
    if (alertString && alertCheckbox && docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog)
      {
        dialog->AlertCheck(nsnull, alertString, alertCheckbox, &checkBox);
        SetWarnFilterChanged(checkBox);
      }   
    }
  }
  return rv;
}

nsresult
nsMsgFolder::GetWarnFilterChanged(PRBool *aVal)
{
  NS_ENSURE_ARG(aVal);
  nsresult rv;
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefService)
  {
    rv = prefService->GetBoolPref(PREF_MAIL_WARN_FILTER_CHANGED, aVal);
    if (NS_FAILED(rv)) 
    {
      *aVal = PR_FALSE;
      rv = NS_OK;
    }
  }
  return rv;
}

nsresult 
nsMsgFolder::SetWarnFilterChanged(PRBool aVal)
{
  nsresult rv=NS_OK;
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefService)
    rv = prefService->SetBoolPref(PREF_MAIL_WARN_FILTER_CHANGED, aVal);
  return rv;
}

NS_IMETHODIMP nsMsgFolder::NotifyCompactCompleted()
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::SetSortOrder(PRInt32 order)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::GetSortOrder(PRInt32 *order)
{
  NS_ENSURE_ARG_POINTER(order);

  PRUint32 flags;
  nsresult rv = GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv,rv);

  if (flags & MSG_FOLDER_FLAG_INBOX)
    *order = 0;
  else if (flags & MSG_FOLDER_FLAG_QUEUE)
    *order = 1;
  else if (flags & MSG_FOLDER_FLAG_DRAFTS)
    *order = 2;
  else if (flags & MSG_FOLDER_FLAG_TEMPLATES)
    *order = 3;
  else if (flags & MSG_FOLDER_FLAG_SENTMAIL)
    *order = 4;
  else if (flags & MSG_FOLDER_FLAG_TRASH)
    *order = 5;
  else
    *order = 6;

  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetPersistElided(PRBool *aPersistElided)
{
  // by default, we should always persist the open / closed state of folders & servers
  *aPersistElided = PR_TRUE;
  return NS_OK;
}
