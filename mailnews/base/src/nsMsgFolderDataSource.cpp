/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "nsMsgFolderDataSource.h"
#include "nsMsgFolderFlags.h"

#include "nsMsgRDFUtils.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"
#include "nsAdapterEnumerator.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"
#include "nsIInputStream.h"
#include "nsIMsgHdr.h"
#include "nsTraceRefcnt.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsArray.h"
#include "nsIPop3IncomingServer.h"

nsIRDFResource* nsMsgFolderDataSource::kNC_Child = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Folder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Name= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Open = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeName= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeSimpleName= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NameSort= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeNameSort= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SpecialFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_ServerType = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsDeferred = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_RedirectorType = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCreateFoldersOnServer = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanFileMessagesOnServer = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsServer = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsSecure = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanSubscribe = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SupportsOffline = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanFileMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCreateSubfolders = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanRename = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCompact = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalUnreadMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderSize = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Charset = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_BiffState = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_HasUnreadMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SubfoldersHaveUnreadMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NoSelect = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_VirtualFolder = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_ImapShared = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Synchronize = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SyncDisabled = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanSearchMessages = nsnull;
// commands
nsIRDFResource* nsMsgFolderDataSource::kNC_Delete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_ReallyDelete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_GetNewMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Copy= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Move= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CopyFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MoveFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MarkAllMessagesRead= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Compact= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CompactAll= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Rename= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_EmptyTrash= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_DownloadFlagged= nsnull;

nsrefcnt nsMsgFolderDataSource::gFolderResourceRefCnt = 0;

nsIAtom * nsMsgFolderDataSource::kBiffStateAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kNewMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kTotalMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kTotalUnreadMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kFolderSizeAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kNameAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kSynchronizeAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kOpenAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kIsDeferredAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kCanFileMessagesAtom = nsnull;

static const PRUint32 kDisplayBlankCount = 0xFFFFFFFE;
static const PRUint32 kDisplayQuestionCount = 0xFFFFFFFF;

nsMsgFolderDataSource::nsMsgFolderDataSource()
{
  // one-time initialization here
  nsIRDFService* rdf = getRDFService();
  
  if (gFolderResourceRefCnt++ == 0) {
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CHILD),   &kNC_Child);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDER),  &kNC_Folder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NAME),    &kNC_Name);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_OPEN),    &kNC_Open);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREENAME),    &kNC_FolderTreeName);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREESIMPLENAME),    &kNC_FolderTreeSimpleName);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NAME_SORT),    &kNC_NameSort);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERTREENAME_SORT),    &kNC_FolderTreeNameSort);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SPECIALFOLDER), &kNC_SpecialFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SERVERTYPE), &kNC_ServerType);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISDEFERRED),&kNC_IsDeferred);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_REDIRECTORTYPE), &kNC_RedirectorType);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCREATEFOLDERSONSERVER), &kNC_CanCreateFoldersOnServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANFILEMESSAGESONSERVER), &kNC_CanFileMessagesOnServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISSERVER), &kNC_IsServer);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_ISSECURE), &kNC_IsSecure);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANSUBSCRIBE), &kNC_CanSubscribe);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SUPPORTSOFFLINE), &kNC_SupportsOffline);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANFILEMESSAGES), &kNC_CanFileMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCREATESUBFOLDERS), &kNC_CanCreateSubfolders);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANRENAME), &kNC_CanRename);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANCOMPACT), &kNC_CanCompact);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_TOTALMESSAGES), &kNC_TotalMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_TOTALUNREADMESSAGES), &kNC_TotalUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_FOLDERSIZE), &kNC_FolderSize);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CHARSET), &kNC_Charset);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_BIFFSTATE), &kNC_BiffState);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_HASUNREADMESSAGES), &kNC_HasUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NEWMESSAGES), &kNC_NewMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SUBFOLDERSHAVEUNREADMESSAGES), &kNC_SubfoldersHaveUnreadMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NOSELECT), &kNC_NoSelect);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_VIRTUALFOLDER), &kNC_VirtualFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_IMAPSHARED), &kNC_ImapShared);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SYNCHRONIZE), &kNC_Synchronize);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_SYNCDISABLED), &kNC_SyncDisabled);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_CANSEARCHMESSAGES), &kNC_CanSearchMessages);
    
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DELETE), &kNC_Delete);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_REALLY_DELETE), &kNC_ReallyDelete);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_NEWFOLDER), &kNC_NewFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_GETNEWMESSAGES), &kNC_GetNewMessages);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COPY), &kNC_Copy);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MOVE), &kNC_Move);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COPYFOLDER), &kNC_CopyFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MOVEFOLDER), &kNC_MoveFolder);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_MARKALLMESSAGESREAD),
                             &kNC_MarkAllMessagesRead);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COMPACT), &kNC_Compact);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_COMPACTALL), &kNC_CompactAll);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_RENAME), &kNC_Rename);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_EMPTYTRASH), &kNC_EmptyTrash);
    rdf->GetResource(NS_LITERAL_CSTRING(NC_RDF_DOWNLOADFLAGGED), &kNC_DownloadFlagged);

    kTotalMessagesAtom           = NS_NewAtom("TotalMessages");
    kTotalUnreadMessagesAtom     = NS_NewAtom("TotalUnreadMessages");
    kFolderSizeAtom              = NS_NewAtom("FolderSize");
    kBiffStateAtom               = NS_NewAtom("BiffState");
    kNewMessagesAtom             = NS_NewAtom("NewMessages");
    kNameAtom                    = NS_NewAtom("Name");
    kSynchronizeAtom             = NS_NewAtom("Synchronize");
    kOpenAtom                    = NS_NewAtom("open");
    kIsDeferredAtom              = NS_NewAtom("isDeferred");
    kCanFileMessagesAtom         = NS_NewAtom("canFileMessages");
  }
  
  CreateLiterals(rdf);
  CreateArcsOutEnumerator();
}

