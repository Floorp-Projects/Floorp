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
#include "nsIMsgHeaderParser.h"

/**
 * The mail message source.
 */
class nsMsgMessageDataSource : public nsIRDFDataSource
{
private:
	char*         mURI;
	nsVoidArray*  mObservers;
	PRBool				mInitialized;

	// The cached service managers
  
	nsIRDFService* mRDFService;
	nsIMsgHeaderParser *mHeaderParser;
  
public:
  
	NS_DECL_ISUPPORTS

	nsMsgMessageDataSource(void);
	virtual ~nsMsgMessageDataSource (void);


	// nsIRDFDataSource methods
	NS_IMETHOD Init(const char* uri);

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
						nsIRDFAssertionCursor** sources);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsIRDFAssertionCursor** targets);

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

	NS_IMETHOD AddObserver(nsIRDFObserver* n);

	NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
						 nsIRDFArcsInCursor** labels);

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
						  nsIRDFArcsOutCursor** labels); 

	NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor);

	NS_IMETHOD Flush();

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments);

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


	static nsresult getMessageArcLabelsOut(nsIMessage *message,
                                         nsISupportsArray **arcs);
  

	static nsIRDFResource* kNC_Subject;
	static nsIRDFResource* kNC_Sender;
	static nsIRDFResource* kNC_Date;
	static nsIRDFResource* kNC_Status;

};