/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...

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
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsDateTimeFormatCID.h"
#include "nsMsgBaseCID.h"


static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kMsgMailSessionCID,		NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kLocaleFactoryCID,			NS_LOCALEFACTORY_CID);
static NS_DEFINE_CID(kDateTimeFormatCID,		NS_DATETIMEFORMAT_CID);

nsIRDFResource* nsMsgMessageDataSource::kNC_Subject = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Sender= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Date= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Status= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Flagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Priority= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Size= nsnull;


//commands
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnread= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_ToggleRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkFlagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnflagged= nsnull;


nsMsgMessageDataSource::nsMsgMessageDataSource():
  mInitialized(PR_FALSE),
  mRDFService(nsnull),
  mHeaderParser(nsnull)
{


}

nsMsgMessageDataSource::~nsMsgMessageDataSource (void)
{
	nsresult rv;
	mRDFService->UnregisterDataSource(this);

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 

	if(NS_SUCCEEDED(rv))
		mailSession->RemoveFolderListener(this);

	nsrefcnt refcnt;

	if (kNC_Subject)
		NS_RELEASE2(kNC_Subject, refcnt);
	if (kNC_Sender)
		NS_RELEASE2(kNC_Sender, refcnt);
	if (kNC_Date)
		NS_RELEASE2(kNC_Date, refcnt);
	if (kNC_Status)
		NS_RELEASE2(kNC_Status, refcnt);
	if (kNC_Flagged)
		NS_RELEASE2(kNC_Flagged, refcnt);
	if (kNC_Priority)
		NS_RELEASE2(kNC_Priority, refcnt);
	if (kNC_Size)
		NS_RELEASE2(kNC_Size, refcnt);
	if (kNC_MarkRead)
		NS_RELEASE2(kNC_MarkRead, refcnt);
	if (kNC_MarkUnread)
		NS_RELEASE2(kNC_MarkUnread, refcnt);
	if (kNC_ToggleRead)
		NS_RELEASE2(kNC_ToggleRead, refcnt);
	if (kNC_MarkRead)
		NS_RELEASE2(kNC_MarkFlagged, refcnt);
	if (kNC_MarkUnread)
		NS_RELEASE2(kNC_MarkUnflagged, refcnt);

	nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); // XXX probably need shutdown listener here
	NS_IF_RELEASE(mHeaderParser);
	mRDFService = nsnull;
}

