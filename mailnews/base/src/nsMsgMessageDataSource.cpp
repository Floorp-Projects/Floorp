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
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

nsIRDFResource* nsMsgMessageDataSource::kNC_Subject = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_SubjectCollation = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Sender= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_SenderCollation = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Recipient= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_RecipientCollation = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Date= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Status= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_StatusString= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Flagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_FlaggedSort= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Priority= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_PriorityString= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_PrioritySort= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Size= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_SizeSort= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Total = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_Unread = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MessageChild = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_IsUnread = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_IsUnreadSort = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_HasAttachment = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_IsImapDeleted = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MessageType = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_OrderReceived = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_OrderReceivedSort = nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_ThreadState = nsnull;


//commands
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnread= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_ToggleRead= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkFlagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkUnflagged= nsnull;
nsIRDFResource* nsMsgMessageDataSource::kNC_MarkThreadRead= nsnull;

nsrefcnt nsMsgMessageDataSource::gMessageResourceRefCnt = 0;

nsIAtom * nsMsgMessageDataSource::kFlaggedAtom  = nsnull;
nsIAtom * nsMsgMessageDataSource::kStatusAtom   = nsnull;



nsMsgMessageDataSource::nsMsgMessageDataSource()
{
	mStringBundle = nsnull;

  mHeaderParser = do_CreateInstance(kMsgHeaderParserCID);
	nsIRDFService *rdf = getRDFService();
  
	if (gMessageResourceRefCnt++ == 0) {
    
		rdf->GetResource(NC_RDF_SUBJECT, &kNC_Subject);
		rdf->GetResource(NC_RDF_SUBJECT_COLLATION_SORT, &kNC_SubjectCollation);
		rdf->GetResource(NC_RDF_SENDER, &kNC_Sender);
		rdf->GetResource(NC_RDF_SENDER_COLLATION_SORT, &kNC_SenderCollation);
		rdf->GetResource(NC_RDF_RECIPIENT, &kNC_Recipient);
		rdf->GetResource(NC_RDF_RECIPIENT_COLLATION_SORT, &kNC_RecipientCollation);
		rdf->GetResource(NC_RDF_DATE, &kNC_Date);
		rdf->GetResource(NC_RDF_STATUS, &kNC_Status);
		rdf->GetResource(NC_RDF_STATUS_STRING, &kNC_StatusString);
		rdf->GetResource(NC_RDF_FLAGGED, &kNC_Flagged);
		rdf->GetResource(NC_RDF_FLAGGED_SORT, &kNC_FlaggedSort);
		rdf->GetResource(NC_RDF_PRIORITY, &kNC_Priority);
		rdf->GetResource(NC_RDF_PRIORITY_STRING, &kNC_PriorityString);
		rdf->GetResource(NC_RDF_PRIORITY_SORT, &kNC_PrioritySort);
		rdf->GetResource(NC_RDF_SIZE, &kNC_Size);
		rdf->GetResource(NC_RDF_SIZE_SORT, &kNC_SizeSort);
		rdf->GetResource(NC_RDF_TOTALMESSAGES,   &kNC_Total);
		rdf->GetResource(NC_RDF_TOTALUNREADMESSAGES,   &kNC_Unread);
		rdf->GetResource(NC_RDF_MESSAGECHILD,   &kNC_MessageChild);
		rdf->GetResource(NC_RDF_ISUNREAD, &kNC_IsUnread);
		rdf->GetResource(NC_RDF_ISUNREAD_SORT, &kNC_IsUnreadSort);
		rdf->GetResource(NC_RDF_HASATTACHMENT, &kNC_HasAttachment);
		rdf->GetResource(NC_RDF_ISIMAPDELETED, &kNC_IsImapDeleted);
		rdf->GetResource(NC_RDF_MESSAGETYPE, &kNC_MessageType);
		rdf->GetResource(NC_RDF_ORDERRECEIVED, &kNC_OrderReceived);
		rdf->GetResource(NC_RDF_ORDERRECEIVED_SORT, &kNC_OrderReceivedSort);
		rdf->GetResource(NC_RDF_THREADSTATE, &kNC_ThreadState);

		rdf->GetResource(NC_RDF_MARKREAD, &kNC_MarkRead);
		rdf->GetResource(NC_RDF_MARKUNREAD, &kNC_MarkUnread);
		rdf->GetResource(NC_RDF_TOGGLEREAD, &kNC_ToggleRead);
		rdf->GetResource(NC_RDF_MARKFLAGGED, &kNC_MarkFlagged);
		rdf->GetResource(NC_RDF_MARKUNFLAGGED, &kNC_MarkUnflagged);
    rdf->GetResource(NC_RDF_MARKTHREADREAD, &kNC_MarkThreadRead);

    kStatusAtom = NS_NewAtom("Status");
    kFlaggedAtom = NS_NewAtom("Flagged");
	}

	CreateLiterals(rdf);
	CreateArcsOutEnumerators();
}

