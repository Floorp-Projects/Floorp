/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "nsMsgFolderDataSource.h"
#include "nsMsgFolderFlags.h"

#include "nsMsgRDFUtils.h"
#include "nsIMessage.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgCopyService.h"
#include "nsMsgBaseCID.h"
#include "nsIInputStream.h"

#include "nsMessageViewDataSource.h"

#include "nsTraceRefcnt.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,		NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);

nsIRDFResource* nsMsgFolderDataSource::kNC_Child = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Folder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Name= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeName= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NameSort= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_FolderTreeNameSort= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SpecialFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_ServerType = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsServer = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_IsSecure = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanSubscribe = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanFileMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanCreateSubfolders = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_CanRename = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_TotalUnreadMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Charset = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_BiffState = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_HasUnreadMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_SubfoldersHaveUnreadMessages = nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NoSelect = nsnull;

// commands
nsIRDFResource* nsMsgFolderDataSource::kNC_Delete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_ReallyDelete= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_NewFolder= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_GetNewMessages= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Copy= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Move= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_MarkAllMessagesRead= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Compact= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_Rename= nsnull;
nsIRDFResource* nsMsgFolderDataSource::kNC_EmptyTrash= nsnull;

nsrefcnt nsMsgFolderDataSource::gFolderResourceRefCnt = 0;

nsIAtom * nsMsgFolderDataSource::kBiffStateAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kNewMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kTotalMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kTotalUnreadMessagesAtom = nsnull;
nsIAtom * nsMsgFolderDataSource::kNameAtom = nsnull;

