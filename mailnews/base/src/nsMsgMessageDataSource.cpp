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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "nsMsgMessageDataSource.h"
#include "nsMsgRDFUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "nsEnumeratorUtils.h"
#include "nsIMessage.h"
#include "nsIMsgFolder.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIMsgMailSession.h"
#include "nsDateTimeFormatCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMessageView.h"
#include "nsMsgUtils.h"
#include "nsMessageViewDataSource.h"
#include "nsTextFormatter.h"


static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kMsgMailSessionCID,		NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kDateTimeFormatCID,		NS_DATETIMEFORMAT_CID);

nsIRDFResource* nsMsgMessageDataSource::kNC_Subject = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_SubjectCollation = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Sender= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_SenderCollation = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Date= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Status= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Flagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Priority= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Size= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Total = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Unread = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_IsUnread = nsnull;


//commands
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnread= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_ToggleRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkFlagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnflagged= nsnull;

nsrefcnt nsMsgMessageDataSource::gMessageResourceRefCnt = 0;

nsIAtom * nsMsgMessageDataSource::kFlaggedAtom  = nsnull;
nsIAtom * nsMsgMessageDataSource::kStatusAtom   = nsnull;



nsMsgMessageDataSource::nsMsgMessageDataSource():
  mInitialized(PR_FALSE),
  mHeaderParser(nsnull)
{

}

nsMsgMessageDataSource::~nsMsgMessageDataSource (void)
{
	nsresult rv;

	if (!m_shuttingDown)
	{
		NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 

		if(NS_SUCCEEDED(rv))
			mailSession->RemoveFolderListener(this);
	}
	if (--gMessageResourceRefCnt == 0)
	{
		nsrefcnt refcnt;

		NS_RELEASE2(kNC_Subject, refcnt);
		NS_RELEASE2(kNC_SubjectCollation, refcnt);
		NS_RELEASE2(kNC_Sender, refcnt);
		NS_RELEASE2(kNC_SenderCollation, refcnt);
		NS_RELEASE2(kNC_Date, refcnt);
		NS_RELEASE2(kNC_Status, refcnt);
		NS_RELEASE2(kNC_Flagged, refcnt);
		NS_RELEASE2(kNC_Priority, refcnt);
		NS_RELEASE2(kNC_Size, refcnt);
		NS_RELEASE2(kNC_Total, refcnt);
		NS_RELEASE2(kNC_Unread, refcnt);
		NS_RELEASE2(kNC_MessageChild, refcnt);
		NS_RELEASE2(kNC_IsUnread, refcnt);

		NS_RELEASE2(kNC_MarkRead, refcnt);
		NS_RELEASE2(kNC_MarkUnread, refcnt);
		NS_RELEASE2(kNC_ToggleRead, refcnt);
		NS_RELEASE2(kNC_MarkFlagged, refcnt);
		NS_RELEASE2(kNC_MarkUnflagged, refcnt);

    NS_RELEASE(kStatusAtom);
    NS_RELEASE(kFlaggedAtom);
	}

	NS_IF_RELEASE(mHeaderParser);
}

nsresult nsMsgMessageDataSource::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;


	nsresult rv;
	nsIRDFService *rdf = getRDFService();
	if(!rdf)
		return NS_ERROR_FAILURE;

	rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                          NULL, 
                                          NS_GET_IID(nsIMsgHeaderParser), 
                                          (void **) &mHeaderParser);

	if(NS_FAILED(rv))
		return rv;

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);
	PR_ASSERT(NS_SUCCEEDED(rv));
	if (gMessageResourceRefCnt++ == 0) {
    
		rdf->GetResource(NC_RDF_SUBJECT, &kNC_Subject);
		rdf->GetResource(NC_RDF_SUBJECT_COLLATION_SORT, &kNC_SubjectCollation);
		rdf->GetResource(NC_RDF_SENDER, &kNC_Sender);
		rdf->GetResource(NC_RDF_SENDER_COLLATION_SORT, &kNC_SenderCollation);
		rdf->GetResource(NC_RDF_DATE, &kNC_Date);
		rdf->GetResource(NC_RDF_STATUS, &kNC_Status);
		rdf->GetResource(NC_RDF_FLAGGED, &kNC_Flagged);
		rdf->GetResource(NC_RDF_PRIORITY, &kNC_Priority);
		rdf->GetResource(NC_RDF_SIZE, &kNC_Size);
		rdf->GetResource(NC_RDF_TOTALMESSAGES,   &kNC_Total);
		rdf->GetResource(NC_RDF_TOTALUNREADMESSAGES,   &kNC_Unread);
		rdf->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
		rdf->GetResource(NC_RDF_ISUNREAD, &kNC_IsUnread);

		rdf->GetResource(NC_RDF_MARKREAD, &kNC_MarkRead);
		rdf->GetResource(NC_RDF_MARKUNREAD, &kNC_MarkUnread);
		rdf->GetResource(NC_RDF_TOGGLEREAD, &kNC_ToggleRead);
		rdf->GetResource(NC_RDF_MARKFLAGGED, &kNC_MarkFlagged);
		rdf->GetResource(NC_RDF_MARKUNFLAGGED, &kNC_MarkUnflagged);

    kStatusAtom = NS_NewAtom("Status");
    kFlaggedAtom = NS_NewAtom("Flagged");
	}

	CreateLiterals(rdf);
	rv = CreateArcsOutEnumerators();

	if(NS_FAILED(rv)) return rv;

	mInitialized = PR_TRUE;
	return nsMsgRDFDataSource::Init();
}