nsMsgMessageDataSource::~nsMsgMessageDataSource (void)
{

	if (--gMessageResourceRefCnt == 0)
	{
		nsrefcnt refcnt;

		NS_RELEASE2(kNC_Subject, refcnt);
		NS_RELEASE2(kNC_SubjectCollation, refcnt);
		NS_RELEASE2(kNC_Sender, refcnt);
		NS_RELEASE2(kNC_SenderCollation, refcnt);
		NS_RELEASE2(kNC_Recipient, refcnt);
		NS_RELEASE2(kNC_RecipientCollation, refcnt);
		NS_RELEASE2(kNC_Date, refcnt);
		NS_RELEASE2(kNC_Status, refcnt);
		NS_RELEASE2(kNC_StatusString, refcnt);
		NS_RELEASE2(kNC_Flagged, refcnt);
		NS_RELEASE2(kNC_FlaggedSort, refcnt);
		NS_RELEASE2(kNC_Priority, refcnt);
		NS_RELEASE2(kNC_PriorityString, refcnt);
		NS_RELEASE2(kNC_PrioritySort, refcnt);
		NS_RELEASE2(kNC_Size, refcnt);
		NS_RELEASE2(kNC_SizeSort, refcnt);
		NS_RELEASE2(kNC_Total, refcnt);
		NS_RELEASE2(kNC_Unread, refcnt);
		NS_RELEASE2(kNC_MessageChild, refcnt);
		NS_RELEASE2(kNC_IsUnread, refcnt);
		NS_RELEASE2(kNC_IsUnreadSort, refcnt);
		NS_RELEASE2(kNC_HasAttachment, refcnt);
		NS_RELEASE2(kNC_IsImapDeleted, refcnt);
		NS_RELEASE2(kNC_MessageType, refcnt);
		NS_RELEASE2(kNC_OrderReceived, refcnt);
		NS_RELEASE2(kNC_OrderReceivedSort, refcnt);
		NS_RELEASE2(kNC_ThreadState, refcnt);

		NS_RELEASE2(kNC_MarkRead, refcnt);
		NS_RELEASE2(kNC_MarkUnread, refcnt);
		NS_RELEASE2(kNC_ToggleRead, refcnt);
		NS_RELEASE2(kNC_MarkFlagged, refcnt);
		NS_RELEASE2(kNC_MarkUnflagged, refcnt);
		NS_RELEASE2(kNC_MarkThreadRead, refcnt);

    NS_RELEASE(kStatusAtom);
    NS_RELEASE(kFlaggedAtom);
	}

}

void nsMsgMessageDataSource::Cleanup()
{
  nsresult rv;
  if (!m_shuttingDown) {
    
    nsCOMPtr<nsIMsgMailSession> mailSession =
      do_GetService(kMsgMailSessionCID, &rv);
    
    if(NS_SUCCEEDED(rv))
      mailSession->RemoveFolderListener(this);
  }

  nsMsgRDFDataSource::Cleanup();
}

nsresult nsMsgMessageDataSource::Init()
{
	nsresult rv;
  
  rv = nsMsgRDFDataSource::Init();
  if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);
  
  return NS_OK;
}