nsMsgFolderDataSource::nsMsgFolderDataSource()
{
  // one-time initialization here
  nsIRDFService* rdf = getRDFService();
  
  if (gFolderResourceRefCnt++ == 0) {
    rdf->GetResource(NC_RDF_CHILD,   &kNC_Child);
    rdf->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
    rdf->GetResource(NC_RDF_FOLDER,  &kNC_Folder);
    rdf->GetResource(NC_RDF_NAME,    &kNC_Name);
    rdf->GetResource(NC_RDF_FOLDERTREENAME,    &kNC_FolderTreeName);
    rdf->GetResource(NC_RDF_NAME_SORT,    &kNC_NameSort);
    rdf->GetResource(NC_RDF_FOLDERTREENAME_SORT,    &kNC_FolderTreeNameSort);
    rdf->GetResource(NC_RDF_SPECIALFOLDER, &kNC_SpecialFolder);
    rdf->GetResource(NC_RDF_SERVERTYPE, &kNC_ServerType);
    rdf->GetResource(NC_RDF_ISSERVER, &kNC_IsServer);
    rdf->GetResource(NC_RDF_ISSECURE, &kNC_IsSecure);
    rdf->GetResource(NC_RDF_CANSUBSCRIBE, &kNC_CanSubscribe);
    rdf->GetResource(NC_RDF_CANFILEMESSAGES, &kNC_CanFileMessages);
    rdf->GetResource(NC_RDF_CANCREATESUBFOLDERS, &kNC_CanCreateSubfolders);
    rdf->GetResource(NC_RDF_CANRENAME, &kNC_CanRename);
    rdf->GetResource(NC_RDF_TOTALMESSAGES, &kNC_TotalMessages);
    rdf->GetResource(NC_RDF_TOTALUNREADMESSAGES, &kNC_TotalUnreadMessages);
    rdf->GetResource(NC_RDF_CHARSET, &kNC_Charset);
    rdf->GetResource(NC_RDF_BIFFSTATE, &kNC_BiffState);
    rdf->GetResource(NC_RDF_HASUNREADMESSAGES, &kNC_HasUnreadMessages);
    rdf->GetResource(NC_RDF_NEWMESSAGES, &kNC_NewMessages);
    rdf->GetResource(NC_RDF_SUBFOLDERSHAVEUNREADMESSAGES, &kNC_SubfoldersHaveUnreadMessages);
    rdf->GetResource(NC_RDF_NOSELECT, &kNC_NoSelect);
    
    rdf->GetResource(NC_RDF_DELETE, &kNC_Delete);
    rdf->GetResource(NC_RDF_REALLY_DELETE, &kNC_ReallyDelete);
    rdf->GetResource(NC_RDF_NEWFOLDER, &kNC_NewFolder);
    rdf->GetResource(NC_RDF_GETNEWMESSAGES, &kNC_GetNewMessages);
    rdf->GetResource(NC_RDF_COPY, &kNC_Copy);
    rdf->GetResource(NC_RDF_MOVE, &kNC_Move);
    rdf->GetResource(NC_RDF_MARKALLMESSAGESREAD,
                             &kNC_MarkAllMessagesRead);
    rdf->GetResource(NC_RDF_COMPACT, &kNC_Compact);
    rdf->GetResource(NC_RDF_RENAME, &kNC_Rename);
    rdf->GetResource(NC_RDF_EMPTYTRASH, &kNC_EmptyTrash);

    kTotalMessagesAtom           = NS_NewAtom("TotalMessages");
    kTotalUnreadMessagesAtom     = NS_NewAtom("TotalUnreadMessages");
    kBiffStateAtom               = NS_NewAtom("BiffState");
    kNewMessagesAtom               = NS_NewAtom("NewMessages");
    kNameAtom              = NS_NewAtom("Name");
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
		NS_RELEASE2(kNC_MessageChild, refcnt);
		NS_RELEASE2(kNC_Folder, refcnt);
		NS_RELEASE2(kNC_Name, refcnt);
		NS_RELEASE2(kNC_FolderTreeName, refcnt);
		NS_RELEASE2(kNC_NameSort, refcnt);
		NS_RELEASE2(kNC_FolderTreeNameSort, refcnt);
		NS_RELEASE2(kNC_SpecialFolder, refcnt);
		NS_RELEASE2(kNC_ServerType, refcnt);
		NS_RELEASE2(kNC_IsServer, refcnt);
		NS_RELEASE2(kNC_IsSecure, refcnt);
		NS_RELEASE2(kNC_CanSubscribe, refcnt);
		NS_RELEASE2(kNC_CanFileMessages, refcnt);
		NS_RELEASE2(kNC_CanCreateSubfolders, refcnt);
		NS_RELEASE2(kNC_CanRename, refcnt);
		NS_RELEASE2(kNC_TotalMessages, refcnt);
		NS_RELEASE2(kNC_TotalUnreadMessages, refcnt);
		NS_RELEASE2(kNC_Charset, refcnt);
		NS_RELEASE2(kNC_BiffState, refcnt);
		NS_RELEASE2(kNC_HasUnreadMessages, refcnt);
		NS_RELEASE2(kNC_NewMessages, refcnt);
		NS_RELEASE2(kNC_SubfoldersHaveUnreadMessages, refcnt);
    NS_RELEASE2(kNC_NoSelect, refcnt);

		NS_RELEASE2(kNC_Delete, refcnt);
		NS_RELEASE2(kNC_ReallyDelete, refcnt);
		NS_RELEASE2(kNC_NewFolder, refcnt);
		NS_RELEASE2(kNC_GetNewMessages, refcnt);
		NS_RELEASE2(kNC_Copy, refcnt);
		NS_RELEASE2(kNC_Move, refcnt);
		NS_RELEASE2(kNC_MarkAllMessagesRead, refcnt);
		NS_RELEASE2(kNC_Compact, refcnt);
		NS_RELEASE2(kNC_Rename, refcnt);
		NS_RELEASE2(kNC_EmptyTrash, refcnt);

    NS_RELEASE(kTotalMessagesAtom);
    NS_RELEASE(kTotalUnreadMessagesAtom);
    NS_RELEASE(kBiffStateAtom);
    NS_RELEASE(kNewMessagesAtom);
    NS_RELEASE(kNameAtom);
	}

}

nsresult nsMsgFolderDataSource::CreateLiterals(nsIRDFService *rdf)
{
	createNode((const PRUnichar*)NS_LITERAL_STRING("true").get(),
             getter_AddRefs(kTrueLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("false").get(),
             getter_AddRefs(kFalseLiteral), rdf);
  
	return NS_OK;
}

nsresult nsMsgFolderDataSource::Init()
{
  nsresult rv;
  
	rv = nsMsgRDFDataSource::Init();
  if (NS_FAILED(rv))
    return rv;

  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);

  return NS_OK;
}