nsresult nsMsgMessageDataSource::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;


    nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                             (nsISupports**) &mRDFService); // XXX probably need shutdown listener here

	if(NS_FAILED(rv))
		return rv;

	mRDFService->RegisterDataSource(this, PR_FALSE);
	rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                          NULL, 
                                          nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(), 
                                          (void **) &mHeaderParser);

	if(NS_FAILED(rv))
		return rv;

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);
	PR_ASSERT(NS_SUCCEEDED(rv));
	if (! kNC_Subject) {
    
		mRDFService->GetResource(NC_RDF_SUBJECT, &kNC_Subject);
		mRDFService->GetResource(NC_RDF_SENDER, &kNC_Sender);
		mRDFService->GetResource(NC_RDF_DATE, &kNC_Date);
		mRDFService->GetResource(NC_RDF_STATUS, &kNC_Status);
		mRDFService->GetResource(NC_RDF_FLAGGED, &kNC_Flagged);
		mRDFService->GetResource(NC_RDF_PRIORITY, &kNC_Priority);
		mRDFService->GetResource(NC_RDF_SIZE, &kNC_Size);

		mRDFService->GetResource(NC_RDF_MARKREAD, &kNC_MarkRead);
		mRDFService->GetResource(NC_RDF_MARKUNREAD, &kNC_MarkUnread);
		mRDFService->GetResource(NC_RDF_TOGGLEREAD, &kNC_ToggleRead);
		mRDFService->GetResource(NC_RDF_MARKFLAGGED, &kNC_MarkFlagged);
		mRDFService->GetResource(NC_RDF_MARKUNFLAGGED, &kNC_MarkUnflagged);
    
	}
	mInitialized = PR_TRUE;
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
	if(iid.Equals(nsCOMTypeInfo<nsIFolderListener>::GetIID()))
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
nsresult nsMsgMessageDataSource::GetSenderName(nsAutoString& sender, nsAutoString *senderUserName)
{
	//XXXOnce we get the csid, use Intl version
	nsresult rv = NS_OK;
	if(mHeaderParser)
	{
		char *name;
		if(NS_SUCCEEDED(rv = mHeaderParser->ExtractHeaderAddressName (nsnull, nsAutoCString(sender), &name)))
		{
			*senderUserName = name;
		}
		if(name)
			PL_strfree(name);
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
		if((kNC_Subject == property) || (kNC_Date == property) ||
			(kNC_Status == property) || (kNC_Flagged == property) ||
			(kNC_Priority == property) || (kNC_Size == property))
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
  nsCOMPtr<nsISupportsArray> arcs;
  nsresult rv = NS_RDF_NO_VALUE;
  

  nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
#ifdef NS_DEBUG
    fflush(stdout);
#endif
    rv = getMessageArcLabelsOut(message, getter_AddRefs(arcs));
  } else {
    // how to return an empty cursor?
    // for now return a 0-length nsISupportsArray
    rv = NS_NewISupportsArray(getter_AddRefs(arcs));
		if(NS_FAILED(rv))
			return rv;
  }

  nsArrayEnumerator* cursor =
    new nsArrayEnumerator(arcs);
  
  if (cursor == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *labels = cursor;
  
  return NS_OK;
}

nsresult
nsMsgMessageDataSource::getMessageArcLabelsOut(nsIMessage *folder,
                                              nsISupportsArray **arcs)
{
	nsresult rv;
	rv = NS_NewISupportsArray(arcs);
	if(NS_FAILED(rv))
		return rv;

	(*arcs)->AppendElement(kNC_Subject);
	(*arcs)->AppendElement(kNC_Sender);
	(*arcs)->AppendElement(kNC_Date);
	(*arcs)->AppendElement(kNC_Status);
	(*arcs)->AppendElement(kNC_Flagged);
	(*arcs)->AppendElement(kNC_Priority);
	(*arcs)->AppendElement(kNC_Size);
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

NS_IMETHODIMP nsMsgMessageDataSource::OnItemAdded(nsIFolder *parentFolder, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnItemRemoved(nsIFolder *parentFolder, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
														   const char *oldValue, const char *newValue)

{

	return NS_OK;
}

NS_IMETHODIMP nsMsgMessageDataSource::OnItemPropertyFlagChanged(nsISupports *item, const char *property,
									   PRUint32 oldFlag, PRUint32 newFlag)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if(PL_strcmp("Status", property) == 0)
		{
			nsCAutoString oldStatusStr, newStatusStr;
			rv = createStatusStringFromFlag(oldFlag, oldStatusStr);
			if(NS_FAILED(rv))
				return rv;
			rv = createStatusStringFromFlag(newFlag, newStatusStr);
			if(NS_FAILED(rv))
				return rv;
			rv = NotifyPropertyChanged(resource, kNC_Status, oldStatusStr, newStatusStr);
		}
		else if(PL_strcmp("Flagged", property) == 0)
		{
			nsCAutoString oldFlaggedStr, newFlaggedStr;
			rv = createFlaggedStringFromFlag(oldFlag, oldFlaggedStr);
			if(NS_FAILED(rv))
				return rv;
			rv = createFlaggedStringFromFlag(newFlag, newFlaggedStr);
			if(NS_FAILED(rv))
				return rv;
			rv = NotifyPropertyChanged(resource, kNC_Flagged, oldFlaggedStr, newFlaggedStr);
		}
	}
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageNode(nsIMessage *message,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
    if (peqCollationSort(kNC_Subject, property))
      return createMessageNameNode(message, PR_TRUE, target);
    else if (kNC_Subject == property)
      return createMessageNameNode(message, PR_FALSE, target);
    else if (peqCollationSort(kNC_Sender, property))
      return createMessageSenderNode(message, PR_TRUE, target);
    else if (kNC_Sender == property)
      return createMessageSenderNode(message, PR_FALSE, target);
    else if ((kNC_Date == property))
      return createMessageDateNode(message, target);
		else if ((kNC_Status == property))
      return createMessageStatusNode(message, target);
		else if ((kNC_Flagged == property))
      return createMessageFlaggedNode(message, target);
		else if ((kNC_Priority == property))
      return createMessagePriorityNode(message, target);
		else if ((kNC_Size == property))
      return createMessageSizeNode(message, target);
    else
      return NS_RDF_NO_VALUE;
}


nsresult
nsMsgMessageDataSource::createMessageNameNode(nsIMessage *message,
                                             PRBool sort,
                                             nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsAutoString subject;
  if(sort)
	{
      rv = message->GetSubjectCollationKey(&subject);
	}
  else
	{
      rv = message->GetMime2DecodedSubject(&subject);
			if(NS_FAILED(rv))
				return rv;
      PRUint32 flags;
      rv = message->GetFlags(&flags);
      if(NS_SUCCEEDED(rv) && (flags & MSG_FLAG_HAS_RE))
			{
					nsAutoString reStr="Re: ";
					reStr +=subject;
					subject = reStr;
			}
	}
	if(NS_SUCCEEDED(rv))
	 rv = createNode(subject, target);
  return rv;
}


nsresult
nsMsgMessageDataSource::createMessageSenderNode(nsIMessage *message,
                                               PRBool sort,
                                               nsIRDFNode **target)
{
  nsresult rv = NS_OK;
  nsAutoString sender, senderUserName;
  if(sort)
	{
      rv = message->GetAuthorCollationKey(&sender);
			if(NS_SUCCEEDED(rv))
	      rv = createNode(sender, target);
	}
  else
	{
      rv = message->GetMime2DecodedAuthor(&sender);
      if(NS_SUCCEEDED(rv))
				 rv = GetSenderName(sender, &senderUserName);
			if(NS_SUCCEEDED(rv))
	       rv = createNode(senderUserName, target);
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
  
  
	rv = createDateNode(aTime, target);
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageStatusNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	nsCAutoString statusStr;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	rv = createStatusStringFromFlag(flags, statusStr);
	if(NS_FAILED(rv))
		return rv;
	nsString uniStr = statusStr;
	rv = createNode(uniStr, target);
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageFlaggedNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	nsCAutoString statusStr;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	rv = createFlaggedStringFromFlag(flags, statusStr);
	if(NS_FAILED(rv))
		return rv;
	nsString uniStr = statusStr;
	rv = createNode(uniStr, target);
	return rv;
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
	nsCAutoString priorityStr;
	nsMsgPriority priority;
	rv = message->GetPriority(&priority);
	if(NS_FAILED(rv))
		return rv;
	rv = createPriorityString(priority, priorityStr);
	if(NS_FAILED(rv))
		return rv;
	nsString uniStr = priorityStr;

	rv = createNode(uniStr, target);
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageSizeNode(nsIMessage *message,
                                               nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 size;
	nsString sizeStr="";
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
	rv = createNode(sizeStr, target);
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

nsresult nsMsgMessageDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  const char *oldValue, const char *newValue)
{
	nsCOMPtr<nsIRDFNode> oldValueNode, newValueNode;
	nsString oldValueStr = oldValue;
	nsString newValueStr = newValue;
	createNode(oldValueStr,getter_AddRefs(oldValueNode));
	createNode(newValueStr, getter_AddRefs(newValueNode));
	NotifyObservers(resource, propertyResource, oldValueNode, PR_FALSE);
	NotifyObservers(resource, propertyResource, newValueNode, PR_TRUE);
	return NS_OK;
}


nsresult nsMsgMessageDataSource::DoMessageHasAssertion(nsIMessage *message, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion)
{
	nsresult rv = NS_OK;
	if(!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	//We're not keeping track of negative assertions on messages.
	if(!tv)
	{
		*hasAssertion = PR_FALSE;
		return NS_OK;
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

nsresult
NS_NewMsgMessageDataSource(const nsIID& iid, void **result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsMsgMessageDataSource* datasource = new nsMsgMessageDataSource();
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = datasource->Init();
    if (NS_FAILED(rv)) {
        delete datasource;
        return rv;
    }

	return datasource->QueryInterface(iid, result);
}


