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

#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIMsgHeaderParser.h"
#include "nsIFolderListener.h"
#include "nsMsgRDFDataSource.h"
#include "nsILocale.h"
#include "nsIDateTimeFormat.h"
#include "nsCOMPtr.h"

/**
 * The mail message source.
 */
class nsMsgMessageDataSource : public nsMsgRDFDataSource, public nsIFolderListener
{
private:
	PRBool				mInitialized;

	// The cached service managers
  
	nsIRDFService* mRDFService;
	nsIMsgHeaderParser *mHeaderParser;
  
public:
  
	NS_DECL_ISUPPORTS_INHERITED

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

	//nsIFolderListener
	NS_IMETHOD OnItemAdded(nsIFolder *parentFolder, nsISupports *item);

	NS_IMETHOD OnItemRemoved(nsIFolder *parentFolder, nsISupports *item);

	NS_IMETHOD OnItemPropertyChanged(nsISupports *item, const char *property,
									const char *oldValue, const char *newValue);

	NS_IMETHOD OnItemPropertyFlagChanged(nsISupports *item, const char *property,
									   PRUint32 oldFlag, PRUint32 newFlag);

	// caching frequently used resources
protected:

	nsresult  GetSenderName(nsAutoString& sender, nsAutoString *senderUserName);


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

	nsresult DoMarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
	nsresult DoMarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged);

	nsresult NotifyPropertyChanged(nsIRDFResource *resource,
								  nsIRDFResource *propertyResource,
								  const char *oldValue, const char *newValue);

	nsresult DoMessageHasAssertion(nsIMessage *message, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion);

	nsresult GetMessagesAndFirstFolder(nsISupportsArray *messages, nsIMsgFolder **folder,
														   nsISupportsArray **messageArray);

	static nsresult getMessageArcLabelsOut(nsIMessage *message,
                                         nsISupportsArray **arcs);
  

	static nsIRDFResource* kNC_Subject;
	static nsIRDFResource* kNC_Sender;
	static nsIRDFResource* kNC_Date;
	static nsIRDFResource* kNC_Status;
	static nsIRDFResource* kNC_Flagged;

	// commands
	static nsIRDFResource* kNC_MarkRead;
	static nsIRDFResource* kNC_MarkUnread;
	static nsIRDFResource* kNC_ToggleRead;
	static nsIRDFResource* kNC_MarkFlagged;
	static nsIRDFResource* kNC_MarkUnflagged;


};

PR_EXTERN(nsresult)
NS_NewMsgMessageDataSource(const nsIID& iid, void **result);