void nsMsgFolderDataSource::Cleanup()
{
  nsresult rv;
  if (!m_shuttingDown)
  {
    nsCOMPtr<nsIMsgMailSession> mailSession =
      do_GetService(kMsgMailSessionCID, &rv);
    
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
  if ((*uri = nsXPIDLCString::Copy("rdf:mailnewsfolders")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgFolderDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
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
  if (folder) {
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
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
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
             (kNC_FolderTreeName == property) ||
             (kNC_SpecialFolder == property) ||
             (kNC_IsServer == property) ||
             (kNC_IsSecure == property) ||
             (kNC_CanSubscribe == property) ||
             (kNC_CanFileMessages == property) ||
             (kNC_CanCreateSubfolders == property) ||
             (kNC_CanRename == property) ||
             (kNC_ServerType == property) ||
             (kNC_NoSelect == property) )
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
	if (NS_SUCCEEDED(rv)) {
    *result = (aArc == kNC_Name ||
               aArc == kNC_FolderTreeName ||
               aArc == kNC_SpecialFolder ||
               aArc == kNC_ServerType ||
               aArc == kNC_IsServer ||
               aArc == kNC_IsSecure ||
               aArc == kNC_CanSubscribe ||
               aArc == kNC_CanFileMessages ||
               aArc == kNC_CanCreateSubfolders ||
               aArc == kNC_CanRename ||
               aArc == kNC_TotalMessages ||
               aArc == kNC_TotalUnreadMessages ||
               aArc == kNC_Charset ||
               aArc == kNC_BiffState ||
               aArc == kNC_Child ||
               aArc == kNC_NoSelect);
	}
	else {
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
	if (NS_SUCCEEDED(rv)) {
		arcsArray = kFolderArcsOutArray;

    rv = NS_NewArrayEnumerator(labels, arcsArray);
	}
	else {
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
  (*arcs)->AppendElement(kNC_FolderTreeName);
  (*arcs)->AppendElement(kNC_SpecialFolder);
  (*arcs)->AppendElement(kNC_ServerType);
  (*arcs)->AppendElement(kNC_IsServer);
  (*arcs)->AppendElement(kNC_IsSecure);
  (*arcs)->AppendElement(kNC_CanSubscribe);
  (*arcs)->AppendElement(kNC_CanFileMessages);
  (*arcs)->AppendElement(kNC_CanCreateSubfolders);
  (*arcs)->AppendElement(kNC_CanRename);
  (*arcs)->AppendElement(kNC_TotalMessages);
  (*arcs)->AppendElement(kNC_TotalUnreadMessages);
  (*arcs)->AppendElement(kNC_Charset);
  (*arcs)->AppendElement(kNC_BiffState);
  (*arcs)->AppendElement(kNC_Child);
  (*arcs)->AppendElement(kNC_NoSelect);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> cmds;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewISupportsArray(getter_AddRefs(cmds));
    if (NS_FAILED(rv)) return rv;
    cmds->AppendElement(kNC_Delete);
    cmds->AppendElement(kNC_ReallyDelete);
    cmds->AppendElement(kNC_NewFolder);
    cmds->AppendElement(kNC_GetNewMessages);
    cmds->AppendElement(kNC_Copy);
    cmds->AppendElement(kNC_Move);
    cmds->AppendElement(kNC_MarkAllMessagesRead);
    cmds->AppendElement(kNC_Compact);
    cmds->AppendElement(kNC_Rename);
    cmds->AppendElement(kNC_EmptyTrash);
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgFolderDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = getter_AddRefs(aSources->ElementAt(i));
		folder = do_QueryInterface(source, &rv);
    if (NS_SUCCEEDED(rv)) {
      // we don't care about the arguments -- folder commands are always enabled
      if (!((aCommand == kNC_Delete) ||
            (aCommand == kNC_ReallyDelete) ||
            (aCommand == kNC_NewFolder) ||
            (aCommand == kNC_Copy) ||
            (aCommand == kNC_Move) ||
            (aCommand == kNC_GetNewMessages) ||
            (aCommand == kNC_MarkAllMessagesRead) ||
            (aCommand == kNC_Compact) || 
            (aCommand == kNC_Rename) ||
            (aCommand == kNC_EmptyTrash) )) 
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

  for ( ; i < cnt; i++) {
    supports  = getter_AddRefs(aSources->ElementAt(i));
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
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
        rv = folder->GetNewMessages(mWindow);
      }
      else if((aCommand == kNC_Copy))
      {
        rv = DoCopyToFolder(folder, aArguments, mWindow, PR_FALSE);
      }
      else if((aCommand == kNC_Move))
      {
        rv = DoCopyToFolder(folder, aArguments, mWindow, PR_TRUE);
      }
      else if((aCommand == kNC_MarkAllMessagesRead))
      {
        rv = folder->MarkAllMessagesRead();
      }
      else if ((aCommand == kNC_Compact))
      {
        rv = folder->Compact();
      }
      else if ((aCommand == kNC_EmptyTrash))
      {
          rv = folder->EmptyTrash(mWindow, nsnull);
      }
      else if ((aCommand == kNC_Rename))
      {
        nsCOMPtr<nsISupports> elem = getter_AddRefs(aArguments->ElementAt(0));
        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(elem, &rv);
        if(NS_SUCCEEDED(rv))
		{
		  PRUnichar *name;
		  literal->GetValue(&name);

          rv = folder->Rename(name);
          PR_FREEIF(name);
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

NS_IMETHODIMP nsMsgFolderDataSource::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	return OnItemAddedOrRemoved(parentItem, item, viewString, PR_TRUE);
}

NS_IMETHODIMP nsMsgFolderDataSource::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char* viewString)
{
	return OnItemAddedOrRemoved(parentItem, item, viewString, PR_FALSE);
}


nsresult nsMsgFolderDataSource::OnItemAddedOrRemoved(nsISupports *parentItem, nsISupports *item, const char* viewString, PRBool added)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> parentResource;
	nsCOMPtr<nsIMsgFolder> parentFolder;
	nsCOMPtr<nsIMsgFolder> folder;

	parentFolder = do_QueryInterface(parentItem);
	//If the parent isn't a folder then we don't handle it.
	if(!parentFolder)
		return NS_OK;

	parentResource = do_QueryInterface(parentItem);
	//If it's not a resource, we don't handle it either
	if(!parentResource)
		return NS_OK;

	//If we are doing this to a folder
	if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIMsgFolder), getter_AddRefs(folder))))
	{
		nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
		if(NS_SUCCEEDED(rv))
		{
			//Notify folders that a folder was added or deleted.
			NotifyObservers(parentResource, kNC_Child, itemNode, added, PR_FALSE);
		}
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyChanged(nsISupports *item,
                                             nsIAtom *property,
                                             const char *oldValue,
                                             const char *newValue)

{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemIntPropertyChanged(nsISupports *item,
                                                nsIAtom *property,
                                                PRInt32 oldValue,
                                                PRInt32 newValue)
{
	//We only care about folder changes
	nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(item);
	if(folder)
	{

		if (kTotalMessagesAtom == property)
		{
			OnTotalMessagePropertyChanged(folder, oldValue, newValue);
		}
		else if (kTotalUnreadMessagesAtom == property)
		{
			OnUnreadMessagePropertyChanged(folder, oldValue, newValue);
		}

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemUnicharPropertyChanged(nsISupports *item,
                                                    nsIAtom *property,
                                                    const PRUnichar *oldValue,
                                                    const PRUnichar *newValue)
{
  nsresult rv=NS_OK;

  if (kNameAtom == property) {
    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(item, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(item, &rv);
      if (NS_SUCCEEDED(rv)) {
        PRInt32 numUnread;
        folder->GetNumUnread(PR_FALSE, &numUnread);
        NotifyFolderTreeNameChanged(folder, numUnread);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemBoolPropertyChanged(nsISupports *item,
                                                 nsIAtom *property,
                                                 PRBool oldValue,
                                                 PRBool newValue)
{
    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item)); 
    if(folder) 
	{ 
		nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item)); 
		if(resource) 
		{ 
			if (kNewMessagesAtom == property) 
			{ 
				if (newValue != oldValue) {
					nsIRDFNode* newMessagesNode = newValue?kTrueLiteral:kFalseLiteral;
					NotifyPropertyChanged(resource, kNC_NewMessages, newMessagesNode); 
				}
			} 
		} 
	} 

	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemPropertyFlagChanged(nsISupports *item,
                                                 nsIAtom *property,
                                                 PRUint32 oldFlag,
                                                 PRUint32 newFlag)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(item));
	if(folder)
	{
		nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item));
		if(resource)
		{
			if (kBiffStateAtom == property)
			{
				nsCAutoString newBiffStateStr;

				rv = GetBiffStateString(newFlag, newBiffStateStr);
				if(NS_FAILED(rv))
					return rv;
				NotifyPropertyChanged(resource, kNC_BiffState, newBiffStateStr);
			}
		}

	}
	return NS_OK;

}

NS_IMETHODIMP
nsMsgFolderDataSource::OnItemEvent(nsIFolder *aFolder, nsIAtom *aEvent)
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
	rv = createFolderTreeNameNode(folder, target, PR_TRUE);
  else if (kNC_Name == property)
		rv = createFolderNameNode(folder, target, PR_FALSE);
  else if (kNC_FolderTreeName == property)
		rv = createFolderTreeNameNode(folder, target, PR_FALSE);
  else if ((kNC_SpecialFolder == property))
		rv = createFolderSpecialNode(folder,target);
  else if ((kNC_ServerType == property))
    rv = createFolderServerTypeNode(folder, target);
  else if ((kNC_IsServer == property))
    rv = createFolderIsServerNode(folder, target);
  else if ((kNC_IsSecure == property))
    rv = createFolderIsSecureNode(folder, target);
  else if ((kNC_CanSubscribe == property))
    rv = createFolderCanSubscribeNode(folder, target);
  else if ((kNC_CanFileMessages == property))
    rv = createFolderCanFileMessagesNode(folder, target);
  else if ((kNC_CanCreateSubfolders == property))
    rv = createFolderCanCreateSubfoldersNode(folder, target);
  else if ((kNC_CanRename == property))
    rv = createFolderCanRenameNode(folder, target);
	else if ((kNC_TotalMessages == property))
		rv = createTotalMessagesNode(folder, target);
	else if ((kNC_TotalUnreadMessages == property))
		rv = createUnreadMessagesNode(folder, target);
	else if ((kNC_Charset == property))
		rv = createCharsetNode(folder, target);
	else if ((kNC_BiffState == property))
		rv = createBiffStateNode(folder, target);
	else if ((kNC_HasUnreadMessages == property))
		rv = createHasUnreadMessagesNode(folder, target);
	else if ((kNC_NewMessages == property))
		rv = createNewMessagesNode(folder, target);
	else if ((kNC_SubfoldersHaveUnreadMessages == property))
		rv = createSubfoldersHaveUnreadMessagesNode(folder, target);
	else if ((kNC_Child == property))
		rv = createFolderChildNode(folder, target);
    else if ((kNC_NoSelect == property))
    rv = createFolderNoSelectNode(folder, target);

  if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;
  return rv;
}


nsresult nsMsgFolderDataSource::createFolderNameNode(nsIMsgFolder *folder,
                                                     nsIRDFNode **target, PRBool sort)
{

  nsXPIDLString name;
  nsresult rv = folder->GetName(getter_Copies(name));
  if (NS_FAILED(rv)) return rv;
  nsAutoString nameString(name);
	if(sort)
	{
		CreateNameSortString(folder, nameString);
	}
	createNode(nameString, target, getRDFService());
  return NS_OK;
}


nsresult nsMsgFolderDataSource::createFolderTreeNameNode(nsIMsgFolder *folder,
                                                     nsIRDFNode **target, PRBool sort)
{

	nsXPIDLString name;
	nsresult rv = folder->GetAbbreviatedName(getter_Copies(name));
	if (NS_FAILED(rv)) return rv;
	nsAutoString nameString(name);
	if(sort)
	{
		CreateNameSortString(folder, nameString);
	}
	else
	{
		PRInt32 unreadMessages;

		rv = folder->GetNumUnread(PR_FALSE, &unreadMessages);
		if(NS_SUCCEEDED(rv)) 
		{
			CreateUnreadMessagesNameString(unreadMessages, nameString);	
		}

	}
	createNode(nameString, target, getRDFService());
  return NS_OK;
}

nsresult nsMsgFolderDataSource::CreateNameSortString(nsIMsgFolder *folder, nsAutoString &name)
{
	PRInt32 order;
	nsresult rv = GetFolderSortOrder(folder, &order);
	if(NS_FAILED(rv))
		return rv;

	nsAutoString orderString;
	orderString.AppendInt(order);

	name.Insert(orderString, 0);

	return NS_OK;
}

nsresult nsMsgFolderDataSource::CreateUnreadMessagesNameString(PRInt32 unreadMessages, nsAutoString &nameString)
{
	//Only do this if unread messages are positive
	if(unreadMessages > 0)
	{
		nameString.AppendWithConversion(" (");
		nameString.AppendInt(unreadMessages);
		nameString.AppendWithConversion(')');
	}
	return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderSpecialNode(nsIMsgFolder *folder,
                                               nsIRDFNode **target)
{
  PRUint32 flags;
  nsresult rv = folder->GetFlags(&flags);
  if(NS_FAILED(rv)) return rv;
  
  nsAutoString specialFolderString;
  
  if(flags & MSG_FOLDER_FLAG_INBOX)
    specialFolderString.AssignWithConversion("Inbox");
  else if(flags & MSG_FOLDER_FLAG_TRASH)
    specialFolderString.AssignWithConversion("Trash");
  else if(flags & MSG_FOLDER_FLAG_QUEUE)
    specialFolderString.AssignWithConversion("Unsent Messages");
  else if(flags & MSG_FOLDER_FLAG_SENTMAIL)
    specialFolderString.AssignWithConversion("Sent");
  else if(flags & MSG_FOLDER_FLAG_DRAFTS)
    specialFolderString.AssignWithConversion("Drafts");
  else if(flags & MSG_FOLDER_FLAG_TEMPLATES)
    specialFolderString.AssignWithConversion("Templates");
  else
    specialFolderString.AssignWithConversion("none");
  
  createNode(specialFolderString, target, getRDFService());
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

  createNode(serverType, target, getRDFService());
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

  *target = nsnull;

  if (noSelect)
	*target = kTrueLiteral;
  else
    *target = kFalseLiteral;
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

  *target = nsnull;

  if (isSecure)
    *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
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

  *target = nsnull;

  if (canSubscribe)
        *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
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

  *target = nsnull;

  if (canFileMessages)
        *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
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

  *target = nsnull;

  if (canCreateSubfolders)
        *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::createFolderCanRenameNode(nsIMsgFolder* folder,
                                                  nsIRDFNode **target)
{
  nsresult rv;
  PRBool canRename;
  rv = folder->GetCanRename(&canRename);
  if (NS_FAILED(rv)) return rv;

  *target = nsnull;

  if (canRename)
        *target = kTrueLiteral;
  else
    *target = kFalseLiteral;
  NS_IF_ADDREF(*target);
  return NS_OK;
}


nsresult
nsMsgFolderDataSource::createTotalMessagesNode(nsIMsgFolder *folder,
											   nsIRDFNode **target)
{
	nsresult rv;


  PRBool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

	PRInt32 totalMessages;
	if(isServer)
		totalMessages = -2;
	else
	{
		rv = folder->GetTotalMessages(PR_FALSE, &totalMessages);
		if(NS_FAILED(rv)) return rv;
	}
	GetNumMessagesNode(totalMessages, target);

	return rv;
}

nsresult
nsMsgFolderDataSource::createCharsetNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
	PRUnichar *charset;
	nsAutoString charsetStr;
	nsresult rv = folder->GetCharset(&charset);
	//We always need to return a value
	if(NS_SUCCEEDED(rv))
		charsetStr = charset;
	else
		charsetStr.SetLength(0);
	createNode(charsetStr, target, getRDFService());
	return NS_OK;

}

nsresult
nsMsgFolderDataSource::createBiffStateNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 biffState;
	rv = folder->GetBiffState(&biffState);
	if(NS_FAILED(rv)) return rv;

	nsCAutoString biffString;
	GetBiffStateString(biffState, biffString);
	nsAutoString uniStr;uniStr.AssignWithConversion(biffString);
	createNode(uniStr, target, getRDFService());
	return NS_OK;
}

nsresult
nsMsgFolderDataSource::GetBiffStateString(PRUint32 biffState, nsCAutoString& biffStateStr)
{
	if(biffState == nsIMsgFolder::nsMsgBiffState_NewMail)
		biffStateStr = "NewMail";
	else if(biffState == nsIMsgFolder::nsMsgBiffState_NoMail)
		biffStateStr = "NoMail";
	else 
		biffStateStr = "UnknownMail";

	return NS_OK;
}

nsresult 
nsMsgFolderDataSource::createUnreadMessagesNode(nsIMsgFolder *folder,
												nsIRDFNode **target)
{
	nsresult rv;

  PRBool isServer;
  rv = folder->GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

	PRInt32 totalUnreadMessages;
	if(isServer)
		totalUnreadMessages = -2;
	else
	{
		rv = folder->GetNumUnread(PR_FALSE, &totalUnreadMessages);
		if(NS_FAILED(rv)) return rv;
	}
	GetNumMessagesNode(totalUnreadMessages, target);


	return NS_OK;
}

nsresult
nsMsgFolderDataSource::createHasUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target)
{

	nsresult rv;

	PRBool isServer;
	rv = folder->GetIsServer(&isServer);
	if (NS_FAILED(rv)) return rv;

	*target = kFalseLiteral;

	PRInt32 totalUnreadMessages;
	if(!isServer)
	{
		rv = folder->GetNumUnread(PR_FALSE, &totalUnreadMessages);
		if(NS_FAILED(rv)) return rv;
		if(totalUnreadMessages > 0)
			*target = kTrueLiteral;
		else
			*target = kFalseLiteral;
	}
	NS_IF_ADDREF(*target);
	return NS_OK;
}

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
		//We will have to change the folderTreeName also

    NotifyFolderTreeNameChanged(folder, newValue);
  }
  return NS_OK;
}