nsresult nsMsgMessageDataSource::CreateLiterals(nsIRDFService *rdf)
{
	PRUnichar *prustr = nsnull;
	createNode((const PRUnichar*)NS_LITERAL_STRING(" "), getter_AddRefs(kEmptyStringLiteral), rdf);

  //
  // internal strings - not to be localized - usually reflected into the DOM
  // via the datasource, so that content can be styled
  //
  
  // priority stuff
	createNode((const PRUnichar*)NS_LITERAL_STRING("lowest"), getter_AddRefs(kLowestLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("low"), getter_AddRefs(kLowLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("high"), getter_AddRefs(kHighLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("highest"),getter_AddRefs(kHighestLiteral),rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("4"), getter_AddRefs(kLowestSortLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("3"), getter_AddRefs(kLowSortLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("2"), getter_AddRefs(kNormalSortLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("1"), getter_AddRefs(kHighSortLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("0"), getter_AddRefs(kHighestSortLiteral), rdf);

  // message status - 
	createNode((const PRUnichar*)NS_LITERAL_STRING("flagged"),getter_AddRefs(kFlaggedLiteral),rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("unflagged"), getter_AddRefs(kUnflaggedLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("replied"),getter_AddRefs(kRepliedLiteral),rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("new"), getter_AddRefs(kNewLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("read"), getter_AddRefs(kReadLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("fowarded"), getter_AddRefs(kForwardedLiteral), rdf);

  // other useful strings
	createNode((const PRUnichar*)NS_LITERAL_STRING("true"), getter_AddRefs(kTrueLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("false"), getter_AddRefs(kFalseLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("news"), getter_AddRefs(kNewsLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("mail"), getter_AddRefs(kMailLiteral), rdf);

	//strings for the thread column.
	createNode((const PRUnichar*)NS_LITERAL_STRING("noThread"), getter_AddRefs(kNoThreadLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("thread"), getter_AddRefs(kThreadLiteral), rdf);
	createNode((const PRUnichar*)NS_LITERAL_STRING("threadWithUnread"), getter_AddRefs(kThreadWithUnreadLiteral), rdf);
  //
  // localized strings - some of the above strings need to be displayed
  // to the user

  // priority
	prustr = GetString(NS_LITERAL_STRING("priorityHighest"));
	createNode(prustr, getter_AddRefs(kHighestLiteralDisplayString), rdf);
  Recycle(prustr);

	prustr = GetString(NS_LITERAL_STRING("priorityHigh"));
	createNode(prustr, getter_AddRefs(kHighLiteralDisplayString), rdf);
  Recycle(prustr);

	prustr = GetString(NS_LITERAL_STRING("priorityLow"));
	createNode(prustr, getter_AddRefs(kLowLiteralDisplayString), rdf);
  Recycle(prustr);
	
	prustr = GetString(NS_LITERAL_STRING("priorityLowest"));
	createNode(prustr, getter_AddRefs(kLowestLiteralDisplayString), rdf);
  Recycle(prustr);

  // message status
	prustr = GetString(NS_LITERAL_STRING("new"));
	createNode(prustr, getter_AddRefs(kNewLiteralDisplayString), rdf);
  Recycle(prustr);
	
	prustr = GetString(NS_LITERAL_STRING("read"));
	createNode(prustr, getter_AddRefs(kReadLiteralDisplayString), rdf);
  Recycle(prustr);
	
	prustr = GetString(NS_LITERAL_STRING("forwarded"));
	createNode(prustr, getter_AddRefs(kForwardedLiteralDisplayString), rdf);
  Recycle(prustr);
	
	prustr = GetString(NS_LITERAL_STRING("replied"));
	createNode(prustr, getter_AddRefs(kRepliedLiteralDisplayString), rdf);
  Recycle(prustr);

	return NS_OK;
}

nsresult nsMsgMessageDataSource::CreateArcsOutEnumerators()
{

	nsresult rv;

	rv = getMessageArcLabelsOut(PR_TRUE, getter_AddRefs(kThreadsArcsOutArray));
	if(NS_FAILED(rv)) return rv;

	rv = getMessageArcLabelsOut(PR_FALSE, getter_AddRefs(kNoThreadsArcsOutArray));
	if(NS_FAILED(rv)) return rv;

	rv = getFolderArcLabelsOut(getter_AddRefs(kFolderArcsOutArray));
  
	return rv;
}


NS_IMPL_ADDREF_INHERITED(nsMsgMessageDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgMessageDataSource, nsMsgRDFDataSource)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMsgMessageDataSource,
                                   nsMsgRDFDataSource,
                                   nsIFolderListener);

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
		return rv;
	}

	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source));
	if(folder)
	{
		rv = createFolderNode(folder, property, target);
		return rv;
	}
	return NS_RDF_NO_VALUE;
  
}

PRUnichar *
nsMsgMessageDataSource::GetString(const PRUnichar *aStringName)
{
	nsresult    res = NS_OK;
	PRUnichar   *ptrv = nsnull;

	if (!mStringBundle)
	{
		char    *propertyURL = MESSENGER_STRING_URL;

		NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			nsILocale   *locale = nsnull;
			res = sBundleService->CreateBundle(propertyURL, locale, getter_AddRefs(mStringBundle));
		}
	}

	if (mStringBundle)
		res = mStringBundle->GetStringFromName(aStringName, &ptrv);

	if ( NS_SUCCEEDED(res) && (ptrv) )
		return ptrv;
	else
		return nsCRT::strdup(aStringName);
}

//sender is the string we need to parse.  senderuserName is the parsed user name we get back.
nsresult nsMsgMessageDataSource::GetSenderName(const PRUnichar *sender, nsAutoString& senderUserName)
{
	//XXXOnce we get the csid, use Intl version
	nsresult rv = NS_OK;
	if(mHeaderParser)
	{

		nsXPIDLCString name;

    rv = mHeaderParser->ExtractHeaderAddressName("UTF-8", NS_ConvertUCS2toUTF8(sender), getter_Copies(name));
    if (NS_SUCCEEDED(rv) && (const char*)name)
      senderUserName.Assign(NS_ConvertUTF8toUCS2(name));
	}
	return rv;
}

NS_IMETHODIMP nsMsgMessageDataSource::GetSources(nsIRDFResource* property,
                                                nsIRDFNode* target,
                                                PRBool tv,
                                                nsISimpleEnumerator** sources)
{
  NS_ASSERTION(PR_FALSE, "Not implemented");
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
	if(kNC_MessageChild == property)
	{
		nsCOMPtr<nsIMessageView> messageView;
		rv = GetMessageView(getter_AddRefs(messageView));
    if (NS_SUCCEEDED(rv) && messageView) {

      rv = messageView->GetMessages(source, mWindow, targets);
      if(NS_FAILED(rv))
        return rv;
      //if we don't have any targets, we will have to continue.
      if(NS_SUCCEEDED(rv) && *targets)
        return rv;
    }
  }

	nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
	if (NS_SUCCEEDED(rv)) {

		if((kNC_Subject == property) || (kNC_Date == property) ||
				(kNC_Status == property) || (kNC_Flagged == property) ||
				(kNC_PriorityString == property) || (kNC_StatusString) ||
				(kNC_Priority == property) || (kNC_Size == property) ||
				(kNC_IsUnread == property) || (kNC_IsImapDeleted == property) || 
				(kNC_OrderReceived == property) || (kNC_HasAttachment == property) ||
				(kNC_MessageType == property) || (kNC_ThreadState == property))
		{
      rv = NS_NewSingletonEnumerator(targets, source);
		}
	}


	if(!*targets) {
	  //create empty cursor
    rv = NS_NewEmptyEnumerator(targets);
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
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source));
	if(folder)
		return DoFolderHasAssertion(folder, property, target, tv, hasAssertion);

	*hasAssertion = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP 
nsMsgMessageDataSource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
  *result = PR_FALSE;

  nsCOMPtr<nsIMessage> message(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    PRBool showThreads;
    rv = GetIsThreaded(&showThreads);
    // handle this failure gracefully - not all datasources have views.
   
    if (NS_SUCCEEDED(rv) && showThreads) {
      *result = (aArc == kNC_Total ||
                 aArc == kNC_Unread ||
                 aArc == kNC_MessageChild ||
				 aArc == kNC_ThreadState);
    }
    *result = (*result ||
               aArc == kNC_Subject ||
               aArc == kNC_Sender ||
               aArc == kNC_Recipient ||
               aArc == kNC_Date ||
               aArc == kNC_Status ||
               aArc == kNC_StatusString ||
               aArc == kNC_Flagged ||
               aArc == kNC_Priority ||
               aArc == kNC_PriorityString ||
               aArc == kNC_Size ||
               aArc == kNC_IsUnread ||
               aArc == kNC_HasAttachment ||
               aArc == kNC_IsImapDeleted ||
               aArc == kNC_MessageType ||
               aArc == kNC_OrderReceived);
    return NS_OK;
  }
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
    // handle this failure gracefully - not all datasources have views.
    
		if(NS_SUCCEEDED(rv) && showThreads)
		{
			arcsArray = kThreadsArcsOutArray;
		}
		else
		{
			arcsArray = kNoThreadsArcsOutArray;
		}
    rv = NS_NewArrayEnumerator(labels, arcsArray);
	}
	else 
	{
    rv = NS_NewEmptyEnumerator(labels);
	}

  NS_ENSURE_SUCCESS(rv, rv);
  
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::getFolderArcLabelsOut(nsISupportsArray **arcs)
{

	nsresult rv;
	rv = NS_NewISupportsArray(arcs);
	if(NS_FAILED(rv))
		return rv;

	(*arcs)->AppendElement(kNC_MessageChild);

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
		(*arcs)->AppendElement(kNC_ThreadState);
	}

	(*arcs)->AppendElement(kNC_Subject);
	(*arcs)->AppendElement(kNC_Sender);
	(*arcs)->AppendElement(kNC_Recipient);
	(*arcs)->AppendElement(kNC_Date);
	(*arcs)->AppendElement(kNC_Status);
	(*arcs)->AppendElement(kNC_StatusString);
	(*arcs)->AppendElement(kNC_Flagged);
	(*arcs)->AppendElement(kNC_Priority); 
	(*arcs)->AppendElement(kNC_PriorityString);
	(*arcs)->AppendElement(kNC_Size);
	(*arcs)->AppendElement(kNC_IsUnread);
	(*arcs)->AppendElement(kNC_HasAttachment);
	(*arcs)->AppendElement(kNC_IsImapDeleted);
	(*arcs)->AppendElement(kNC_MessageType);
	(*arcs)->AppendElement(kNC_OrderReceived);

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
                                      nsIEnumerator/*<nsIRDFResource>*/ ** commands)
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
                                      nsISimpleEnumerator/*<nsIRDFResource>*/ ** commands)
{
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgMessageDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/ * aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/ * aArguments,
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
nsMsgMessageDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/ * aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/ * aArguments)
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
  else if((aCommand == kNC_MarkThreadRead))
    rv = DoMarkThreadRead(aSources, aArguments);

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

	nsCOMPtr<nsIMessage> parentMessage;

	parentMessage = do_QueryInterface(parentItem);
	//If the parent isn't a message then we don't handle it.
	if(parentMessage)
	{
		return OnItemAddedOrRemovedFromMessage(parentMessage, item, viewString, added);
	}

	nsCOMPtr<nsIMsgFolder> parentFolder;

	parentFolder = do_QueryInterface(parentItem);
	//If the parent isn't a message then we don't handle it.
	if(parentFolder)
	{
		return OnItemAddedOrRemovedFromFolder(parentFolder, item, viewString, added);
	}

	return NS_OK;
}


nsresult nsMsgMessageDataSource::OnItemAddedOrRemovedFromMessage(nsIMessage *parentMessage, nsISupports *item, const char *viewString, PRBool added)
{
	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIRDFResource> parentResource;
	parentResource = do_QueryInterface(parentMessage);
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
			OnChangeThreadState(parentMessage);
		}
	}
	return NS_OK;
}

nsresult nsMsgMessageDataSource::OnItemAddedOrRemovedFromFolder(nsIMsgFolder *parentFolder, nsISupports *item, const char *viewString, PRBool added)
{

	nsresult rv;
	nsCOMPtr<nsIMessage> message;
	nsCOMPtr<nsIRDFResource> parentResource;

	parentResource = do_QueryInterface(parentFolder);
	//If it's not a resource, we don't handle it either
	if(!parentResource)
		return NS_OK;

	//If it is a message
	if(NS_SUCCEEDED(item->QueryInterface(NS_GET_IID(nsIMessage), getter_AddRefs(message))))
	{
		//If we're in a threaded view only do this if the view passed in is the thread view. Or if we're in 
		//a non threaded view only do this if the view passed in is the flat view.

		PRBool isThreaded, isThreadNotification;
		GetIsThreaded(&isThreaded);
		isThreadNotification = PL_strcmp(viewString, "threadMessageView") == 0;
		
		if((isThreaded && isThreadNotification) ||
			(!isThreaded && !isThreadNotification))
		{
			nsCOMPtr<nsIRDFNode> itemNode(do_QueryInterface(item, &rv));
			if(NS_SUCCEEDED(rv))
			{
				//Notify folders that a message was added or deleted.
				NotifyObservers(parentResource, kNC_MessageChild, itemNode, added, PR_FALSE);
			}
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
			nsAutoString newFlaggedStr;
			rv = createFlaggedStringFromFlag(newFlag, newFlaggedStr);
			if(NS_FAILED(rv))
				return rv;
			nsCOMPtr<nsIRDFNode> newNode;
			//rv = createNode(newFlaggedStr, newNode, getRDFService());
			rv = createNode(newFlaggedStr, getter_AddRefs(newNode), getRDFService());
			
			if(NS_SUCCEEDED(rv))
				rv = NotifyPropertyChanged(resource, kNC_Flagged, /*newFlaggedStr*/ newNode);
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgMessageDataSource::OnItemEvent(nsIFolder *aFolder, nsIAtom *aEvent)
{
  return NS_OK;
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
			{
				OnChangeUnreadMessageCount(message);
				OnChangeThreadState(message);
			}
		}

	}
	else if(changedFlag & MSG_FLAG_IMAP_DELETED)
	{
		OnChangeIsImapDeleted(resource, oldFlag, newFlag);
  }
	return NS_OK;
}

nsresult nsMsgMessageDataSource::OnChangeStatusString(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag)
{

	nsresult rv;
	nsCOMPtr<nsIRDFNode> newNode;

	rv = createStatusNodeFromFlag(newFlag, getter_AddRefs(newNode), PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

	rv = NotifyPropertyChanged(resource, kNC_Status, newNode);
  NS_ENSURE_SUCCESS(rv, rv);

	rv = createStatusNodeFromFlag(newFlag, getter_AddRefs(newNode), PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

	rv = NotifyPropertyChanged(resource, kNC_StatusString, newNode);
  NS_ENSURE_SUCCESS(rv, rv);
	
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

nsresult nsMsgMessageDataSource::OnChangeIsImapDeleted(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag)
{
	nsresult rv;
	nsCOMPtr<nsIRDFNode> newIsImapDeletedNode;
	
	newIsImapDeletedNode = (newFlag & MSG_FLAG_IMAP_DELETED) ? kTrueLiteral : kFalseLiteral;

	rv = NotifyPropertyChanged(resource, kNC_IsImapDeleted, newIsImapDeletedNode);

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

nsresult nsMsgMessageDataSource::OnChangeThreadState(nsIMessage *message)
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

	nsCOMPtr<nsIRDFNode> threadStateNode;
	rv = GetThreadStateNode(thread, getter_AddRefs(threadStateNode));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMessage> firstMessage;
	rv = GetThreadsFirstMessage(thread, folder, getter_AddRefs(firstMessage));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> firstMessageResource = do_QueryInterface(firstMessage);
	if(firstMessageResource)
		rv = NotifyPropertyChanged(firstMessageResource, kNC_ThreadState, threadStateNode);
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
	else if (kNC_RecipientCollation == property)
		rv = createMessageRecipientNode(message, PR_TRUE, target);
	else if (kNC_Recipient == property)
		rv = createMessageRecipientNode(message, PR_FALSE, target);
	else if ((kNC_Date == property))
		rv = createMessageDateNode(message, target);
	else if ((kNC_Status == property))
		rv = createMessageStatusNode(message, target, PR_FALSE);
	else if ((kNC_StatusString == property))
		rv = createMessageStatusNode(message, target, PR_TRUE);
	else if ((kNC_Flagged == property))
		rv = createMessageFlaggedNode(message, target, PR_FALSE);
	else if ((kNC_FlaggedSort == property))
		rv = createMessageFlaggedNode(message, target, PR_TRUE);
	else if ((kNC_Priority == property))
		rv = createMessagePriorityNode(message, target, PR_FALSE);
	else if ((kNC_PriorityString == property))
		rv = createMessagePriorityNode(message, target, PR_TRUE);
	else if ((kNC_PrioritySort == property))
		rv = createMessagePrioritySortNode(message, target);
	else if ((kNC_Size == property))
		rv = createMessageSizeNode(message, target, PR_FALSE);
	else if ((kNC_SizeSort == property))
		rv = createMessageSizeNode(message, target, PR_TRUE);
	else if (( kNC_Total == property))
		rv = createMessageTotalNode(message, target);
	else if ((kNC_Unread == property))
		rv = createMessageUnreadNode(message, target);
	else if((kNC_IsUnread == property))
		rv = createMessageIsUnreadNode(message, target, PR_FALSE);
	else if((kNC_IsUnreadSort == property))
		rv = createMessageIsUnreadNode(message, target, PR_TRUE);
	else if((kNC_HasAttachment == property))
		rv = createMessageHasAttachmentNode(message, target);
	else if((kNC_IsImapDeleted == property))
		rv = createMessageIsImapDeletedNode(message, target);
	else if((kNC_MessageType == property))
		rv = createMessageMessageTypeNode(message, target);
  else if ((kNC_MessageChild == property))
    rv = createMessageMessageChildNode(message, target);
  else if ((kNC_OrderReceived == property))
    rv = createMessageOrderReceivedNode(message, target);
  else if ((kNC_OrderReceivedSort == property))
    rv = createMessageOrderReceivedSortNode(message, target);
  else if ((kNC_ThreadState == property))
    rv = createMessageThreadStateNode(message, target);

  if (NS_FAILED(rv))
    return NS_RDF_NO_VALUE;

  return rv;
}

nsresult
nsMsgMessageDataSource::createFolderNode(nsIMsgFolder *folder,
                                         nsIRDFResource *property,
                                         nsIRDFNode **target)
{
	nsresult rv = NS_RDF_NO_VALUE;

	if ((kNC_MessageChild == property))
		rv = createFolderMessageChildNode(folder, target);

	if(NS_FAILED(rv))
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
					nsAutoString reStr(NS_LITERAL_STRING("Re: "));
					reStr.Append(subject);
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
				 rv = GetSenderName(sender, senderUserName);
			if(NS_SUCCEEDED(rv))
	       rv = createNode(senderUserName, target, getRDFService());
	}
  return rv;
}

nsresult
nsMsgMessageDataSource::createMessageRecipientNode(nsIMessage *message,
                                               PRBool sort,
                                               nsIRDFNode **target)
{
	nsresult rv = NS_OK;
	nsXPIDLString recipients;
	nsAutoString recipientUserName;
	if(sort)
	{
		rv = message->GetRecipientsCollationKey(getter_Copies(recipients));
		if(NS_SUCCEEDED(rv))
			rv = createNode(recipients, target, getRDFService());
	}
	else
	{
		rv = message->GetMime2DecodedRecipients(getter_Copies(recipients));
		if(NS_SUCCEEDED(rv))
			rv = GetSenderName(recipients, recipientUserName);
		if(NS_SUCCEEDED(rv))
			rv = createNode(recipientUserName, target, getRDFService());
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
                                               nsIRDFNode **target,
																							 PRBool needDisplayString)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	*target = kEmptyStringLiteral;
	if(flags & MSG_FLAG_REPLIED)
		*target = (needDisplayString) ? kRepliedLiteralDisplayString : kRepliedLiteral;
	else if(flags & MSG_FLAG_FORWARDED)
		*target = (needDisplayString) ? kForwardedLiteralDisplayString : kForwardedLiteral;
	else if(flags & MSG_FLAG_NEW)
		*target = (needDisplayString) ? kNewLiteralDisplayString : kNewLiteral;
	else if(flags & MSG_FLAG_READ)
		*target = (needDisplayString) ? kReadLiteralDisplayString : kReadLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageIsUnreadNode(nsIMessage *message, nsIRDFNode **target, PRBool sort)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	if(flags & MSG_FLAG_READ)
		*target = sort ? kHighestSortLiteral : kFalseLiteral;
	else
		*target =  sort ? kLowestSortLiteral : kTrueLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageHasAttachmentNode(nsIMessage *message, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	if(flags & MSG_FLAG_ATTACHMENT)
		*target = kTrueLiteral;
	else
		*target = kFalseLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageIsImapDeletedNode(nsIMessage *message, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;
	if(flags & MSG_FLAG_IMAP_DELETED)
		*target = kTrueLiteral;
	else
		*target = kFalseLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageMessageTypeNode(nsIMessage *message, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 type;
	rv = message->GetMessageType(&type);
	if(NS_FAILED(rv))
		return rv;
	if(type == nsIMessage::NewsMessage)
		*target = kNewsLiteral;
	else
		*target = kMailLiteral;

	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageOrderReceivedNode(nsIMessage *message, nsIRDFNode **target)
{
	//there's no visual string for order received.
	*target = kEmptyStringLiteral;
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageOrderReceivedSortNode(nsIMessage *message, nsIRDFNode **target)
{
	nsMsgKey msgKey;
	nsresult rv;

	rv = message->GetMessageKey(&msgKey);
	if(NS_FAILED(rv))
		return rv;

	createIntNode(msgKey, target, getRDFService());
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageThreadStateNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv = NS_OK;

	PRBool showThreads;

	GetIsThreaded(&showThreads);


	if(showThreads)
	{
		rv = GetMessageFolderAndThread(message, getter_AddRefs(folder), getter_AddRefs(thread));
		if(NS_SUCCEEDED(rv) && thread)
		{
			if(IsThreadsFirstMessage(thread, message))
			{
				rv = GetThreadStateNode(thread, target);
			}
		}
	}
	else
	{
		*target = kNoThreadLiteral;
		NS_IF_ADDREF(*target);
	}

	if(NS_FAILED(rv))
		return NS_RDF_NO_VALUE;

	return rv;

}

nsresult nsMsgMessageDataSource::GetThreadStateNode(nsIMsgThread *thread, nsIRDFNode **target)
{
	nsresult rv;
	PRUint32 numUnread, numChildren;

	*target = kNoThreadLiteral;

	rv = thread->GetNumUnreadChildren(&numUnread);
	if(NS_SUCCEEDED(rv))
	{
		rv = thread->GetNumChildren(&numChildren);
		if(NS_SUCCEEDED(rv))
		{
			//We need to have more than one child to show thread state.
			if(numChildren > 1)
			{
				//If we have at least one unread and two children then we should show the thread with unread
				//icon
				if(numUnread > 0)
					*target = kThreadWithUnreadLiteral;
				//otherwise just show the regular thread icon.
				else
					*target = kThreadLiteral;

			}
		}
	}

	NS_IF_ADDREF(*target);
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageFlaggedNode(nsIMessage *message,
                                               nsIRDFNode **target, PRBool sort)
{
	nsresult rv;
	PRUint32 flags;
	rv = message->GetFlags(&flags);
	if(NS_FAILED(rv))
		return rv;

	if(flags & MSG_FLAG_MARKED)
		*target = sort ? kHighestSortLiteral : kFlaggedLiteral;
	else 
		*target = sort ? kLowestSortLiteral : kUnflaggedLiteral;
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult 
nsMsgMessageDataSource::createStatusNodeFromFlag(PRUint32 flags, /*nsAutoString &statusStr*/ nsIRDFNode **node, PRBool needDisplayString)
{
	nsresult rv = NS_OK;
	*node = kEmptyStringLiteral;

	if(flags & MSG_FLAG_REPLIED)
		*node = (needDisplayString) ? kRepliedLiteralDisplayString : kRepliedLiteral;
	else if(flags & MSG_FLAG_FORWARDED)
		*node = (needDisplayString) ? kForwardedLiteralDisplayString : kForwardedLiteral;
	else if(flags & MSG_FLAG_NEW)
		*node = (needDisplayString) ? kNewLiteralDisplayString : kNewLiteral;
	else if(flags & MSG_FLAG_READ)
		*node = (needDisplayString) ? kReadLiteralDisplayString : kReadLiteral;

  NS_IF_ADDREF(*node);
	return rv;
}

nsresult 
nsMsgMessageDataSource::createFlaggedStringFromFlag(PRUint32 flags, nsAutoString &flaggedStr)
{
	nsresult rv = NS_OK;
	flaggedStr.Assign(NS_LITERAL_STRING(" "));
	if(flags & MSG_FLAG_MARKED)
		flaggedStr.Assign(NS_LITERAL_STRING("flagged"));
	else 
		flaggedStr.Assign(NS_LITERAL_STRING("unflagged"));
	return rv;
}

nsresult 
nsMsgMessageDataSource::createPriorityString(nsMsgPriorityValue priority, nsAutoString &priorityStr)
{
	nsresult rv = NS_OK;
	PRUnichar *prustr;
	priorityStr.Assign(NS_LITERAL_STRING(" "));
	switch (priority)
	{
		case nsMsgPriority::notSet:
		case nsMsgPriority::none:
		case nsMsgPriority::normal:
			priorityStr.Assign(NS_LITERAL_STRING(" "));
			break;
		case nsMsgPriority::lowest:
			//priorityStr = "Lowest";
			prustr = GetString(NS_LITERAL_STRING("Lowest"));
			priorityStr.Assign(prustr);
			break;
		case nsMsgPriority::low:
			//priorityStr = "Low";
			prustr = GetString(NS_LITERAL_STRING("Low"));
			priorityStr.Assign(prustr);
			break;
		case nsMsgPriority::high:
			//priorityStr = "High";
			prustr = GetString(NS_LITERAL_STRING("High"));
			priorityStr.Assign(prustr);
			break;
		case nsMsgPriority::highest:
			//priorityStr = "Highest";
			prustr = GetString(NS_LITERAL_STRING("Highest"));
			priorityStr.Assign(prustr);
			break;
	}
	if(prustr != nsnull)
		nsCRT::free(prustr);
	return rv;
}

nsresult
nsMsgMessageDataSource::createMessagePriorityNode(nsIMessage *message,
                                               nsIRDFNode **target, PRBool needDisplayString)
{
	nsresult rv;
	nsMsgPriorityValue priority;
	rv = message->GetPriority(&priority);
	if(NS_FAILED(rv))
		return rv;
	*target = kEmptyStringLiteral;
	switch (priority)
	{
		case nsMsgPriority::notSet:
		case nsMsgPriority::none:
		case nsMsgPriority::normal:
			*target = kEmptyStringLiteral;
			break;
		case nsMsgPriority::lowest:
			*target = (needDisplayString) ? kLowestLiteralDisplayString : kLowestLiteral; 
			break;
		case nsMsgPriority::low:
			*target = (needDisplayString) ? kLowLiteralDisplayString : kLowLiteral;
			break;
		case nsMsgPriority::high:
			*target = (needDisplayString) ? kHighLiteralDisplayString : kHighLiteral;
			break;
		case nsMsgPriority::highest:
			*target = (needDisplayString) ? kHighestLiteralDisplayString : kHighestLiteral;
			break;
	}
	
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessagePrioritySortNode(nsIMessage *message, nsIRDFNode **target)
{
	nsresult rv;
	nsMsgPriorityValue priority;
	rv = message->GetPriority(&priority);
	if(NS_FAILED(rv))
		return rv;
	*target = kNormalSortLiteral;
	switch (priority)
	{
		case nsMsgPriority::notSet:
		case nsMsgPriority::none:
		case nsMsgPriority::normal:
			*target = kNormalSortLiteral;
			break;
		case nsMsgPriority::lowest:
			*target = kLowestSortLiteral;
			break;
		case nsMsgPriority::low:
			*target = kLowSortLiteral;
			break;
		case nsMsgPriority::high:
			*target = kHighSortLiteral;
			break;
		case nsMsgPriority::highest:
			*target = kHighestSortLiteral;
			break;
	}
	
	NS_IF_ADDREF(*target);
	return NS_OK;
}

nsresult
nsMsgMessageDataSource::createMessageSizeNode(nsIMessage *message, nsIRDFNode **target, PRBool sort)
{
	nsresult rv;
	PRUint32 size;
	nsAutoString sizeStr;
	rv = message->GetMessageSize(&size);
	if(NS_FAILED(rv))
		return rv;
	if(size < 1024)
		size = 1024;
	PRUint32 sizeInKB = size/1024;

	if(!sort)
	{
    sizeStr.AppendInt(sizeInKB);
    sizeStr.AppendWithConversion("KB");
		rv = createNode(sizeStr, target, getRDFService());
	}
	else
		rv = createIntNode(sizeInKB, target, getRDFService());
	return rv;
}

nsresult nsMsgMessageDataSource::createMessageTotalNode(nsIMessage *message, nsIRDFNode **target)
{
	nsCOMPtr<nsIMsgFolder> folder;
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv;
	nsAutoString emptyString;

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
	nsAutoString emptyString;

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
	nsAutoString emptyString;
	PRUint32 numUnread;
	rv = thread->GetNumUnreadChildren(&numUnread);
	if(NS_SUCCEEDED(rv))
	{
		if(numUnread > 0)
			rv = createIntNode(numUnread, target, getRDFService());
		else
			rv = createNode(emptyString, target, getRDFService());
	}

	return rv;
}

nsresult nsMsgMessageDataSource::GetTotalChildrenNode(nsIMsgThread *thread, nsIRDFNode **target)
{
	nsresult rv;
	nsAutoString emptyString;
	PRUint32 numChildren;
	rv = thread->GetNumChildren(&numChildren);
	if(NS_SUCCEEDED(rv))
	{
		if(numChildren > 0)
			rv = createIntNode(numChildren, target, getRDFService());
		else
			rv = createNode(emptyString, target, getRDFService());
	}

	return rv;
}

nsresult
nsMsgMessageDataSource::createMessageMessageChildNode(nsIMessage *message,
                                                 nsIRDFNode **target)
{
  // this is slow, but for now, call GetTargets and then create
  // a node out of the first message, if any
  nsCOMPtr<nsIRDFResource> messageResource(do_QueryInterface(message));
  return createMessageChildNode(messageResource, target);
}

nsresult
nsMsgMessageDataSource::createFolderMessageChildNode(nsIMsgFolder *folder,
                                                 nsIRDFNode **target)
{
  nsCOMPtr<nsIRDFResource> folderResource(do_QueryInterface(folder));
  return createMessageChildNode(folderResource, target);
}

nsresult
nsMsgMessageDataSource::createMessageChildNode(nsIRDFResource *resource, nsIRDFNode** target)
{
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> messages;
  rv = GetTargets(resource, kNC_MessageChild, PR_TRUE,
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

	NS_ASSERTION(firstHdr,"firstHdr is null. can you reproduce this?  add info to bug #35567");
	if (!firstHdr) return NS_ERROR_FAILURE;

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

nsresult nsMsgMessageDataSource::DoMarkThreadRead(nsISupportsArray *folders, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;

  nsCOMPtr<nsISupports> supports;
  supports  = getter_AddRefs(folders->ElementAt(0));
  nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);

	nsCOMPtr<nsISupports> elem = getter_AddRefs(arguments->ElementAt(0));
	nsCOMPtr<nsIMsgThread> thread = do_QueryInterface(elem);
	if(folder && thread)
	{
		rv = folder->MarkThreadRead(thread);
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

nsresult nsMsgMessageDataSource::DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
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
		nsCOMPtr<nsIMessage> message(do_QueryInterface(target, &rv));
		if(NS_SUCCEEDED(rv))
			rv = folder->HasMessage(message, hasAssertion);
	}

	return NS_OK;


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