nsresult nsMsgMessageDataSource::CreateLiterals(nsIRDFService *rdf)
{

	nsAutoString str(" ");;
	createNode(str, getter_AddRefs(kEmptyStringLiteral), rdf);
	str = "lowest";
	createNode(str, getter_AddRefs(kLowestLiteral), rdf);
	str = "low";
	createNode(str, getter_AddRefs(kLowLiteral), rdf);
	str = "high";
	createNode(str, getter_AddRefs(kHighLiteral), rdf);
	str = "highest";
	createNode(str, getter_AddRefs(kHighestLiteral), rdf);
	str = "flagged";
	createNode(str, getter_AddRefs(kFlaggedLiteral), rdf);
	str = "unflagged";
	createNode(str, getter_AddRefs(kUnflaggedLiteral), rdf);
	str = "replied";
	createNode(str, getter_AddRefs(kRepliedLiteral), rdf);
	str = "fowarded";
	createNode(str, getter_AddRefs(kForwardedLiteral), rdf);
	str = "new";
	createNode(str, getter_AddRefs(kNewLiteral), rdf);
	str = "read";
	createNode(str, getter_AddRefs(kReadLiteral), rdf);
	str = "true";
	createNode(str, getter_AddRefs(kTrueLiteral), rdf);
	str = "false";
	createNode(str, getter_AddRefs(kFalseLiteral), rdf);
	return NS_OK;
}

nsresult nsMsgMessageDataSource::CreateArcsOutEnumerators()
{
	nsCOMPtr<nsISupportsArray> threadsArcsOut;
	nsCOMPtr<nsISupportsArray> noThreadsArcsOut;

	nsresult rv;

	rv = getMessageArcLabelsOut(PR_TRUE, getter_AddRefs(kThreadsArcsOutArray));
	if(NS_FAILED(rv)) return rv;

	rv = getMessageArcLabelsOut(PR_FALSE, getter_AddRefs(kNoThreadsArcsOutArray));
	if(NS_FAILED(rv)) return rv;

  
	return rv;
}


NS_IMPL_ADDREF_INHERITED(nsMsgMessageDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgMessageDataSource, nsMsgRDFDataSource)