nsresult
nsMsgFolderDataSource::NotifyFolderTreeNameChanged(nsIMsgFolder* aFolder,
                                                   PRInt32 aUnreadMessages)
{
		nsXPIDLString name;
		nsresult rv = aFolder->GetAbbreviatedName(getter_Copies(name));
		if (NS_SUCCEEDED(rv)) 
		{
			nsAutoString newNameString(name);
			
			CreateUnreadMessagesNameString(aUnreadMessages, newNameString);	
			
			nsCOMPtr<nsIRDFNode> newNameNode;
			createNode(newNameString, getter_AddRefs(newNameNode), getRDFService());
      nsCOMPtr<nsIRDFResource> folderResource =
        do_QueryInterface(aFolder);
			NotifyPropertyChanged(folderResource, kNC_FolderTreeName, newNameNode);
		}

    return NS_OK;
}

// <<<<<<<<< >>>>>>>>>>>>>>>>..
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
		if(isNewMessages)
			*target = kTrueLiteral;
		else
			*target = kFalseLiteral;
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
nsMsgFolderDataSource::GetNewMessagesString(PRBool newMessages, nsCAutoString& newMessagesStr)
{
	if(newMessages)
		newMessagesStr = "true";
	else 
		newMessagesStr = "false";



	return NS_OK;
}
// <<<<<<<<< >>>>>>>>>>>>>>>>..


