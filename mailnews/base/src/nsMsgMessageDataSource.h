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

#include "nsIStringBundle.h"

#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"


/**
 * The mail message source.
 */
class nsMsgMessageDataSource : public nsMsgRDFDataSource, public nsIFolderListener
{
private:
	PRBool				mInitialized;

	// The cached service managers
	nsCOMPtr<nsIMsgHeaderParser> mHeaderParser;
  
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

  NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result);

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
						 nsISimpleEnumerator** labels);

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
						  nsISimpleEnumerator** labels); 

	NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor);

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/ ** commands);
	NS_IMETHOD GetAllCmds(nsIRDFResource* source,
							nsISimpleEnumerator/*<nsIRDFResource>*/ ** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/ * aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/ * aArguments,
							  PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/ * aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/ * aArguments);

protected:
	PRUnichar *GetString(const PRUnichar *aStringName);
	//String bundle:
	nsCOMPtr<nsIStringBundle>   mStringBundle;

	nsresult  GetSenderName(const PRUnichar *sender, nsAutoString& senderUserName);

	nsresult createMessageNode(nsIMessage *message, nsIRDFResource *property,
							 nsIRDFNode **target);

	nsresult createFolderNode(nsIMsgFolder *folder, nsIRDFResource *property,
							 nsIRDFNode **target);

	nsresult createMessageNameNode(nsIMessage *message,
								 PRBool sort,
								 nsIRDFNode **target);
	nsresult createMessageSenderNode(nsIMessage *message,
								 PRBool sort,
								 nsIRDFNode **target);
	nsresult createMessageRecipientNode(nsIMessage *message,
                                               PRBool sort,
                                               nsIRDFNode **target);
	nsresult createMessageDateNode(nsIMessage *message,
								 nsIRDFNode **target);
	nsresult createMessageStatusNode(nsIMessage *message,
								   nsIRDFNode **target, PRBool needDisplayString);
	nsresult createStatusNodeFromFlag(PRUint32 flags, /*nsAutoString &statusStr*/ nsIRDFNode **node, PRBool needDisplayString);

	nsresult createMessageFlaggedNode(nsIMessage *message,
								   nsIRDFNode **target, PRBool sort);
	nsresult createFlaggedStringFromFlag(PRUint32 flags, nsAutoString &statusStr);

	nsresult createMessagePriorityNode(nsIMessage *message,
								   nsIRDFNode **target, PRBool needDisplayString);

	nsresult createMessagePrioritySortNode(nsIMessage *message, nsIRDFNode **target);

	nsresult createPriorityString(nsMsgPriorityValue priority, nsAutoString &priorityStr);

	nsresult createMessageSizeNode(nsIMessage *message, nsIRDFNode **target, PRBool sort);

	nsresult createMessageIsUnreadNode(nsIMessage *message, nsIRDFNode **target, PRBool sort);
	nsresult createMessageHasAttachmentNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageIsImapDeletedNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageMessageTypeNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageOrderReceivedNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageOrderReceivedSortNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageThreadStateNode(nsIMessage *message, nsIRDFNode **target);

	nsresult createMessageUnreadNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageTotalNode(nsIMessage *message, nsIRDFNode **target);
	nsresult createMessageMessageChildNode(nsIMessage* message, nsIRDFNode **target);
	nsresult createFolderMessageChildNode(nsIMsgFolder *folder, nsIRDFNode **target);
	nsresult createMessageChildNode(nsIRDFResource *resource, nsIRDFNode **target);
	nsresult GetMessageFolderAndThread(nsIMessage *message, nsIMsgFolder **folder,
										nsIMsgThread **thread);
	PRBool IsThreadsFirstMessage(nsIMsgThread *thread, nsIMessage *message);
	nsresult GetThreadsFirstMessage(nsIMsgThread *thread, nsIMsgFolder *folder, nsIMessage **message);

	nsresult DoMarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
	nsresult DoMarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged);

  nsresult DoMarkThreadRead(nsISupportsArray *folders, nsISupportsArray *arguments);

	nsresult DoMessageHasAssertion(nsIMessage *message, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult DoFolderHasAssertion(nsIMsgFolder *folder, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetMessagesAndFirstFolder(nsISupportsArray *messages, nsIMsgFolder **folder,
														   nsISupportsArray **messageArray);

	nsresult getMessageArcLabelsOut(PRBool showThreads,
                                         nsISupportsArray **arcs);

	nsresult getFolderArcLabelsOut(nsISupportsArray **arcs);

	nsresult CreateLiterals(nsIRDFService *rdf);
	nsresult CreateArcsOutEnumerators();

	nsresult OnItemAddedOrRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString,
								  PRBool added);

	nsresult OnItemAddedOrRemovedFromMessage(nsIMessage *parentMessage, nsISupports *item, const char *viewString,
								  PRBool added);

	nsresult OnItemAddedOrRemovedFromFolder(nsIMsgFolder *parentFolder, nsISupports *item, const char *viewString,
								  PRBool added);

	nsresult OnChangeStatus(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeStatusString(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeIsUnread(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeIsImapDeleted(nsIRDFResource *resource, PRUint32 oldFlag, PRUint32 newFlag);
	nsresult OnChangeUnreadMessageCount(nsIMessage *message);
	nsresult OnChangeTotalMessageCount(nsIMessage *message);
	nsresult OnChangeThreadState(nsIMessage *message);

	nsresult GetUnreadChildrenNode(nsIMsgThread *thread, nsIRDFNode **target);
	nsresult GetTotalChildrenNode(nsIMsgThread *thread, nsIRDFNode **target);
	nsresult GetThreadStateNode(nsIMsgThread *thread, nsIRDFNode **target);

  virtual void Cleanup();
  
	static nsIRDFResource* kNC_Subject;
	static nsIRDFResource* kNC_SubjectCollation;
	static nsIRDFResource* kNC_Sender;
	static nsIRDFResource* kNC_SenderCollation;
	static nsIRDFResource* kNC_Recipient;
	static nsIRDFResource* kNC_RecipientCollation;
	static nsIRDFResource* kNC_Date;
	static nsIRDFResource* kNC_Status;
	static nsIRDFResource* kNC_StatusString;
	static nsIRDFResource* kNC_Flagged;
	static nsIRDFResource* kNC_FlaggedSort;
	static nsIRDFResource* kNC_Priority;
	static nsIRDFResource* kNC_PriorityString;
	static nsIRDFResource* kNC_PrioritySort;
	static nsIRDFResource* kNC_Size;
	static nsIRDFResource* kNC_SizeSort;
	static nsIRDFResource* kNC_Total;
	static nsIRDFResource* kNC_Unread;
	static nsIRDFResource* kNC_MessageChild;
	static nsIRDFResource* kNC_IsUnread;
	static nsIRDFResource* kNC_IsUnreadSort;
	static nsIRDFResource* kNC_HasAttachment;
	static nsIRDFResource* kNC_IsImapDeleted;
	static nsIRDFResource* kNC_MessageType;
	static nsIRDFResource* kNC_OrderReceived;
	static nsIRDFResource* kNC_OrderReceivedSort;
	static nsIRDFResource* kNC_ThreadState;


	// commands
	static nsIRDFResource* kNC_MarkRead;
	static nsIRDFResource* kNC_MarkUnread;
	static nsIRDFResource* kNC_ToggleRead;
	static nsIRDFResource* kNC_MarkFlagged;
	static nsIRDFResource* kNC_MarkUnflagged;
  static nsIRDFResource* kNC_MarkThreadRead;

	//Cached literals
	nsCOMPtr<nsIRDFNode> kEmptyStringLiteral;
	nsCOMPtr<nsIRDFNode> kLowestLiteral;
	nsCOMPtr<nsIRDFNode> kLowestLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kLowLiteral;
	nsCOMPtr<nsIRDFNode> kLowLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kHighLiteral;
	nsCOMPtr<nsIRDFNode> kHighLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kHighestLiteral;
	nsCOMPtr<nsIRDFNode> kHighestLiteralDisplayString;

	nsCOMPtr<nsIRDFNode> kLowestSortLiteral;
	nsCOMPtr<nsIRDFNode> kLowSortLiteral;
	nsCOMPtr<nsIRDFNode> kNormalSortLiteral;
	nsCOMPtr<nsIRDFNode> kHighSortLiteral;
	nsCOMPtr<nsIRDFNode> kHighestSortLiteral;
	nsCOMPtr<nsIRDFNode> kFlaggedLiteral;
	nsCOMPtr<nsIRDFNode> kUnflaggedLiteral;

	nsCOMPtr<nsIRDFNode> kRepliedLiteral;
	nsCOMPtr<nsIRDFNode> kRepliedLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kForwardedLiteral;
	nsCOMPtr<nsIRDFNode> kForwardedLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kNewLiteral;
	nsCOMPtr<nsIRDFNode> kNewLiteralDisplayString;
	nsCOMPtr<nsIRDFNode> kReadLiteral;
	nsCOMPtr<nsIRDFNode> kReadLiteralDisplayString;

	nsCOMPtr<nsIRDFNode> kTrueLiteral;
	nsCOMPtr<nsIRDFNode> kFalseLiteral;
	nsCOMPtr<nsIRDFNode> kNewsLiteral;
	nsCOMPtr<nsIRDFNode> kMailLiteral;

	nsCOMPtr<nsIRDFNode> kNoThreadLiteral;
	nsCOMPtr<nsIRDFNode> kThreadLiteral;
	nsCOMPtr<nsIRDFNode> kThreadWithUnreadLiteral;


  // message properties
  static nsIAtom *kStatusAtom;
  static nsIAtom *kFlaggedAtom;
  
	nsCOMPtr<nsISupportsArray> kThreadsArcsOutArray;
	nsCOMPtr<nsISupportsArray> kNoThreadsArcsOutArray;
	nsCOMPtr<nsISupportsArray> kFolderArcsOutArray;

	static nsrefcnt gMessageResourceRefCnt;

};