nsMsgFolderDataSource::~nsMsgFolderDataSource (void)
{
  if (--gFolderResourceRefCnt == 0)
  {
    nsrefcnt refcnt;
    NS_RELEASE2(kNC_Child, refcnt);
    NS_RELEASE2(kNC_Folder, refcnt);
    NS_RELEASE2(kNC_Name, refcnt);
    NS_RELEASE2(kNC_Open, refcnt);
    NS_RELEASE2(kNC_FolderTreeName, refcnt);
    NS_RELEASE2(kNC_FolderTreeSimpleName, refcnt);
    NS_RELEASE2(kNC_NameSort, refcnt);
    NS_RELEASE2(kNC_FolderTreeNameSort, refcnt);
    NS_RELEASE2(kNC_SpecialFolder, refcnt);
    NS_RELEASE2(kNC_ServerType, refcnt);
    NS_RELEASE2(kNC_IsDeferred, refcnt);
    NS_RELEASE2(kNC_RedirectorType, refcnt);
    NS_RELEASE2(kNC_CanCreateFoldersOnServer, refcnt);
    NS_RELEASE2(kNC_CanFileMessagesOnServer, refcnt);
    NS_RELEASE2(kNC_IsServer, refcnt);
    NS_RELEASE2(kNC_IsSecure, refcnt);
    NS_RELEASE2(kNC_CanSubscribe, refcnt);
    NS_RELEASE2(kNC_SupportsOffline, refcnt);
    NS_RELEASE2(kNC_CanFileMessages, refcnt);
    NS_RELEASE2(kNC_CanCreateSubfolders, refcnt);
    NS_RELEASE2(kNC_CanRename, refcnt);
    NS_RELEASE2(kNC_CanCompact, refcnt);
    NS_RELEASE2(kNC_TotalMessages, refcnt);
    NS_RELEASE2(kNC_TotalUnreadMessages, refcnt);
    NS_RELEASE2(kNC_FolderSize, refcnt);
    NS_RELEASE2(kNC_Charset, refcnt);
    NS_RELEASE2(kNC_BiffState, refcnt);
    NS_RELEASE2(kNC_HasUnreadMessages, refcnt);
    NS_RELEASE2(kNC_NewMessages, refcnt);
    NS_RELEASE2(kNC_SubfoldersHaveUnreadMessages, refcnt);
    NS_RELEASE2(kNC_NoSelect, refcnt);
    NS_RELEASE2(kNC_VirtualFolder, refcnt);
    NS_RELEASE2(kNC_ImapShared, refcnt);
    NS_RELEASE2(kNC_Synchronize, refcnt);
    NS_RELEASE2(kNC_SyncDisabled, refcnt);
    NS_RELEASE2(kNC_CanSearchMessages, refcnt);
    
    NS_RELEASE2(kNC_Delete, refcnt);
    NS_RELEASE2(kNC_ReallyDelete, refcnt);
    NS_RELEASE2(kNC_NewFolder, refcnt);
    NS_RELEASE2(kNC_GetNewMessages, refcnt);
    NS_RELEASE2(kNC_Copy, refcnt);
    NS_RELEASE2(kNC_Move, refcnt);
    NS_RELEASE2(kNC_CopyFolder, refcnt);
    NS_RELEASE2(kNC_MoveFolder, refcnt);
    NS_RELEASE2(kNC_MarkAllMessagesRead, refcnt);
    NS_RELEASE2(kNC_Compact, refcnt);
    NS_RELEASE2(kNC_CompactAll, refcnt);
    NS_RELEASE2(kNC_Rename, refcnt);
    NS_RELEASE2(kNC_EmptyTrash, refcnt);
    NS_RELEASE2(kNC_DownloadFlagged, refcnt);
    
    NS_RELEASE(kTotalMessagesAtom);
    NS_RELEASE(kTotalUnreadMessagesAtom);
    NS_RELEASE(kFolderSizeAtom);
    NS_RELEASE(kBiffStateAtom);
    NS_RELEASE(kNewMessagesAtom);
    NS_RELEASE(kNameAtom);
    NS_RELEASE(kSynchronizeAtom);
    NS_RELEASE(kOpenAtom);
    NS_RELEASE(kIsDeferredAtom);
    NS_RELEASE(kCanFileMessagesAtom);
  }
}

nsresult nsMsgFolderDataSource::CreateLiterals(nsIRDFService *rdf)
{
  createNode(NS_LITERAL_STRING("true").get(),
    getter_AddRefs(kTrueLiteral), rdf);
  createNode(NS_LITERAL_STRING("false").get(),
    getter_AddRefs(kFalseLiteral), rdf);

  return NS_OK;
}

nsresult nsMsgFolderDataSource::Init()
{
  nsresult rv;
  
  rv = nsMsgRDFDataSource::Init();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMsgMailSession> mailSession = 
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 

  if(NS_SUCCEEDED(rv))
    mailSession->AddFolderListener(this, 
      nsIFolderListener::added |
      nsIFolderListener::removed |
      nsIFolderListener::intPropertyChanged |
      nsIFolderListener::boolPropertyChanged |
      nsIFolderListener::unicharPropertyChanged |
      nsIFolderListener::propertyFlagChanged);

  return NS_OK;
}

void nsMsgFolderDataSource::Cleanup()
{
  nsresult rv;
  if (!m_shuttingDown)
  {
    nsCOMPtr<nsIMsgMailSession> mailSession =
      do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
    
    if(NS_SUCCEEDED(rv))
      mailSession->RemoveFolderListener(this);
  }
  
  nsMsgRDFDataSource::Cleanup();
}

nsresult nsMsgFolderDataSource::CreateArcsOutEnumerator()
{
  nsresult rv;

  rv = getFolderArcLabelsOut(getter_AddRefs(kFolderArcsOutArray));
  if(NS_FAILED(rv)) return rv;

  return rv;
}

NS_IMPL_ADDREF_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgFolderDataSource, nsMsgRDFDataSource)

NS_IMETHODIMP
nsMsgFolderDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;
  
  *result = nsnull;
  if(iid.Equals(NS_GET_IID(nsIFolderListener)))
  {
    *result = NS_STATIC_CAST(nsIFolderListener*, this);
    NS_ADDREF(this);
    return NS_OK;
  }
  else
    return nsMsgRDFDataSource::QueryInterface(iid, result);
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsMsgFolderDataSource::GetURI(char* *uri)
{
  if ((*uri = nsCRT::strdup("rdf:mailnewsfolders")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source));
  if (folder) 
  {
    rv = createFolderNode(folder, property, target);
#if 0
    nsXPIDLCString srcval;
    nsXPIDLCString propval;
    nsXPIDLCString targetval;
    source->GetValue(getter_Copies(srcval));
    property->GetValue(getter_Copies(propval));
    //    (*target)->GetValue(getter_Copies(targetval));

    printf("nsMsgFolderDataSource::GetTarget(%s, %s, %s, (%s))\n",
           (const char*)srcval,
           (const char*)propval, tv ? "TRUE" : "FALSE",
           (const char*)"");
#endif
    
  }
  else
    return NS_RDF_NO_VALUE;
  return rv;
}


NS_IMETHODIMP nsMsgFolderDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
  return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
  nsresult rv = NS_RDF_NO_VALUE;
  if(!targets)
    return NS_ERROR_NULL_POINTER;
  
#if 0
  nsXPIDLCString srcval;
  nsXPIDLCString propval;
  nsXPIDLCString targetval;
  source->GetValue(getter_Copies(srcval));
  property->GetValue(getter_Copies(propval));
  //    (*target)->GetValue(getter_Copies(targetval));
  
  printf("nsMsgFolderDataSource::GetTargets(%s, %s, %s, (%s))\n",
    (const char*)srcval,
    (const char*)propval, tv ? "TRUE" : "FALSE",
    (const char*)"");
#endif
  *targets = nsnull;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv))
  {
    if ((kNC_Child == property))
    {
      nsCOMPtr<nsIEnumerator> subFolders;
      rv = folder->GetSubFolders(getter_AddRefs(subFolders));
      if(NS_SUCCEEDED(rv))
      {
        nsAdapterEnumerator* cursor =
          new nsAdapterEnumerator(subFolders);
        if (cursor == nsnull)
          return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(cursor);
        *targets = cursor;
        rv = NS_OK;
      }
    }
    else if ((kNC_Name == property) ||
      (kNC_Open == property) ||
      (kNC_FolderTreeName == property) ||
      (kNC_FolderTreeSimpleName == property) ||
      (kNC_SpecialFolder == property) ||
      (kNC_IsServer == property) ||
      (kNC_IsSecure == property) ||
      (kNC_CanSubscribe == property) ||
      (kNC_SupportsOffline == property) ||
      (kNC_CanFileMessages == property) ||
      (kNC_CanCreateSubfolders == property) ||
      (kNC_CanRename == property) ||
      (kNC_CanCompact == property) ||
      (kNC_ServerType == property) ||
      (kNC_IsDeferred == property) ||
      (kNC_RedirectorType == property) ||
      (kNC_CanCreateFoldersOnServer == property) ||
      (kNC_CanFileMessagesOnServer == property) ||
      (kNC_NoSelect == property) ||
      (kNC_VirtualFolder == property) ||
      (kNC_ImapShared == property) ||
      (kNC_Synchronize == property) ||
      (kNC_SyncDisabled == property) ||
      (kNC_CanSearchMessages == property))
    {
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
      rv = NS_OK;
    }
  }
  if(!*targets)
  {
    //create empty cursor
    rv = NS_NewEmptyEnumerator(targets);
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgFolderDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  //We don't handle tv = PR_FALSE at the moment.
  if(NS_SUCCEEDED(rv) && tv)
    return DoFolderAssert(folder, property, target);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolderDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  NS_ENSURE_SUCCESS(rv,rv);
  return DoFolderUnassert(folder, property, target);
}