nsresult
nsMsgFolderDataSource::OnTotalMessagePropertyChanged(nsIMsgFolder *folder, PRInt32 oldValue, PRInt32 newValue)
{
	nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(folder);
	if(folderResource)
	{
		//First send a regular unread message changed notification
		nsCOMPtr<nsIRDFNode> newNode;

		GetNumMessagesNode(newValue, getter_AddRefs(newNode));
		NotifyPropertyChanged(folderResource, kNC_TotalMessages, newNode);
	}
	return NS_OK;
}

nsresult 
nsMsgFolderDataSource::GetNumMessagesNode(PRInt32 numMessages, nsIRDFNode **node)
{

	if(numMessages >0)
		createIntNode(numMessages, node, getRDFService());
	else if(numMessages == -1)
	{
		nsAutoString unknownMessages; unknownMessages.AssignWithConversion("???");
		createNode(unknownMessages, node, getRDFService());
	}
	else
	{
		nsAutoString unknownMessages;
		createNode(unknownMessages, node, getRDFService());
	}
	return NS_OK;
}

nsresult
nsMsgFolderDataSource::createSubfoldersHaveUnreadMessagesNode(nsIMsgFolder *folder, nsIRDFNode **target)
{
	nsAutoString unknownMessages; unknownMessages.AssignWithConversion("true");
	createNode(unknownMessages, target, getRDFService());

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
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISupports> firstFolder;
    rv = subFolders->CurrentItem(getter_AddRefs(firstFolder));
    if (NS_SUCCEEDED(rv)) {
      firstFolder->QueryInterface(NS_GET_IID(nsIRDFResource), (void**)target);
    }
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


	nsCOMPtr<nsISupports> srcFolderSupports = getter_AddRefs(arguments->ElementAt(0));
	nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryInterface(srcFolderSupports));
	if(!srcFolder)
		return NS_ERROR_FAILURE;

    arguments->RemoveElementAt(0);
    itemCount--;

	nsCOMPtr<nsISupportsArray> messageArray;
	NS_NewISupportsArray(getter_AddRefs(messageArray));

	for(PRUint32 i = 0; i < itemCount; i++)
	{

		nsCOMPtr<nsISupports> supports = getter_AddRefs(arguments->ElementAt(i));
		nsCOMPtr<nsIMessage> message(do_QueryInterface(supports));
		if (message)
		{
			messageArray->AppendElement(supports);
		}

	}
	//Call copyservice with dstFolder, srcFolder, messages, isMove, and txnManager
	NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		rv = copyService->CopyMessages(srcFolder, messageArray, dstFolder, isMove, 
                              nsnull, msgWindow);

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
		nsCOMPtr<nsIMessage> deletedMessage(do_QueryInterface(supports));
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
		rv = folder->DeleteMessages(messageArray, msgWindow, reallyDelete, PR_FALSE);

	rv = folderArray->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	if (cnt > 0)
		rv = folder->DeleteSubFolders(folderArray, msgWindow);

	return rv;
}