NS_IMETHODIMP
nsMsgMessageDataSource::QueryInterface(REFNSIID iid, void** result)
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
NS_IMETHODIMP nsMsgMessageDataSource::GetURI(char* *uri)
{
  if ((*uri = nsXPIDLCString::Copy("rdf:mailnewsmessages")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetSource(nsIRDFResource* property,
                                               nsIRDFNode* target,
                                               PRBool tv,
                                               nsIRDFResource** source /* out */)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetTarget(nsIRDFResource* source,
                                               nsIRDFResource* property,
                                               PRBool tv,
                                               nsIRDFNode** target)
{
	nsresult rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the mail data source.
	if (! tv)
		return NS_RDF_NO_VALUE;

	nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
	if (NS_SUCCEEDED(rv)) {
		rv = createMessageNode(message, property,target);
	}
	else
		return NS_RDF_NO_VALUE;
  
  return rv;
}

//sender is the string we need to parse.  senderuserName is the parsed user name we get back.
nsresult nsMsgMessageDataSource::GetSenderName(const PRUnichar *sender, nsAutoString *senderUserName)
{
	//XXXOnce we get the csid, use Intl version
	nsresult rv = NS_OK;
	if(mHeaderParser)
	{

		char *name;
    nsAutoString senderStr(sender);
		char *senderUTF8 = senderStr.ToNewUTF8String();
		if(!senderUTF8)
			return NS_ERROR_FAILURE;

		if(NS_SUCCEEDED(rv = mHeaderParser->ExtractHeaderAddressName("UTF-8", senderUTF8, &name)))
        {
			if(name)
			{
				PRUnichar *newSender;
				nsAutoString fmt("%s");
				newSender = nsTextFormatter::smprintf(fmt.GetUnicode(),name);
				if(newSender)
				{
					senderUserName->SetString(newSender);
					nsTextFormatter::smprintf_free(newSender);
				}
				nsCRT::free(name);
			}
			Recycle(senderUTF8);
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
	nsresult rv = NS_RDF_NO_VALUE;

	if(!targets)
		return NS_ERROR_NULL_POINTER;

	*targets = nsnull;
	nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
	if (NS_SUCCEEDED(rv)) {
		PRBool showThreads;
    GetIsThreaded(&showThreads);
    
		if(showThreads && property == kNC_MessageChild)
		{
			nsCOMPtr<nsIMsgFolder> msgfolder;
			rv = message->GetMsgFolder(getter_AddRefs(msgfolder));
			if(NS_SUCCEEDED(rv))
			{
				nsCOMPtr<nsIMsgThread> thread;
				rv = msgfolder->GetThreadForMessage(message, getter_AddRefs(thread));
				if(NS_SUCCEEDED(rv))
				{
					nsCOMPtr<nsISimpleEnumerator> messages;
					nsMsgKey msgKey;
					message->GetMessageKey(&msgKey);
					thread->EnumerateMessages(msgKey, getter_AddRefs(messages));
					nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
					NS_NewMessageFromMsgHdrEnumerator(messages, msgfolder, getter_AddRefs(converter));
					PRUint32 viewType;
					rv = GetViewType(&viewType);
					if(NS_FAILED(rv)) return rv;

					nsMessageViewMessageEnumerator * messageEnumerator = 
						new nsMessageViewMessageEnumerator(converter, viewType);
					if(!messageEnumerator)
						return NS_ERROR_OUT_OF_MEMORY;
					NS_ADDREF(messageEnumerator);
					*targets = messageEnumerator;
					rv = NS_OK;

				}
			}
		}
		else if((kNC_Subject == property) || (kNC_Date == property) ||
				(kNC_Status == property) || (kNC_Flagged == property) ||
				(kNC_Priority == property) || (kNC_Size == property) ||
				(kNC_IsUnread == property))
		{
			nsSingletonEnumerator* cursor =
				new nsSingletonEnumerator(source);
			if (cursor == nsnull)
				return NS_ERROR_OUT_OF_MEMORY;
			NS_ADDREF(cursor);
			*targets = cursor;
			rv = NS_OK;
		}
	}
	if(!*targets) {
	  //create empty cursor
	  nsCOMPtr<nsISupportsArray> assertions;
	  rv = NS_NewISupportsArray(getter_AddRefs(assertions));
		if(NS_FAILED(rv))
			return rv;

	  nsArrayEnumerator* cursor = 
		  new nsArrayEnumerator(assertions);
	  if(cursor == nsnull)
		  return NS_ERROR_OUT_OF_MEMORY;
	  NS_ADDREF(cursor);
	  *targets = cursor;
	  rv = NS_OK;
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::Assert(nsIRDFResource* source,
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMessageDataSource::Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgMessageDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
	nsCOMPtr<nsIMessage> message(do_QueryInterface(source));
	if(message)
		return DoMessageHasAssertion(message, property, target, tv, hasAssertion);
	else
		*hasAssertion = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP nsMsgMessageDataSource::ArcLabelsIn(nsIRDFNode* node,
                                                 nsISimpleEnumerator** labels)
{
	return nsMsgRDFDataSource::ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsMsgMessageDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                  nsISimpleEnumerator** labels)
{
	nsresult rv = NS_RDF_NO_VALUE;
  
		nsCOMPtr<nsISupportsArray> arcsArray;

	nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
	if (NS_SUCCEEDED(rv))
	{

		PRBool showThreads;
    rv = GetIsThreaded(&showThreads);
		if(NS_FAILED(rv)) return rv;

		if(showThreads)
		{
			arcsArray = kThreadsArcsOutArray;
		}
		else
		{
			arcsArray = kNoThreadsArcsOutArray;
		}
	}
	else 
	{
		arcsArray = kEmptyArray;
	}

	rv = NS_NewArrayEnumerator(labels, arcsArray);
	if(NS_FAILED(rv)) return rv;
  
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::getMessageArcLabelsOut(PRBool showThreads,
                                              nsISupportsArray **arcs)
{
	nsresult rv;
	rv = NS_NewISupportsArray(arcs);
	if(NS_FAILED(rv))
		return rv;

	if(NS_SUCCEEDED(rv) && showThreads)
	{
		(*arcs)->AppendElement(kNC_Total);
		(*arcs)->AppendElement(kNC_Unread);
		(*arcs)->AppendElement(kNC_MessageChild);
	}

	(*arcs)->AppendElement(kNC_Subject);
	(*arcs)->AppendElement(kNC_Sender);
	(*arcs)->AppendElement(kNC_Date);
	(*arcs)->AppendElement(kNC_Status);
	(*arcs)->AppendElement(kNC_Flagged);
	(*arcs)->AppendElement(kNC_Priority);
	(*arcs)->AppendElement(kNC_Size);
	(*arcs)->AppendElement(kNC_IsUnread);

	return NS_OK;
}


NS_IMETHODIMP
nsMsgMessageDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgMessageDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> cmds;

  nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewISupportsArray(getter_AddRefs(cmds));
    if (NS_FAILED(rv)) return rv;
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgMessageDataSource::GetAllCmds(nsIRDFResource* source,
                                      nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgMessageDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
  nsCOMPtr<nsIMessage> message;
	nsresult rv;

  PRUint32 cnt;
  rv = aSources->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = getter_AddRefs(aSources->ElementAt(i));
		message = do_QueryInterface(source, &rv);
		if (NS_SUCCEEDED(rv)) {

      // we don't care about the arguments -- message commands are always enabled
        *aResult = PR_FALSE;
        return NS_OK;
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsMsgMessageDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	nsresult rv = NS_OK;

	if((aCommand == kNC_MarkRead))
		rv = DoMarkMessagesRead(aSources, PR_TRUE);
	else if((aCommand == kNC_MarkUnread))
		rv = DoMarkMessagesRead(aSources, PR_FALSE);
	if((aCommand == kNC_MarkFlagged))
		rv = DoMarkMessagesFlagged(aSources, PR_TRUE);
	else if((aCommand == kNC_MarkUnflagged))
		rv = DoMarkMessagesFlagged(aSources, PR_FALSE);

  //for the moment return NS_OK, because failure stops entire DoCommand process.
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return OnItemAddedOrRemoved(parentItem, item, viewString, PR_TRUE);
}

NS_IMETHODIMP nsMsgMessageDataSource::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return OnItemAddedOrRemoved(parentItem, item, viewString, PR_FALSE);
}

nsresult nsMsgMessageDataSource::OnItemAddedOrRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString, PRBool added)
{

	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIRDFResource> parentResource;
	nsCOMPtr<nsIMessage> parentMessage;

	parentMessage = do_QueryInterface(parentItem);
	//If the parent isn't a message then we don't handle it.
	if(!parentMessage)
		return NS_OK;

	parentResource = do_QueryInterface(parentItem);
	//If it's not a resource, we don't handle it either
	if(!parentResource)
		return NS_OK;

	//If we are removing a message
	if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIMessage), getter_AddRefs(message))))
	{
		//We only handle threaded views

		PRBool isThreaded, isThreadNotification;
		GetIsThreaded(&isThreaded);
		isThreadNotification = PL_strcmp(viewString, "threadMessageView") == 0;
		
		if((isThreaded && isThreadNotification))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was deleted.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, added, PR_FALSE);
			}

			//Unread and total message counts will have changed.
			PRUint32 flags;
			//use the changed message to get the flags, but use the parent message to change the counts since
			//it hasn't been removed from the thread yet.
			rv = message->GetFlags(&flags);
			if(NS_SUCCEEDED(rv))
			{
				if(!(flags & MSG_FLAG_READ))
					OnChangeUnreadMessageCount(parentMessage);
			}
			OnChangeTotalMessageCount(parentMessage);
		}
	}
	return NS_OK;

}

NS_IMETHODIMP
nsMsgMessageDataSource::OnItemPropertyChanged(nsISupports *item,
                                              nsIAtom *property,
                                              const char *oldValue,
                                              const char *newValue)

{

	return NS_OK;
}
NS_IMETHODIMP
nsMsgMessageDataSource::OnItemUnicharPropertyChanged(nsISupports *item,
                                                     nsIAtom *property,
                                                     const PRUnichar *oldValue,
                                                     const PRUnichar *newValue)
{

	return NS_OK;
}

NS_IMETHODIMP
nsMsgMessageDataSource::OnItemIntPropertyChanged(nsISupports *item,
                                                 nsIAtom *property,
                                                 PRInt32 oldValue,
                                                 PRInt32 newValue)
{

	return NS_OK;
}

NS_IMETHODIMP
nsMsgMessageDataSource::OnItemBoolPropertyChanged(nsISupports *item,
                                                  nsIAtom *property,
                                                  PRBool oldValue,
                                                  PRBool newValue)
{

	return NS_OK;
}

NS_IMETHODIMP
nsMsgMessageDataSource::OnItemPropertyFlagChanged(nsISupports *item,
                                                  nsIAtom *property,
                                                  PRUint32 oldFlag,
                                                  PRUint32 newFlag)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if (kStatusAtom == property)
		{
			OnChangeStatus(resource, oldFlag, newFlag);
		}
		else if(kFlaggedAtom == property)
		{
			nsCAutoString newFlaggedStr;
			rv = createFlaggedStringFromFlag(newFlag, newFlaggedStr);
			if(NS_FAILED(rv))
				return rv;
			rv = NotifyPropertyChanged(resource, kNC_Flagged, newFlaggedStr);
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnFolderLoaded(nsIFolder *folder)
{
	nsresult rv = NS_OK;
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnDeleteOrMoveMessagesCompleted(nsIFolder *folder)
{
	nsresult rv = NS_OK;
	return rv;
}

nsresult nsMsgMessageDataSource::OnChangeStatus(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag)
{
	OnChangeStatusString(resource, oldFlag, newFlag);

	PRUint32 changedFlag = oldFlag ^ newFlag;
	if(changedFlag & MSG_FLAG_READ)
	{
		OnChangeIsUnread(resource, oldFlag, newFlag);
		PRBool showThreads;
		GetIsThreaded(&showThreads);
		if(showThreads)
		{
			nsCOMPtr<nsIMessage> message = do_QueryInterface(resource);
			if(message)
				OnChangeUnreadMessageCount(message);
		}

	}

	return NS_OK;
}

nsresult nsMsgMessageDataSource::OnChangeStatusString(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag)
{

	nsresult rv;
	nsCAutoString newStatusStr;
	rv = createStatusStringFromFlag(newFlag, newStatusStr);
	if(NS_FAILED(rv))
		return rv;
	rv = NotifyPropertyChanged(resource, kNC_Status, newStatusStr);

	
	return rv;
}

nsresult nsMsgMessageDataSource::OnChangeIsUnread(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag)
{
	nsresult rv;
	nsCOMPtr<nsIRDFNode> newIsUnreadNode;
	
	newIsUnreadNode = (newFlag & MSG_FLAG_READ) ? kFalseLiteral : kTrueLiteral;

	rv = NotifyPropertyChanged(resource, kNC_IsUnread, newIsUnreadNode);

	return rv;
}

nsresult nsMsgMessageDataSource::OnChangeUnreadMessageCount(nsIMessage *message)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;

	rv = message->GetMsgFolder(getter_AddRefs(folder));
	if(NS_FAILED(rv))
		return rv;

	rv = folder->GetThreadForMessage(message, getter_AddRefs(thread));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFNode> unreadChildrenNode;
	rv = GetUnreadChildrenNode(thread, getter_AddRefs(unreadChildrenNode));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMessage> firstMessage;
	rv = GetThreadsFirstMessage(thread, folder, getter_AddRefs(firstMessage));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> firstMessageResource = do_QueryInterface(firstMessage);
	if(firstMessageResource)
		rv = NotifyPropertyChanged(firstMessageResource, kNC_Unread, unreadChildrenNode);
	else
		return NS_ERROR_FAILURE;

	return rv;

}

nsresult nsMsgMessageDataSource::OnChangeTotalMessageCount(nsIMessage *message)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;

	rv = message->GetMsgFolder(getter_AddRefs(folder));
	if(NS_FAILED(rv))
		return rv;

	rv = folder->GetThreadForMessage(message, getter_AddRefs(thread));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFNode> numChildrenNode;
	rv = GetTotalChildrenNode(thread, getter_AddRefs(numChildrenNode));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMessage> firstMessage;
	rv = GetThreadsFirstMessage(thread, folder, getter_AddRefs(firstMessage));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> firstMessageResource = do_QueryInterface(firstMessage);
	if(firstMessageResource)
		rv = NotifyPropertyChanged(firstMessageResource, kNC_Total, numChildrenNode);
	else
		return NS_ERROR_FAILURE;

	return rv;

}
nsresult
nsMsgMessageDataSource::createMessageNode(nsIMessage *message,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;
  
	if (kNC_SubjectCollation == property)
		rv = createMessageNameNode(message, PR_TRUE, target);
	else if (kNC_Subject == property)
		rv = createMessageNameNode(message, PR_FALSE, target);
	else if (kNC_SenderCollation == property)
		rv = createMessageSenderNode(message, PR_TRUE, target);
	else if (kNC_Sender == property)
		rv = createMessageSenderNode(message, PR_FALSE, target);
	else if ((kNC_Date == property))
		rv = createMessageDateNode(message, target);
	else if ((kNC_Status == property))
		rv = createMessageStatusNode(message, target);
	else if ((kNC_Flagged == property))
		rv = createMessageFlaggedNode(message, target);
	else if ((kNC_Priority == property))
		rv = createMessagePriorityNode(message, target);
	else if ((kNC_Size == property))
		rv = createMessageSizeNode(message, target);
	else if (( kNC_Total == property))
		rv = createMessageTotalNode(message, target);
	else if ((kNC_Unread == property))
		rv = createMessageUnreadNode(message, target);
	else if((kNC_IsUnread == property))
		rv = createMessageIsUnreadNode(message, target);
  else if ((kNC_MessageChild == property))
    rv = createMessageMessageChildNode(message, target);

  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  return rv;
}


nsresult
nsMsgMessageDataSource::createMessageNameNode(nsIMessage *message,
                                             PRBool sort,
                                             nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsXPIDLString subject;
  if(sort)
	{
      rv = message->GetSubjectCollationKey(getter_Copies(subject));
	}
  else
	{
      rv = message->GetMime2DecodedSubject(getter_Copies(subject));
			if(NS_FAILED(rv))
				return rv;
      PRUint32 flags;
      rv = message->GetFlags(&flags);
      if(NS_SUCCEEDED(rv) && (flags & MSG_FLAG_HAS_RE))
			{
					nsAutoString reStr="Re: ";
					reStr +=subject;
					*((PRUnichar **)getter_Copies(subject)) = nsXPIDLString::Copy(reStr.GetUnicode());
			}
	}
	if(NS_SUCCEEDED(rv))
	 rv = createNode(subject, target, getRDFService());
  return rv;
}


nsresult
nsMsgMessageDataSource::createMessageSenderNode(nsIMessage *message,
                                               PRBool sort,
                                               nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsXPIDLString sender;
  nsAutoString senderUserName;
  if(sort)
	{
      rv = message->GetAuthorCollationKey(getter_Copies(sender));
			if(NS_SUCCEEDED(rv))
	      rv = createNode(sender, target, getRDFService());
	}
  else
	{
      rv = message->GetMime2DecodedAuthor(getter_Copies(sender));
      if(NS_SUCCEEDED(rv))
				 rv = GetSenderName(sender, &senderUserName);
			if(NS_SUCCEEDED(rv))
	       rv = createNode(senderUserName, target, getRDFService());
	}
  return rv;
}

nsresult
nsMsgMessageDataSource::createMessageDateNode(nsIMessage *message,
                                             nsIRDFNode **target)
{
  nsAutoString date;
  nsresult rv = message->GetProperty("date", date);
	if(NS_FAILED(rv))
		return rv;
  PRInt32 error;
  PRUint32 aLong = date.ToInteger(&error, 16);
  // As the time is stored in seconds, we need to multiply it by PR_USEC_PER_SEC,
  // to get back a valid 64 bits value
  PRInt64 microSecondsPerSecond, intermediateResult;
  PRTime aTime;
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(intermediateResult, aLong);
  LL_MUL(aTime, intermediateResult, microSecondsPerSecond);
  
  
	rv = createDateNode(aTime, target, getRDFService());
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageStatusNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	*target = kEmptyStringLiteral;
	if(flags & MSG_FLAG_REPLIED)
		*target = kRepliedLiteral;
	else if(flags & MSG_FLAG_FORWARDED)
		*target = kForwardedLiteral;
	else if(flags & MSG_FLAG_NEW)
		*target = kNewLiteral;
	else if(flags & MSG_FLAG_READ)
		*target = kReadLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageIsUnreadNode(nsIMessage *message, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	if(flags & MSG_FLAG_READ)
		*target = kFalseLiteral;
	else
		*target = kTrueLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageFlaggedNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	*target = kEmptyStringLiteral;

	if(flags & MSG_FLAG_MARKED)
		*target = kFlaggedLiteral;
	else 
		*target = kUnflaggedLiteral;
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult 
nsMsgMessageDataSource::createStatusStringFromFlag(PRUint32 flags, nsCAutoString &statusStr)
{
	nsresult rv = NS_OK;
	statusStr = " ";
	if(flags & MSG_FLAG_REPLIED)
		statusStr = "replied";
	else if(flags & MSG_FLAG_FORWARDED)
		statusStr = "forwarded";
	else if(flags & MSG_FLAG_NEW)
		statusStr = "new";
	else if(flags & MSG_FLAG_READ)
		statusStr = "read";
	return rv;
}

nsresult 
nsMsgMessageDataSource::createFlaggedStringFromFlag(PRUint32 flags, nsCAutoString &statusStr)
{
	nsresult rv = NS_OK;
	if(flags & MSG_FLAG_MARKED)
		statusStr = "flagged";
	else 
		statusStr = "unflagged";
	return rv;
}

nsresult 
nsMsgMessageDataSource::createPriorityString(nsMsgPriority priority, nsCAutoString &priorityStr)
{
	nsresult rv = NS_OK;
	priorityStr = " ";
	switch (priority)
	{
		case nsMsgPriorityNotSet:
		case nsMsgPriorityNone:
		case nsMsgPriorityNormal:
			priorityStr = " ";
			break;
		case nsMsgPriorityLowest:
			priorityStr = "Lowest";
			break;
		case nsMsgPriorityLow:
			priorityStr = "Low";
			break;
		case nsMsgPriorityHigh:
			priorityStr = "High";
			break;
		case nsMsgPriorityHighest:
			priorityStr = "Highest";
			break;
	}
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessagePriorityNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	nsMsgPriority priority;
	rv = message->GetPriority(&priority);
	if(NS_FAILED(rv))
		return rv;
	*target = kEmptyStringLiteral;
	switch (priority)
	{
		case nsMsgPriorityNotSet:
		case nsMsgPriorityNone:
		case nsMsgPriorityNormal:
			*target = kEmptyStringLiteral;
			break;
		case nsMsgPriorityLowest:
			*target = kLowestLiteral;
			break;
		case nsMsgPriorityLow:
			*target = kLowLiteral;
			break;
		case nsMsgPriorityHigh:
			*target = kHighLiteral;
			break;
		case nsMsgPriorityHighest:
			*target = kHighestLiteral;
			break;
	}
	
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageSizeNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 size;
	nsAutoString sizeStr("");
	rv = message->GetMessageSize(&size);
	if(NS_FAILED(rv))
		return rv;
	if(size < 1024)
		size = 1024;
	PRUint32 sizeInKB = size/1024;

	char * kbStr = PR_smprintf("%uKB", sizeInKB);
	if(kbStr)
	{
		sizeStr =kbStr;
		PR_smprintf_free(kbStr);
	}
	rv = createNode(sizeStr, target, getRDFService());
	return rv;
}

nsresult nsMsgMessageDataSource::createMessageTotalNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv;
	nsAutoString emptyString("");

	PRBool showThreads;
  GetIsThreaded(&showThreads);

	if(!showThreads)
		rv = createNode(emptyString, target, getRDFService());
	else
	{
		rv = GetMessageFolderAndThread(message, getter_AddRefs(folder), getter_AddRefs(thread));
		if(NS_SUCCEEDED(rv) && thread)
		{
			if(IsThreadsFirstMessage(thread, message))
			{
				rv = GetTotalChildrenNode(thread, target);
			}
			else
			{
				rv = createNode(emptyString, target, getRDFService());
			}

		}
	}

	if(NS_SUCCEEDED(rv))
		return rv;
	else 
		return NS_RDF_NO_VALUE;
}

nsresult nsMsgMessageDataSource::createMessageUnreadNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv;
	nsAutoString emptyString("");

	PRBool showThreads;
  rv = GetIsThreaded(&showThreads);
	if(NS_FAILED(rv)) return rv;

	if(!showThreads)
		rv = createNode(emptyString, target, getRDFService());
	else
	{
		rv = GetMessageFolderAndThread(message, getter_AddRefs(folder), getter_AddRefs(thread));
		if(NS_SUCCEEDED(rv) && thread)
		{
			if(IsThreadsFirstMessage(thread, message))
			{
				rv = GetUnreadChildrenNode(thread, target);
			}
			else
			{
				rv = createNode(emptyString, target, getRDFService());
			}
		}
	}

	if(NS_SUCCEEDED(rv))
		return rv;
	else 
		return NS_RDF_NO_VALUE;
}

nsresult nsMsgMessageDataSource::GetUnreadChildrenNode(nsIMsgThread *thread, nsIRDFNode **target)
{
	nsresult rv;
	nsAutoString emptyString("");
	PRUint32 numUnread;
	rv = thread->GetNumUnreadChildren(&numUnread);
	if(NS_SUCCEEDED(rv))
	{
		if(numUnread > 0)
			rv = createNode(numUnread, target, getRDFService());
		else
			rv = createNode(emptyString, target, getRDFService());
	}

	return rv;
}

nsresult nsMsgMessageDataSource::GetTotalChildrenNode(nsIMsgThread *thread, nsIRDFNode **target)
{
	nsresult rv;
	nsAutoString emptyString("");
	PRUint32 numChildren;
	rv = thread->GetNumChildren(&numChildren);
	if(NS_SUCCEEDED(rv))
	{
		if(numChildren > 0)
			rv = createNode(numChildren, target, getRDFService());
		else
			rv = createNode(emptyString, target, getRDFService());
	}

	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageMessageChildNode(nsIMessage *message,
                                                 nsIRDFNode **target)
{
  nsresult rv;
  // this is slow, but for now, call GetTargets and then create
  // a node out of the first message, if any
  nsCOMPtr<nsIRDFResource> messageResource(do_QueryInterface(message));
  nsCOMPtr<nsISimpleEnumerator> messages;
  rv = GetTargets(messageResource, kNC_MessageChild, PR_TRUE,
                  getter_AddRefs(messages));

  PRBool hasMessages;
  messages->HasMoreElements(&hasMessages);

  if (hasMessages)
    return createNode("has messages", target, getRDFService());

  return NS_RDF_NO_VALUE;
}


nsresult nsMsgMessageDataSource::GetMessageFolderAndThread(nsIMessage *message,
															nsIMsgFolder **folder,
															nsIMsgThread **thread)
{
	nsresult rv;
	rv = message->GetMsgFolder(folder);
	if(NS_SUCCEEDED(rv))
	{
		rv = (*folder)->GetThreadForMessage(message, thread);
	}
	return rv;
}

PRBool nsMsgMessageDataSource::IsThreadsFirstMessage(nsIMsgThread *thread, nsIMessage *message)
{
	nsCOMPtr<nsIMsgDBHdr> firstHdr;
	nsresult rv;

	rv = thread->GetRootHdr(nsnull, getter_AddRefs(firstHdr));
	if(NS_FAILED(rv))
		return PR_FALSE;
  NS_ASSERTION(firstHdr, "Failed to get root header!");

	nsMsgKey messageKey, firstHdrKey;

	rv = message->GetMessageKey(&messageKey);

	if(NS_FAILED(rv))
		return PR_FALSE;

	rv = firstHdr->GetMessageKey(&firstHdrKey);

	if(NS_FAILED(rv))
		return PR_FALSE;

	return messageKey == firstHdrKey;

}

nsresult nsMsgMessageDataSource::GetThreadsFirstMessage(nsIMsgThread *thread, nsIMsgFolder *folder, nsIMessage **message)
{
	nsCOMPtr<nsIMsgDBHdr> firstHdr;
	nsresult rv;

	rv = thread->GetRootHdr(nsnull, getter_AddRefs(firstHdr));
	if(NS_FAILED(rv))
		return rv;

	rv = folder->CreateMessageFromMsgDBHdr(firstHdr, message);

	return rv;
}

nsresult
nsMsgMessageDataSource::DoMarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
	PRUint32 count;
	nsresult rv;

	nsCOMPtr<nsITransactionManager> transactionManager;
	rv = GetTransactionManager(messages, getter_AddRefs(transactionManager));
	if(NS_FAILED(rv))
		return rv;

	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;
	while(count > 0)
	{
		nsCOMPtr<nsISupportsArray> messageArray;
		nsCOMPtr<nsIMsgFolder> folder;
	
		rv = GetMessagesAndFirstFolder(messages, getter_AddRefs(folder), getter_AddRefs(messageArray));
		if(NS_FAILED(rv))
			return rv;

		folder->MarkMessagesRead(messageArray, markRead);
		rv = messages->Count(&count);
		if(NS_FAILED(rv))
			return rv;
	}
	return rv;
}

nsresult
nsMsgMessageDataSource::DoMarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged)
{
	PRUint32 count;
	nsresult rv;

	nsCOMPtr<nsITransactionManager> transactionManager;
	rv = GetTransactionManager(messages, getter_AddRefs(transactionManager));
	if(NS_FAILED(rv))
		return rv;

	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;
	while(count > 0)
	{
		nsCOMPtr<nsISupportsArray> messageArray;
		nsCOMPtr<nsIMsgFolder> folder;
	
		rv = GetMessagesAndFirstFolder(messages, getter_AddRefs(folder), getter_AddRefs(messageArray));
		if(NS_FAILED(rv))
			return rv;

		folder->MarkMessagesFlagged(messageArray, markFlagged);
		rv = messages->Count(&count);
		if(NS_FAILED(rv))
			return rv;
	}
	return rv;
}