NS_IMETHODIMP nsMsgFolderDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
  nsresult rv;
#if 0
  nsXPIDLCString sourceval;
  nsXPIDLCString propval;
  nsXPIDLCString targetval;
  source->GetValue(getter_Copies(sourceval));
  property->GetValue(getter_Copies(propval));
  /*  target->GetValue(getter_Copies(targetval)); */
  printf("HasAssertion(%s, %s, ??...)\n", (const char*)sourceval, (const char*)propval);
#endif
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if(NS_SUCCEEDED(rv))
    return DoFolderHasAssertion(folder, property, target, tv, hasAssertion);
  else
    *hasAssertion = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP 
nsMsgFolderDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(aSource, &rv));
  if (NS_SUCCEEDED(rv)) 
  {
    *result = (aArc == kNC_Name ||
      aArc == kNC_Open ||
      aArc == kNC_FolderTreeName ||
      aArc == kNC_FolderTreeSimpleName ||
      aArc == kNC_SpecialFolder ||
      aArc == kNC_ServerType ||
      aArc == kNC_IsDeferred ||
      aArc == kNC_RedirectorType ||
      aArc == kNC_CanCreateFoldersOnServer ||
      aArc == kNC_CanFileMessagesOnServer ||
      aArc == kNC_IsServer ||
      aArc == kNC_IsSecure ||
      aArc == kNC_CanSubscribe ||
      aArc == kNC_SupportsOffline ||
      aArc == kNC_CanFileMessages ||
      aArc == kNC_CanCreateSubfolders ||
      aArc == kNC_CanRename ||
      aArc == kNC_CanCompact ||
      aArc == kNC_TotalMessages ||
      aArc == kNC_TotalUnreadMessages ||
      aArc == kNC_FolderSize ||
      aArc == kNC_Charset ||
      aArc == kNC_BiffState ||
      aArc == kNC_Child ||
      aArc == kNC_NoSelect ||
      aArc == kNC_VirtualFolder ||
      aArc == kNC_ImapShared ||
      aArc == kNC_Synchronize ||
      aArc == kNC_SyncDisabled ||
      aArc == kNC_CanSearchMessages);
  }
  else 
  {
    *result = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsISimpleEnumerator** labels)
{
  return nsMsgRDFDataSource::ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsMsgFolderDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsISimpleEnumerator** labels)
{
  nsresult rv = NS_RDF_NO_VALUE;
  nsCOMPtr<nsISupportsArray> arcsArray;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) 
  {
    arcsArray = kFolderArcsOutArray;
    rv = NS_NewArrayEnumerator(labels, arcsArray);
  }
  else 
  {
    rv = NS_NewEmptyEnumerator(labels);
  }
  
  return rv;
}

nsresult
nsMsgFolderDataSource::getFolderArcLabelsOut(nsISupportsArray **arcs)
{
  nsresult rv;
  rv = NS_NewISupportsArray(arcs);
  if(NS_FAILED(rv))
    return rv;
  
  (*arcs)->AppendElement(kNC_Name);
  (*arcs)->AppendElement(kNC_Open);
  (*arcs)->AppendElement(kNC_FolderTreeName);
  (*arcs)->AppendElement(kNC_FolderTreeSimpleName);
  (*arcs)->AppendElement(kNC_SpecialFolder);
  (*arcs)->AppendElement(kNC_ServerType);
  (*arcs)->AppendElement(kNC_IsDeferred);
  (*arcs)->AppendElement(kNC_RedirectorType);
  (*arcs)->AppendElement(kNC_CanCreateFoldersOnServer);
  (*arcs)->AppendElement(kNC_CanFileMessagesOnServer);
  (*arcs)->AppendElement(kNC_IsServer);
  (*arcs)->AppendElement(kNC_IsSecure);
  (*arcs)->AppendElement(kNC_CanSubscribe);
  (*arcs)->AppendElement(kNC_SupportsOffline);
  (*arcs)->AppendElement(kNC_CanFileMessages);
  (*arcs)->AppendElement(kNC_CanCreateSubfolders);
  (*arcs)->AppendElement(kNC_CanRename);
  (*arcs)->AppendElement(kNC_CanCompact);
  (*arcs)->AppendElement(kNC_TotalMessages);
  (*arcs)->AppendElement(kNC_TotalUnreadMessages);
  (*arcs)->AppendElement(kNC_FolderSize);
  (*arcs)->AppendElement(kNC_Charset);
  (*arcs)->AppendElement(kNC_BiffState);
  (*arcs)->AppendElement(kNC_Child);
  (*arcs)->AppendElement(kNC_NoSelect);
  (*arcs)->AppendElement(kNC_VirtualFolder);
  (*arcs)->AppendElement(kNC_ImapShared);
  (*arcs)->AppendElement(kNC_Synchronize);
  (*arcs)->AppendElement(kNC_SyncDisabled);
  (*arcs)->AppendElement(kNC_CanSearchMessages);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("no one actually uses me");
  nsresult rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMutableArray> cmds;
  NS_NewArray(getter_AddRefs(cmds));
  if (!cmds) return rv;

  cmds->AppendElement(kNC_Delete, PR_FALSE);
  cmds->AppendElement(kNC_ReallyDelete, PR_FALSE);
  cmds->AppendElement(kNC_NewFolder, PR_FALSE);
  cmds->AppendElement(kNC_GetNewMessages, PR_FALSE);
  cmds->AppendElement(kNC_Copy, PR_FALSE);
  cmds->AppendElement(kNC_Move, PR_FALSE);
  cmds->AppendElement(kNC_CopyFolder, PR_FALSE);
  cmds->AppendElement(kNC_MoveFolder, PR_FALSE);
  cmds->AppendElement(kNC_MarkAllMessagesRead, PR_FALSE);
  cmds->AppendElement(kNC_Compact, PR_FALSE);
  cmds->AppendElement(kNC_CompactAll, PR_FALSE);
  cmds->AppendElement(kNC_Rename, PR_FALSE);
  cmds->AppendElement(kNC_EmptyTrash, PR_FALSE);
  cmds->AppendElement(kNC_DownloadFlagged, PR_FALSE);

  return cmds->Enumerate(commands);
}