nsresult nsMsgFolderDataSource::DoNewFolder(nsIMsgFolder *folder, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsISupports> elem = getter_AddRefs(arguments->ElementAt(0));
	nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(elem, &rv);
	if(NS_SUCCEEDED(rv))
	{
		PRUnichar *name;
		literal->GetValue(&name);

		rv = folder->CreateSubfolder(name);
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
			PRUnichar *value;
			rv = literal->GetValue(&value);
			if(NS_SUCCEEDED(rv))
			{
				rv = folder->SetCharset(value);
				nsMemory::Free(value);
			}
		}
		else
			rv = NS_ERROR_FAILURE;
	}

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
		nsCOMPtr<nsIFolder> childFolder(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIFolder> folderasFolder(do_QueryInterface(folder));
			nsCOMPtr<nsIFolder> childsParent;
			rv = childFolder->GetParent(getter_AddRefs(childsParent));
			*hasAssertion = (NS_SUCCEEDED(rv) && childsParent && folderasFolder
							&& (childsParent.get() == folderasFolder.get()));
		}
	}
	else if ((kNC_Name == property) ||
			(kNC_FolderTreeName == property) ||
           (kNC_SpecialFolder == property) ||
           (kNC_ServerType == property) ||
           (kNC_IsServer == property) ||
           (kNC_IsSecure == property) ||
           (kNC_CanSubscribe == property) ||
           (kNC_CanFileMessages == property) ||
           (kNC_CanCreateSubfolders == property) ||
           (kNC_CanRename == property) ||
           (kNC_TotalMessages == property) ||
           (kNC_TotalUnreadMessages == property) ||
           (kNC_Charset == property) ||
           (kNC_BiffState == property) ||
           (kNC_HasUnreadMessages == property) ||
           (kNC_NoSelect == property) )
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


nsresult nsMsgFolderDataSource::GetFolderSortOrder(nsIMsgFolder *folder, PRInt32* order)
{
	nsresult rv;

  PRUint32 flags;
  rv = folder->GetFlags(&flags);
  if(NS_FAILED(rv)) return rv;
  
  
  if(flags & MSG_FOLDER_FLAG_INBOX)
    *order = 0;
  else if(flags & MSG_FOLDER_FLAG_QUEUE)
    *order = 1;
  else if(flags & MSG_FOLDER_FLAG_DRAFTS)
    *order = 2;
  else if(flags & MSG_FOLDER_FLAG_TEMPLATES)
    *order = 3;
  else if(flags & MSG_FOLDER_FLAG_SENTMAIL)
    *order = 4;
  else if(flags & MSG_FOLDER_FLAG_TRASH)
    *order = 5;
  else
    *order = 6;

	return NS_OK;

}