nsresult nsMsgMessageDataSource::DoMessageHasAssertion(nsIMessage *message, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion)
{
	nsresult rv = NS_OK;
	if(!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	*hasAssertion = PR_FALSE;

	//We're not keeping track of negative assertions on messages.
	if(!tv)
	{
		return NS_OK;
	}

	//first check to see if message child property
	if(kNC_MessageChild == property)
	{
		PRBool isThreaded;
		GetIsThreaded(&isThreaded);

		//We only care if we're in the threaded view.  Otherwise just say we don't have the assertion.
		if(isThreaded)
		{
			//If the message is the threadParent of the target then it has the assertion.  Otherwise it doesn't

			nsCOMPtr<nsIMessage> childMessage = do_QueryInterface(target);
			if(!childMessage)
				return NS_OK;

			nsMsgKey threadParent, msgKey;
			rv = message->GetMessageKey(&msgKey);
			if(NS_FAILED(rv))
				return rv;

			rv = childMessage->GetThreadParent(&threadParent);
			if(NS_FAILED(rv))
				return rv;

			*hasAssertion = (msgKey == threadParent);
			return NS_OK;
		}
		else
		{
			return NS_OK;
		}

	}
	nsCOMPtr<nsIRDFResource> messageResource(do_QueryInterface(message, &rv));

	if(NS_FAILED(rv))
		return rv;

	rv = GetTargetHasAssertion(this, messageResource, property, tv, target, hasAssertion);


	return rv;


}

nsresult nsMsgMessageDataSource::GetMessagesAndFirstFolder(nsISupportsArray *messages, nsIMsgFolder **folder,
														   nsISupportsArray **messageArray)
{
	nsresult rv;
	PRUint32 count;
	
	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;

	if(count <= 0)
		return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsISupportsArray> folderMessageArray;

	rv = NS_NewISupportsArray(getter_AddRefs(folderMessageArray));
	if(NS_FAILED(rv))
		return rv;

	//Get the first message and its folder
	nsCOMPtr<nsISupports> messageSupports = getter_AddRefs(messages->ElementAt(0));
	nsCOMPtr<nsIMessage> message = do_QueryInterface(messageSupports);

	if(!message)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIMsgFolder> messageFolder;
	rv = message->GetMsgFolder(getter_AddRefs(messageFolder));
	if(NS_FAILED(rv))
		return rv;

	//Now add all messages that have the same folder as the first one to the array.
	for(PRUint32 i=0; i < count; i++)
	{
		messageSupports = getter_AddRefs(messages->ElementAt(i));
		message = do_QueryInterface(messageSupports);

		if(!message)
			return NS_ERROR_NULL_POINTER;

		nsCOMPtr<nsIMsgFolder> curFolder;
		rv = message->GetMsgFolder(getter_AddRefs(curFolder));
		if(NS_FAILED(rv))
			return rv;

		if(curFolder.get() == messageFolder.get())
		{
			folderMessageArray->AppendElement(messageSupports);
			messages->RemoveElementAt(i);
			i--;
			count--;
		}

	}
	*folder = messageFolder;
	NS_IF_ADDREF(*folder);

	*messageArray = folderMessageArray;
	NS_IF_ADDREF(*messageArray);
	return rv;

}