NS_IMETHODIMP
nsMsgFolderDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder;

  PRUint32 cnt;
  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) 
  {
    folder = do_QueryElementAt(aSources, i, &rv);
    if (NS_SUCCEEDED(rv)) 
    {
      // we don't care about the arguments -- folder commands are always enabled
      if (!((aCommand == kNC_Delete) ||
            (aCommand == kNC_ReallyDelete) ||
            (aCommand == kNC_NewFolder) ||
            (aCommand == kNC_Copy) ||
            (aCommand == kNC_Move) ||
            (aCommand == kNC_CopyFolder) ||
            (aCommand == kNC_MoveFolder) ||
            (aCommand == kNC_GetNewMessages) ||
            (aCommand == kNC_MarkAllMessagesRead) ||
            (aCommand == kNC_Compact) || 
            (aCommand == kNC_CompactAll) || 
            (aCommand == kNC_Rename) ||
            (aCommand == kNC_EmptyTrash) ||
            (aCommand == kNC_DownloadFlagged) )) 
      {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMsgFolderDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> supports;
  // XXX need to handle batching of command applied to all sources

  PRUint32 cnt = 0;
  PRUint32 i = 0;

  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  for ( ; i < cnt; i++) 
  {
    nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(aSources, i, &rv);
    if (NS_SUCCEEDED(rv)) 
    {
      if ((aCommand == kNC_Delete))
      {
        rv = DoDeleteFromFolder(folder, aArguments, mWindow, PR_FALSE);
      }
      if ((aCommand == kNC_ReallyDelete))
      {
        rv = DoDeleteFromFolder(folder, aArguments, mWindow, PR_TRUE);
      }
      else if((aCommand == kNC_NewFolder)) 
      {
        rv = DoNewFolder(folder, aArguments);
      }
      else if((aCommand == kNC_GetNewMessages))
      {
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(aArguments, i, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = server->GetNewMessages(folder, mWindow, nsnull);
      }
      else if((aCommand == kNC_Copy))
      {
        rv = DoCopyToFolder(folder, aArguments, mWindow, PR_FALSE);
      }
      else if((aCommand == kNC_Move))
      {
        rv = DoCopyToFolder(folder, aArguments, mWindow, PR_TRUE);
      }
      else if((aCommand == kNC_CopyFolder))
      {
        rv = DoFolderCopyToFolder(folder, aArguments, mWindow, PR_FALSE);
      }
      else if((aCommand == kNC_MoveFolder))
      {
        rv = DoFolderCopyToFolder(folder, aArguments, mWindow, PR_TRUE);
      }
      else if((aCommand == kNC_MarkAllMessagesRead))
      {
        rv = folder->MarkAllMessagesRead();
      }
      else if ((aCommand == kNC_Compact))
      {
        rv = folder->Compact(nsnull, mWindow);
      }
      else if ((aCommand == kNC_CompactAll))
      {
        rv = folder->CompactAll(nsnull, mWindow, nsnull, PR_TRUE, nsnull);
      }
      else if ((aCommand == kNC_EmptyTrash))
      {
          rv = folder->EmptyTrash(mWindow, nsnull);
      }
      else if ((aCommand == kNC_Rename))
      {
        nsCOMPtr<nsIRDFLiteral> literal = do_QueryElementAt(aArguments, 0, &rv);
        if(NS_SUCCEEDED(rv))
        {
          nsXPIDLString name;
          literal->GetValue(getter_Copies(name));

          rv = folder->Rename(name.get(),mWindow);
        }
      }
    }
    else 
    {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
  }
  //for the moment return NS_OK, because failure stops entire DoCommand process.
  return rv;
  //return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemAdded(nsIRDFResource *parentItem, nsISupports *item)
{
  return OnItemAddedOrRemoved(parentItem, item, PR_TRUE);
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemRemoved(nsIRDFResource *parentItem, nsISupports *item)
{
  return OnItemAddedOrRemoved(parentItem, item, PR_FALSE);
}


nsresult nsMsgFolderDataSource::OnItemAddedOrRemoved(nsIRDFResource *parentItem, nsISupports *item, PRBool added)
{
  nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item));
  if (itemNode)
  {
    NotifyObservers(parentItem, kNC_Child, itemNode, nsnull, added, PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyChanged(nsIRDFResource *resource,
                                             nsIAtom *property,
                                             const char *oldValue,
                                             const char *newValue)

{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemIntPropertyChanged(nsIRDFResource *resource,
                                                nsIAtom *property,
                                                PRInt32 oldValue,
                                                PRInt32 newValue)
{
  if (kTotalMessagesAtom == property)
    OnTotalMessagePropertyChanged(resource, oldValue, newValue);
  else if (kTotalUnreadMessagesAtom == property)
    OnUnreadMessagePropertyChanged(resource, oldValue, newValue);
  else if (kFolderSizeAtom == property)
    OnFolderSizePropertyChanged(resource, oldValue, newValue);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemUnicharPropertyChanged(nsIRDFResource *resource,
                                                    nsIAtom *property,
                                                    const PRUnichar *oldValue,
                                                    const PRUnichar *newValue)
{
  if (kNameAtom == property) 
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(resource));
    if (folder) 
    {
      PRInt32 numUnread;
      folder->GetNumUnread(PR_FALSE, &numUnread);
      NotifyFolderTreeNameChanged(folder, resource, numUnread);
      NotifyFolderTreeSimpleNameChanged(folder, resource);
      NotifyFolderNameChanged(folder, resource);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemBoolPropertyChanged(nsIRDFResource *resource,
                                                 nsIAtom *property,
                                                 PRBool oldValue,
                                                 PRBool newValue)
{
  if (newValue != oldValue) {
    nsIRDFNode* literalNode = newValue?kTrueLiteral:kFalseLiteral;
    nsIRDFNode* oldLiteralNode = oldValue?kTrueLiteral:kFalseLiteral;
    if (kNewMessagesAtom == property)
      NotifyPropertyChanged(resource, kNC_NewMessages, literalNode); 
    else if (kSynchronizeAtom == property)
      NotifyPropertyChanged(resource, kNC_Synchronize, literalNode); 
    else if (kOpenAtom == property)
      NotifyPropertyChanged(resource, kNC_Open, literalNode);
    else if (kIsDeferredAtom == property) 
      NotifyPropertyChanged(resource, kNC_IsDeferred, literalNode, oldLiteralNode);
    else if (kCanFileMessagesAtom == property)
      NotifyPropertyChanged(resource, kNC_CanFileMessages, literalNode, oldLiteralNode);
  } 

  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyFlagChanged(nsISupports *item,
                                                 nsIAtom *property,
                                                 PRUint32 oldFlag,
                                                 PRUint32 newFlag)
{
  nsresult rv = NS_OK;

  if (kBiffStateAtom == property) {
    // for Incoming biff (to turn it on) the item is of type nsIMsgFolder (see nsMsgFolder::SetBiffState)
    // for clearing the biff the item is of type nsIMsgDBHdr (see nsMsgDBFolder::OnKeyChange)
    // so check for both of these here
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
    if(!folder) {
      nsCOMPtr<nsIMsgDBHdr> msgHdr  = do_QueryInterface(item);
      if (msgHdr)
        rv = msgHdr->GetFolder(getter_AddRefs(folder));
      if(NS_FAILED(rv))
        return rv;
    }

    nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(folder));
    if(resource) {
      // be careful about skipping if oldFlag == newFlag
      // see the comment in nsMsgFolder::SetBiffState() about filters

      nsCOMPtr<nsIRDFNode> biffNode;
      rv = createBiffStateNodeFromFlag(newFlag, getter_AddRefs(biffNode));
      NS_ENSURE_SUCCESS(rv,rv);

      NotifyPropertyChanged(resource, kNC_BiffState, biffNode);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemEvent(nsIMsgFolder *aFolder, nsIAtom *aEvent)
{
  return NS_OK;
}


nsresult nsMsgFolderDataSource::createFolderNode(nsIMsgFolder* folder,
                                                 nsIRDFResource* property,
                                                 nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (kNC_NameSort == property)
    rv = createFolderNameNode(folder, target, PR_TRUE);
  else if(kNC_FolderTreeNameSort == property)
    rv = createFolderNameNode(folder, target, PR_TRUE);
  else if (kNC_Name == property)
    rv = createFolderNameNode(folder, target, PR_FALSE);
  else if(kNC_Open == property)
    rv = createFolderOpenNode(folder, target);
  else if (kNC_FolderTreeName == property)
    rv = createFolderTreeNameNode(folder, target);
  else if (kNC_FolderTreeSimpleName == property)
    rv = createFolderTreeSimpleNameNode(folder, target);
  else if ((kNC_SpecialFolder == property))
    rv = createFolderSpecialNode(folder,target);
  else if ((kNC_ServerType == property))
    rv = createFolderServerTypeNode(folder, target);
  else if ((kNC_IsDeferred == property))
    rv = createServerIsDeferredNode(folder, target);
  else if ((kNC_RedirectorType == property))
    rv = createFolderRedirectorTypeNode(folder, target);
  else if ((kNC_CanCreateFoldersOnServer == property))
    rv = createFolderCanCreateFoldersOnServerNode(folder, target);
  else if ((kNC_CanFileMessagesOnServer == property))
    rv = createFolderCanFileMessagesOnServerNode(folder, target);
  else if ((kNC_IsServer == property))
    rv = createFolderIsServerNode(folder, target);
  else if ((kNC_IsSecure == property))
    rv = createFolderIsSecureNode(folder, target);
  else if ((kNC_CanSubscribe == property))
    rv = createFolderCanSubscribeNode(folder, target);
  else if ((kNC_SupportsOffline == property))
    rv = createFolderSupportsOfflineNode(folder, target);
  else if ((kNC_CanFileMessages == property))
    rv = createFolderCanFileMessagesNode(folder, target);
  else if ((kNC_CanCreateSubfolders == property))
    rv = createFolderCanCreateSubfoldersNode(folder, target);
  else if ((kNC_CanRename == property))
    rv = createFolderCanRenameNode(folder, target);
  else if ((kNC_CanCompact == property))
    rv = createFolderCanCompactNode(folder, target);
  else if ((kNC_TotalMessages == property))
    rv = createTotalMessagesNode(folder, target);
  else if ((kNC_TotalUnreadMessages == property))
    rv = createUnreadMessagesNode(folder, target);
  else if ((kNC_FolderSize == property))
    rv = createFolderSizeNode(folder, target);
  else if ((kNC_Charset == property))
    rv = createCharsetNode(folder, target);
  else if ((kNC_BiffState == property))
    rv = createBiffStateNodeFromFolder(folder, target);
  else if ((kNC_HasUnreadMessages == property))
    rv = createHasUnreadMessagesNode(folder, PR_FALSE, target);
  else if ((kNC_NewMessages == property))
    rv = createNewMessagesNode(folder, target);
  else if ((kNC_SubfoldersHaveUnreadMessages == property))
    rv = createHasUnreadMessagesNode(folder, PR_TRUE, target);
  else if ((kNC_Child == property))
    rv = createFolderChildNode(folder, target);
  else if ((kNC_NoSelect == property))
    rv = createFolderNoSelectNode(folder, target);
  else if ((kNC_VirtualFolder == property))
    rv = createFolderVirtualNode(folder, target);
  else if ((kNC_ImapShared == property))
    rv = createFolderImapSharedNode(folder, target);
  else if ((kNC_Synchronize == property))
    rv = createFolderSynchronizeNode(folder, target);
  else if ((kNC_SyncDisabled == property))
    rv = createFolderSyncDisabledNode(folder, target);
  else if ((kNC_CanSearchMessages == property))
    rv = createCanSearchMessages(folder, target);


  if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;
  return rv;
}


nsresult 
nsMsgFolderDataSource::createFolderNameNode(nsIMsgFolder *folder,
                                            nsIRDFNode **target, PRBool sort)
{
  nsresult rv;
  if (sort) 
  {
    PRUint8 *sortKey=nsnull;
    PRUint32 sortKeyLength;
    rv = folder->GetSortKey(&sortKey, &sortKeyLength);
    NS_ENSURE_SUCCESS(rv, rv);
    createBlobNode(sortKey, sortKeyLength, target, getRDFService());
    PR_Free(sortKey);
  }
  else 
  {
    nsXPIDLString name;
    rv = folder->GetName(getter_Copies(name));
    if (NS_FAILED(rv)) 
      return rv;
    createNode(name.get(), target, getRDFService());
  }
    
  return NS_OK;
}


nsresult 
nsMsgFolderDataSource::createFolderTreeNameNode(nsIMsgFolder *folder,
                                                nsIRDFNode **target)
{
  nsXPIDLString name;
  nsresult rv = folder->GetAbbreviatedName(getter_Copies(name));
  if (NS_FAILED(rv)) return rv;
  nsAutoString nameString(name);
  PRInt32 unreadMessages;

  rv = folder->GetNumUnread(PR_FALSE, &unreadMessages);
  if(NS_SUCCEEDED(rv)) 
    CreateUnreadMessagesNameString(unreadMessages, nameString);	

  createNode(nameString.get(), target, getRDFService());
  return NS_OK;
}

nsresult nsMsgFolderDataSource::createFolderTreeSimpleNameNode(nsIMsgFolder * folder, nsIRDFNode **target)
{
  nsXPIDLString name;
  nsresult rv = folder->GetAbbreviatedName(getter_Copies(name));
  if (NS_FAILED(rv)) return rv;

  createNode(name.get(), target, getRDFService());
  return NS_OK;
}

nsresult nsMsgFolderDataSource::CreateUnreadMessagesNameString(PRInt32 unreadMessages, nsAutoString &nameString)
{
  //Only do this if unread messages are positive
  if(unreadMessages > 0)
  {
    nameString.Append(NS_LITERAL_STRING(" (").get());
    nameString.AppendInt(unreadMessages);
    nameString.Append(NS_LITERAL_STRING(")").get());
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSpecialNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{
  PRUint32 flags;
  nsresult rv = folder->GetFlags(&flags);
  if(NS_FAILED(rv)) 
    return rv;
  
  nsAutoString specialFolderString;
  if (flags & MSG_FOLDER_FLAG_INBOX)
    specialFolderString.AssignLiteral("Inbox");
  else if (flags & MSG_FOLDER_FLAG_TRASH)
    specialFolderString.AssignLiteral("Trash");
  else if (flags & MSG_FOLDER_FLAG_QUEUE)
    specialFolderString.AssignLiteral("Unsent Messages");
  else if (flags & MSG_FOLDER_FLAG_SENTMAIL)
    specialFolderString.AssignLiteral("Sent");
  else if (flags & MSG_FOLDER_FLAG_DRAFTS)
    specialFolderString.AssignLiteral("Drafts");
  else if (flags & MSG_FOLDER_FLAG_TEMPLATES)
    specialFolderString.AssignLiteral("Templates");
  else if (flags & MSG_FOLDER_FLAG_JUNK)
    specialFolderString.AssignLiteral("Junk");
  else {
    // XXX why do this at all? or just ""
    specialFolderString.AssignLiteral("none");
  }
  
  createNode(specialFolderString.get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderServerTypeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  nsXPIDLCString serverType;
  rv = server->GetType(getter_Copies(serverType));
  if (NS_FAILED(rv)) return rv;

  createNode(NS_ConvertASCIItoUCS2(serverType).get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createServerIsDeferredNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  PRBool isDeferred = PR_FALSE;
  nsCOMPtr <nsIMsgIncomingServer> incomingServer;
  folder->GetServer(getter_AddRefs(incomingServer));
  if (incomingServer)
  {
    nsCOMPtr <nsIPop3IncomingServer> pop3Server = do_QueryInterface(incomingServer);
    if (pop3Server)
    {
      nsXPIDLCString deferredToServer;
      pop3Server->GetDeferredToAccount(getter_Copies(deferredToServer));
      isDeferred = !deferredToServer.IsEmpty();
    }
  }
  *target = (isDeferred) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderRedirectorTypeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  nsXPIDLCString redirectorType;
  rv = server->GetRedirectorType(getter_Copies(redirectorType));
  if (NS_FAILED(rv)) return rv;

  createNode(NS_ConvertASCIItoUCS2(redirectorType).get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCreateFoldersOnServerNode(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;
  
  PRBool canCreateFoldersOnServer;
  rv = server->GetCanCreateFoldersOnServer(&canCreateFoldersOnServer);
  if (NS_FAILED(rv)) return rv;

  if (canCreateFoldersOnServer)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanFileMessagesOnServerNode(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  PRBool canFileMessagesOnServer;
  rv = server->GetCanFileMessagesOnServer(&canFileMessagesOnServer);
  if (NS_FAILED(rv)) return rv;
  
  *target = (canFileMessagesOnServer) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderIsServerNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  *target = nsnull;

  if (isServer)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderNoSelectNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool noSelect;
  rv = folder->GetNoSelect(&noSelect);
  if (NS_FAILED(rv)) return rv;

  *target = (noSelect) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderVirtualNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  PRUint32 folderFlags;
  folder->GetFlags(&folderFlags);

  *target = (folderFlags & MSG_FOLDER_FLAG_VIRTUAL) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderImapSharedNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool imapShared; 
  rv = folder->GetImapShared(&imapShared);
  if (NS_FAILED(rv)) return rv;

  *target = (imapShared) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderSynchronizeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool sync;
  rv = folder->GetFlag(MSG_FOLDER_FLAG_OFFLINE, &sync);		
  if (NS_FAILED(rv)) return rv;

  *target = nsnull;

  *target = (sync) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSyncDisabledNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  
  nsresult rv;
  PRBool isServer;
  nsCOMPtr<nsIMsgIncomingServer> server;

  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
    
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  nsXPIDLCString serverType;
  rv = server->GetType(getter_Copies(serverType));
  if (NS_FAILED(rv)) return rv;

  *target = nsnull;

  if (nsCRT::strcasecmp(serverType, "none")==0 || nsCRT::strcasecmp(serverType, "pop3")==0
      || isServer)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createCanSearchMessages(nsIMsgFolder* folder,
                                                                 nsIRDFNode **target)
{
  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) return NS_ERROR_FAILURE;

  PRBool canSearchMessages;
  rv = server->GetCanSearchMessages(&canSearchMessages);
  if (NS_FAILED(rv)) return rv;
  
  *target = (canSearchMessages) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderOpenNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  NS_ENSURE_ARG_POINTER(target);

  // call GetSubFolders() to ensure mFlags is set correctly 
  // from the folder cache on startup
  nsCOMPtr<nsIEnumerator> subFolders;
  nsresult rv = folder->GetSubFolders(getter_AddRefs(subFolders));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  PRBool closed;
  rv = folder->GetFlag(MSG_FOLDER_FLAG_ELIDED, &closed);		
  if (NS_FAILED(rv)) 
    return rv;

  *target = (closed) ? kFalseLiteral : kTrueLiteral;

  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderIsSecureNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool isSecure;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv) || !server) {
    // this could be a folder, not a server, so that's ok;
    isSecure = PR_FALSE;
  }
  else {
    rv = server->GetIsSecure(&isSecure);
    if (NS_FAILED(rv)) return rv;
  }

  *target = (isSecure) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createFolderCanSubscribeNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool canSubscribe;
  rv = folder->GetCanSubscribe(&canSubscribe);
  if (NS_FAILED(rv)) return rv;

  *target = (canSubscribe) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSupportsOfflineNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool supportsOffline;
  rv = folder->GetSupportsOffline(&supportsOffline);
  NS_ENSURE_SUCCESS(rv,rv);
 
  *target = (supportsOffline) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanFileMessagesNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool canFileMessages;
  rv = folder->GetCanFileMessages(&canFileMessages);
  if (NS_FAILED(rv)) return rv;

  *target = (canFileMessages) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCreateSubfoldersNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool canCreateSubfolders;
  rv = folder->GetCanCreateSubfolders(&canCreateSubfolders);
  if (NS_FAILED(rv)) return rv;

  *target = (canCreateSubfolders) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanRenameNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  PRBool canRename;
  nsresult rv = folder->GetCanRename(&canRename);
  if (NS_FAILED(rv)) return rv;

  *target = (canRename) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanCompactNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  PRBool canCompact;
  nsresult rv = folder->GetCanCompact(&canCompact);
  if (NS_FAILED(rv)) return rv;

  *target = (canCompact) ? kTrueLiteral : kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createTotalMessagesNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{

  PRBool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
  
  PRInt32 totalMessages;
  if(isServer)
    totalMessages = kDisplayBlankCount;
  else
  {
    rv = folder->GetTotalMessages(PR_FALSE, &totalMessages);
    if(NS_FAILED(rv)) return rv;
  }
  GetNumMessagesNode(totalMessages, target);
  
  return rv;
}

nsresult
nsMsgFolderDataSource::createFolderSizeNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  PRBool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRInt32 folderSize;
  if(isServer)
    folderSize = kDisplayBlankCount;
  else
  {
    // XXX todo, we are asserting here for news
    // for offline news, we'd know the size on disk, right?
    rv = folder->GetSizeOnDisk((PRUint32 *) &folderSize);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  GetFolderSizeNode(folderSize, target);
  
  return rv;
}

nsresult
nsMsgFolderDataSource::createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  nsXPIDLCString charset;
  nsresult rv = folder->GetCharset(getter_Copies(charset));
  if (NS_SUCCEEDED(rv))
    createNode(NS_ConvertASCIItoUCS2(charset).get(), target, getRDFService());
  else
    createNode(EmptyString().get(), target, getRDFService());
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createBiffStateNodeFromFolder(nsIMsgFolder *folder, nsIRDFNode **target)
{
  PRUint32 biffState;
  nsresult rv = folder->GetBiffState(&biffState);
  if(NS_FAILED(rv)) return rv;

  rv = createBiffStateNodeFromFlag(biffState, target);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createBiffStateNodeFromFlag(PRUint32 flag, nsIRDFNode **target)
{
  const PRUnichar *biffStateStr;

  switch (flag) {
    case nsIMsgFolder::nsMsgBiffState_NewMail:
      biffStateStr = NS_LITERAL_STRING("NewMail").get();
      break;
    case nsIMsgFolder::nsMsgBiffState_NoMail:
      biffStateStr = NS_LITERAL_STRING("NoMail").get();
      break;
    default:
      biffStateStr = NS_LITERAL_STRING("UnknownMail").get();
      break;
  }

  createNode(biffStateStr, target, getRDFService());
  return NS_OK;
}

nsresult 
nsMsgFolderDataSource::createUnreadMessagesNode(nsIMsgFolder *folder,
												nsIRDFNode **target)
{
  PRBool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
  
  PRInt32 totalUnreadMessages;
  if(isServer)
    totalUnreadMessages = kDisplayBlankCount;
  else
  {
    rv = folder->GetNumUnread(PR_FALSE, &totalUnreadMessages);
    if(NS_FAILED(rv)) return rv;
  }
  GetNumMessagesNode(totalUnreadMessages, target);
  
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createHasUnreadMessagesNode(nsIMsgFolder *folder, PRBool aIncludeSubfolders, nsIRDFNode **target)
{
  PRBool isServer;
  nsresult rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  *target = kFalseLiteral;

  PRInt32 totalUnreadMessages;
  if(!isServer)
  {
    rv = folder->GetNumUnread(aIncludeSubfolders, &totalUnreadMessages);
    if(NS_FAILED(rv)) return rv;
    // if we're including sub-folders, we're trying to find out if child folders
    // have unread. If so, we subtract the unread msgs in the current folder.
    if (aIncludeSubfolders)
    {
      PRInt32 numUnreadInFolder;
      rv = folder->GetNumUnread(PR_FALSE, &numUnreadInFolder);
      NS_ENSURE_SUCCESS(rv, rv);
      // don't subtract if numUnread is negative (which means we don't know the unread count)
      if (numUnreadInFolder > 0) 
        totalUnreadMessages -= numUnreadInFolder;
    }
    *target = (totalUnreadMessages > 0) ? kTrueLiteral : kFalseLiteral;
  }

  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::OnUnreadMessagePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue)
{
  nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(folderResource);
  if(folder)
  {
    //First send a regular unread message changed notification
    nsCOMPtr<nsIRDFNode> newNode;

    GetNumMessagesNode(newValue, getter_AddRefs(newNode));
    NotifyPropertyChanged(folderResource, kNC_TotalUnreadMessages, newNode);
	
    //Now see if hasUnreadMessages has changed
    if(oldValue <=0 && newValue >0)
    {
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, kTrueLiteral);
      NotifyAncestors(folder, kNC_SubfoldersHaveUnreadMessages, kTrueLiteral);
    }
    else if(oldValue > 0 && newValue <= 0)
    {
      NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, kFalseLiteral);
      NotifyAncestors(folder, kNC_SubfoldersHaveUnreadMessages, kFalseLiteral);
    }

    //We will have to change the folderTreeName if the unread column is hidden
    NotifyFolderTreeNameChanged(folder, folderResource, newValue);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderNameChanged(nsIMsgFolder* aFolder, nsIRDFResource *folderResource)
{
  nsXPIDLString name;
  nsresult rv = aFolder->GetName(getter_Copies(name));

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(name.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(folderResource, kNC_Name, newNameNode);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderTreeSimpleNameChanged(nsIMsgFolder* aFolder, nsIRDFResource *folderResource)
{
  nsXPIDLString abbreviatedName;
  nsresult rv = aFolder->GetAbbreviatedName(getter_Copies(abbreviatedName));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(abbreviatedName.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(folderResource, kNC_FolderTreeSimpleName, newNameNode);
  }

  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderTreeNameChanged(nsIMsgFolder* aFolder,
                                                   nsIRDFResource* aFolderResource,
                                                   PRInt32 aUnreadMessages)
{
  nsXPIDLString name;
  nsresult rv = aFolder->GetAbbreviatedName(getter_Copies(name));
  if (NS_SUCCEEDED(rv)) {
    nsAutoString newNameString(name);
			
    CreateUnreadMessagesNameString(aUnreadMessages, newNameString);	
			
    nsCOMPtr<nsIRDFNode> newNameNode;
    createNode(newNameString.get(), getter_AddRefs(newNameNode), getRDFService());
    NotifyPropertyChanged(aFolderResource, kNC_FolderTreeName, newNameNode);
  }
  return NS_OK;
}

nsresult 
nsMsgFolderDataSource::NotifyAncestors(nsIMsgFolder *aFolder, nsIRDFResource *aPropertyResource, nsIRDFNode *aNode)
{
  PRBool isServer = PR_FALSE;
  nsresult rv = aFolder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);
 
  if (isServer)
    // done, stop
    return NS_OK;
  
  nsCOMPtr <nsIMsgFolder> parentMsgFolder;
  rv = aFolder->GetParentMsgFolder(getter_AddRefs(parentMsgFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!parentMsgFolder)
    return NS_OK;

  rv = parentMsgFolder->GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);
 
  // don't need to notify servers either.
  if (isServer) 
    // done, stop
    return NS_OK;

  nsCOMPtr<nsIRDFResource> parentFolderResource = do_QueryInterface(parentMsgFolder,&rv);
  NS_ENSURE_SUCCESS(rv,rv);

  NotifyPropertyChanged(parentFolderResource, aPropertyResource, aNode);

  return NotifyAncestors(parentMsgFolder, aPropertyResource, aNode);
}

// New Messages

nsresult
nsMsgFolderDataSource::createNewMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
  
  nsresult rv;
  
  PRBool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;
  
  *target = kFalseLiteral;
  
  //PRInt32 totalNewMessages;
  PRBool isNewMessages;
  if(!isServer)
  {
    rv = folder->GetHasNewMessages(&isNewMessages);
    if(NS_FAILED(rv)) return rv;
    *target = (isNewMessages) ? kTrueLiteral : kFalseLiteral;
  }
  NS_IF_ADDREF(*target);
  return NS_OK;
}

/**
nsresult
nsMsgFolderDataSource::OnUnreadMessagePropertyChanged(nsIMsgFolder *folder, PRInt32 oldValue, PRInt32 newValue)
{
	nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(folder);
	if(folderResource)
	{
		//First send a regular unread message changed notification
		nsCOMPtr<nsIRDFNode> newNode;

		GetNumMessagesNode(newValue, getter_AddRefs(newNode));
		NotifyPropertyChanged(folderResource, kNC_TotalUnreadMessages, newNode);
	
		//Now see if hasUnreadMessages has changed
		nsCOMPtr<nsIRDFNode> oldHasUnreadMessages;
		nsCOMPtr<nsIRDFNode> newHasUnreadMessages;
		if(oldValue <=0 && newValue >0)
		{
			oldHasUnreadMessages = kFalseLiteral;
			newHasUnreadMessages = kTrueLiteral;
			NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, newHasUnreadMessages);
		}
		else if(oldValue > 0 && newValue <= 0)
		{
			newHasUnreadMessages = kFalseLiteral;
			NotifyPropertyChanged(folderResource, kNC_HasUnreadMessages, newHasUnreadMessages);
		}
  }
  return NS_OK;
}

**/

nsresult
nsMsgFolderDataSource::OnFolderSizePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue)
{
  nsCOMPtr<nsIRDFNode> newNode;
  GetFolderSizeNode(newValue, getter_AddRefs(newNode));
  NotifyPropertyChanged(folderResource, kNC_FolderSize, newNode);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::OnTotalMessagePropertyChanged(nsIRDFResource *folderResource, PRInt32 oldValue, PRInt32 newValue)
{
  nsCOMPtr<nsIRDFNode> newNode;
  GetNumMessagesNode(newValue, getter_AddRefs(newNode));
  NotifyPropertyChanged(folderResource, kNC_TotalMessages, newNode);
  return NS_OK;
}

nsresult 
nsMsgFolderDataSource::GetNumMessagesNode(PRInt32 aNumMessages, nsIRDFNode **node)
{
  PRUint32 numMessages = aNumMessages;
  if(numMessages == kDisplayQuestionCount)
    createNode(NS_LITERAL_STRING("???").get(), node, getRDFService());
  else if (numMessages == kDisplayBlankCount || numMessages == 0)
    createNode(EmptyString().get(), node, getRDFService());
  else
    createIntNode(numMessages, node, getRDFService());
  return NS_OK;
}

nsresult 
nsMsgFolderDataSource::GetFolderSizeNode(PRInt32 aFolderSize, nsIRDFNode **aNode)
{
  PRUint32 folderSize = aFolderSize;
  if (folderSize == kDisplayBlankCount || folderSize == 0)
    createNode(EmptyString().get(), aNode, getRDFService());
  else if(folderSize == kDisplayQuestionCount)
    createNode(NS_LITERAL_STRING("???").get(), aNode, getRDFService());
  else
  {
    nsAutoString sizeString;
    if (folderSize < 1024)
      folderSize = 1024; // make at least 1 k;
    folderSize /= 1024;  // normalize into k;
    PRBool sizeInMB = (folderSize > 1024);
    sizeString.AppendInt((sizeInMB) ? folderSize / 1024 : folderSize);
    // On OS/2, we have an issue where temporaries get destructed in
    // conditionals. Solution is to break it out
    //
    // XXX todo
    // can we catch this problem at compile time?
    // see #179234
    if (sizeInMB)
      sizeString.AppendLiteral(" MB");
    else
      sizeString.AppendLiteral(" KB");
    createNode(sizeString.get(), aNode, getRDFService());
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderChildNode(nsIMsgFolder *folder,
                                             nsIRDFNode **target)
{
  nsCOMPtr<nsIEnumerator> subFolders;
  nsresult rv = folder->GetSubFolders(getter_AddRefs(subFolders));
  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;
  
  rv = subFolders->First();
  if (NS_SUCCEEDED(rv)) 
  {
    nsCOMPtr<nsISupports> firstFolder;
    rv = subFolders->CurrentItem(getter_AddRefs(firstFolder));
    if (NS_SUCCEEDED(rv)) 
      firstFolder->QueryInterface(NS_GET_IID(nsIRDFResource), (void**)target);
  }
  return NS_FAILED(rv) ? NS_RDF_NO_VALUE : rv;
}


nsresult nsMsgFolderDataSource::DoCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
											   nsIMsgWindow *msgWindow, PRBool isMove)
{
  nsresult rv;
  PRUint32 itemCount;
  rv = arguments->Count(&itemCount);
  if (NS_FAILED(rv)) return rv;
  
  //need source folder and at least one item to copy
  if(itemCount < 2)
    return NS_ERROR_FAILURE;
  
  
  nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryElementAt(arguments, 0));
  if(!srcFolder)
    return NS_ERROR_FAILURE;
  
  arguments->RemoveElementAt(0);
  itemCount--;
  
  nsCOMPtr<nsISupportsArray> messageArray;
  NS_NewISupportsArray(getter_AddRefs(messageArray));
  
  for(PRUint32 i = 0; i < itemCount; i++)
  {
    
    nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(i));
    nsCOMPtr<nsIMsgDBHdr> message(do_QueryInterface(supports));
    if (message)
    {
      messageArray->AppendElement(supports);
    }
    
  }
  
  //Call copyservice with dstFolder, srcFolder, messages, isMove, and txnManager
  nsCOMPtr<nsIMsgCopyService> copyService = 
    do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return copyService->CopyMessages(srcFolder, messageArray, dstFolder, isMove, 
    nsnull, msgWindow, PR_TRUE/* allowUndo */);
}

nsresult nsMsgFolderDataSource::DoFolderCopyToFolder(nsIMsgFolder *dstFolder, nsISupportsArray *arguments,
                                                     nsIMsgWindow *msgWindow, PRBool isMoveFolder)
{
  nsresult rv;
  PRUint32 itemCount;
  rv = arguments->Count(&itemCount);
  if (NS_FAILED(rv)) return rv;
  
  //need at least one item to copy
  if(itemCount < 1)
    return NS_ERROR_FAILURE;
  
  if (!isMoveFolder)   // copy folder not on the same server
  {
    //Call copyservice with dstFolder, srcFolder, folders and isMoveFolder
    nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv); 
    if(NS_SUCCEEDED(rv))
    {
      rv = copyService->CopyFolders(arguments, dstFolder, isMoveFolder, 
        nsnull, msgWindow);
      
    }
  }
  else    //within the same server therefore no need for copy service 
  {
    
    nsCOMPtr<nsIMsgFolder> msgFolder;
    for (PRUint32 i=0;i< itemCount; i++)
    {
      msgFolder = do_QueryElementAt(arguments, i, &rv);
      if (NS_SUCCEEDED(rv))
      {
        rv = dstFolder->CopyFolder(msgFolder, isMoveFolder , msgWindow, nsnull);
        NS_ASSERTION((NS_SUCCEEDED(rv)),"Copy folder failed.");
      }
    }
  }
  
  return rv;
  //return NS_OK;
}

nsresult nsMsgFolderDataSource::DoDeleteFromFolder(
                                                   nsIMsgFolder *folder, nsISupportsArray *arguments, 
                                                   nsIMsgWindow *msgWindow, PRBool reallyDelete)
{
  nsresult rv = NS_OK;
  PRUint32 itemCount;
  rv = arguments->Count(&itemCount);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISupportsArray> messageArray, folderArray;
  NS_NewISupportsArray(getter_AddRefs(messageArray));
  NS_NewISupportsArray(getter_AddRefs(folderArray));
  
  //Split up deleted items into different type arrays to be passed to the folder
  //for deletion.
  for(PRUint32 item = 0; item < itemCount; item++)
  {
    nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(item));
    nsCOMPtr<nsIMsgDBHdr> deletedMessage(do_QueryInterface(supports));
    nsCOMPtr<nsIMsgFolder> deletedFolder(do_QueryInterface(supports));
    if (deletedMessage)
    {
      messageArray->AppendElement(supports);
    }
    else if(deletedFolder)
    {
      folderArray->AppendElement(supports);
    }
  }
  PRUint32 cnt;
  rv = messageArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt > 0)
    rv = folder->DeleteMessages(messageArray, msgWindow, reallyDelete, PR_FALSE, nsnull, PR_TRUE /*allowUndo*/);
  
  rv = folderArray->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt > 0)
    rv = folder->DeleteSubFolders(folderArray, msgWindow);
  
  return rv;
}

nsresult nsMsgFolderDataSource::DoNewFolder(nsIMsgFolder *folder, nsISupportsArray *arguments)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFLiteral> literal = do_QueryElementAt(arguments, 0, &rv);
  if(NS_SUCCEEDED(rv))
  {
    nsXPIDLString name;
    literal->GetValue(getter_Copies(name));
    
    rv = folder->CreateSubfolder(name,mWindow);
    
  }
  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderAssert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target)
{
  nsresult rv = NS_ERROR_FAILURE;

  if((kNC_Charset == property))
  {
    nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(target));
    if(literal)
    {
      const PRUnichar* value;
      rv = literal->GetValueConst(&value);
      if(NS_SUCCEEDED(rv))
        rv = folder->SetCharset(NS_LossyConvertUCS2toASCII(value).get());
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  else if (kNC_Open == property && target == kTrueLiteral)
      rv = folder->ClearFlag(MSG_FOLDER_FLAG_ELIDED);

  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderUnassert(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target)
{
  nsresult rv = NS_ERROR_FAILURE;

  if((kNC_Open == property) && target == kTrueLiteral)
      rv = folder->SetFlag(MSG_FOLDER_FLAG_ELIDED);

  return rv;
}

nsresult nsMsgFolderDataSource::DoFolderHasAssertion(nsIMsgFolder *folder,
                                                     nsIRDFResource *property,
                                                     nsIRDFNode *target,
                                                     PRBool tv,
                                                     PRBool *hasAssertion)
{
  nsresult rv = NS_OK;
  if(!hasAssertion)
    return NS_ERROR_NULL_POINTER;
  
  //We're not keeping track of negative assertions on folders.
  if(!tv)
  {
    *hasAssertion = PR_FALSE;
    return NS_OK;
  }
  
  if((kNC_Child == property))
  {
    nsCOMPtr<nsIMsgFolder> childFolder(do_QueryInterface(target, &rv));
    if(NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgFolder> childsParent;
      rv = childFolder->GetParent(getter_AddRefs(childsParent));
      *hasAssertion = (NS_SUCCEEDED(rv) && childsParent && folder
        && (childsParent.get() == folder));
    }
  }
  else if ((kNC_Name == property) ||
    (kNC_Open == property) ||
    (kNC_FolderTreeName == property) ||
    (kNC_FolderTreeSimpleName == property) ||
    (kNC_SpecialFolder == property) ||
    (kNC_ServerType == property) ||
    (kNC_IsDeferred == property) ||
    (kNC_RedirectorType == property) ||
    (kNC_CanCreateFoldersOnServer == property) ||
    (kNC_CanFileMessagesOnServer == property) ||
    (kNC_IsServer == property) ||
    (kNC_IsSecure == property) ||
    (kNC_CanSubscribe == property) ||
    (kNC_SupportsOffline == property) ||
    (kNC_CanFileMessages == property) ||
    (kNC_CanCreateSubfolders == property) ||
    (kNC_CanRename == property) ||
    (kNC_CanCompact == property) ||
    (kNC_TotalMessages == property) ||
    (kNC_TotalUnreadMessages == property) ||
    (kNC_FolderSize == property) ||
    (kNC_Charset == property) ||
    (kNC_BiffState == property) ||
    (kNC_HasUnreadMessages == property) ||
    (kNC_NoSelect == property)  ||
    (kNC_Synchronize == property) ||
    (kNC_SyncDisabled == property) ||
    (kNC_CanSearchMessages == property))
  {
    nsCOMPtr<nsIRDFResource> folderResource(do_QueryInterface(folder, &rv));
    
    if(NS_FAILED(rv))
      return rv;
    
    rv = GetTargetHasAssertion(this, folderResource, property, tv, target, hasAssertion);
  }
  else 
    *hasAssertion = PR_FALSE;
  
  return rv;
  
  
}
