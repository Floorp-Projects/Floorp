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
 */

#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIMsgHeaderParser.h"
#include "nsIFolderListener.h"
#include "nsMsgRDFDataSource.h"
#include "nsILocale.h"
#include "nsIDateTimeFormat.h"
#include "nsCOMPtr.h"
#include "MailNewsTypes.h"
#include "nsIMsgThread.h"

/**
 * The mail message source.
 */
class nsMsgMessageDataSource : public nsMsgRDFDataSource, public nsIFolderListener
{
private:
	PRBool				mInitialized;

	// The cached service managers
	nsIMsgHeaderParser *mHeaderParser;
  
public:
  
	NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFOLDERLISTENER
	nsMsgMessageDataSource(void);
	virtual ~nsMsgMessageDataSource (void);
	virtual nsresult Init();

	// nsIRDFDataSource methods
	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetSource(nsIRDFResource* property,
					   nsIRDFNode* target,
					   PRBool tv,
					   nsIRDFResource** source /* out */);

	NS_IMETHOD GetTarget(nsIRDFResource* source,
					   nsIRDFResource* property,
					   PRBool tv,
					   nsIRDFNode** target);

	NS_IMETHOD GetSources(nsIRDFResource* property,
						nsIRDFNode* target,
						PRBool tv,
						nsISimpleEnumerator** sources);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsISimpleEnumerator** targets);

	NS_IMETHOD Assert(nsIRDFResource* source,
					nsIRDFResource* property, 
					nsIRDFNode* target,
					PRBool tv);

	NS_IMETHOD Unassert(nsIRDFResource* source,
					  nsIRDFResource* property,
					  nsIRDFNode* target);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
						  nsIRDFResource* property,
						  nsIRDFNode* target,
						  PRBool tv,
						  PRBool* hasAssertion);

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
						 nsISimpleEnumerator** labels);

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
						  nsISimpleEnumerator** labels); 

	NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor);

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD GetAllCmds(nsIRDFResource* source,
							nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments);

protected:

	nsresult  GetSenderName(const PRUnichar *sender, nsAutoString *senderUserName);

	nsresult createMessageNode(nsIMessage *message, nsIRDFResource *property,
							 nsIRDFNode **target);

	nsresult createMessageNameNode(nsIMessage *message,
								 PRBool sort,
								 nsIRDFNode **target);
	nsresult createMessageSenderNode(nsIMessage *message,
								 PRBool sort,
								 nsIRDFNode **target);
	nsresult createMessageDateNode(nsIMessage *message,
								 nsIRDFNode **target);
	nsresult createMessageStatusNode(nsIMessage *message,
								   nsIRDFNode **target);
	nsresult createStatusStringFromFlag(PRUint32 flags, nsCAutoString &statusStr);

	nsresult createMessageFlaggedNode(nsIMessage *message,
								   nsIRDFNode **target);
	nsresult createFlaggedStringFromFlag(PRUint32 flags, nsCAutoString &statusStr);

	nsresult createMessagePriorityNode(nsIMessage *message,
								   nsIRDFNode **target);

	nsresult createPriorityString(nsMsgPriority priority, nsCAutoString &priorityStr);

	nsresult createMessageSizeNode(nsIMessage *message,
								   nsIRDFNode **target);

	nsresult createMessageIsUnreadNode(nsIMessage *message, nsIRDFNode **target);

	nsresult createMessageUnreadNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageTotalNode(nsIMessage *message, nsIRDFNode **target);
  nsresult createMessageMessageChildNode(nsIMessage* message, nsIRDFNode **target);
	nsresult GetMessageFolderAndThread(nsIMessage *message, nsIMsgFolder **folder,
										nsIMsgThread **thread);
	PRBool IsThreadsFirstMessage(nsIMsgThread *thread, nsIMessage *message);
	nsresult GetThreadsFirstMessage(nsIMsgThread *thread, nsIMsgFolder *folder, nsIMessage **message);

	nsresult DoMarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
	nsresult DoMarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged);

	nsresult DoMessageHasAssertion(nsIMessage *message, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetMessagesAndFirstFolder(nsISupportsArray *messages, nsIMsgFolder **folder,
														   nsISupportsArray **messageArray);

	nsresult getMessageArcLabelsOut(PRBool showThreads,
                                         nsISupportsArray **arcs);
  
	nsresult CreateLiterals(nsIRDFService *rdf);
	nsresult CreateArcsOutEnumerators();

	nsresult OnItemAddedOrRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString,
								  PRBool added);

	nsresult OnChangeStatus(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeStatusString(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeIsUnread(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeUnreadMessageCount(nsIMessage *message);
	nsresult OnChangeTotalMessageCount(nsIMessage *message);

	nsresult GetUnreadChildrenNode(nsIMsgThread *thread, nsIRDFNode **target);
	nsresult GetTotalChildrenNode(nsIMsgThread *thread, nsIRDFNode **target);

	static nsIRDFResource* kNC_Subject;
	static nsIRDFResource* kNC_SubjectCollation;
	static nsIRDFResource* kNC_Sender;
	static nsIRDFResource* kNC_SenderCollation;
	static nsIRDFResource* kNC_Date;
	static nsIRDFResource* kNC_Status;
	static nsIRDFResource* kNC_Flagged;
	static nsIRDFResource* kNC_Priority;
	static nsIRDFResource* kNC_Size;
	static nsIRDFResource* kNC_Total;
	static nsIRDFResource* kNC_Unread;
	static nsIRDFResource* kNC_MessageChild;
	static nsIRDFResource* kNC_IsUnread;


	// commands
	static nsIRDFResource* kNC_MarkRead;
	static nsIRDFResource* kNC_MarkUnread;
	static nsIRDFResource* kNC_ToggleRead;
	static nsIRDFResource* kNC_MarkFlagged;
	static nsIRDFResource* kNC_MarkUnflagged;

	//Cached literals
	nsCOMPtr<nsIRDFNode> kEmptyStringLiteral;
	nsCOMPtr<nsIRDFNode> kLowestLiteral;
	nsCOMPtr<nsIRDFNode> kLowLiteral;
	nsCOMPtr<nsIRDFNode> kHighLiteral;
	nsCOMPtr<nsIRDFNode> kHighestLiteral;
	nsCOMPtr<nsIRDFNode> kFlaggedLiteral;
	nsCOMPtr<nsIRDFNode> kUnflaggedLiteral;
	nsCOMPtr<nsIRDFNode> kRepliedLiteral;
	nsCOMPtr<nsIRDFNode> kForwardedLiteral;
	nsCOMPtr<nsIRDFNode> kNewLiteral;
	nsCOMPtr<nsIRDFNode> kReadLiteral;
	nsCOMPtr<nsIRDFNode> kTrueLiteral;
	nsCOMPtr<nsIRDFNode> kFalseLiteral;

  // message properties
  static nsIAtom *kStatusAtom;
  static nsIAtom *kFlaggedAtom;
  
	nsCOMPtr<nsISupportsArray> kThreadsArcsOutArray;
	nsCOMPtr<nsISupportsArray> kNoThreadsArcsOutArray;

	static nsrefcnt gMessageResourceRefCnt;

};


