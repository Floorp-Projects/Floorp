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

#include "nsIRDFCompositeDataSource.h"
#include "nsIMessageView.h"
#include "nsIMsgFolder.h"
#include "nsIRDFNode.h"
#include "nsVoidArray.h"

/**
 * The mail data source.
 */
class nsMessageViewDataSource : public nsIRDFCompositeDataSource, public nsIMessageView,
								public nsIRDFObserver
{
private:
	char*			mURI;
	nsVoidArray*	mObservers;
	PRBool			mInitialized;

public:
  
	NS_DECL_ISUPPORTS

	nsMessageViewDataSource(void);
	virtual ~nsMessageViewDataSource (void);


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


	//nsIRDFCompositeDataSource
	NS_IMETHOD AddDataSource(nsIRDFDataSource* source);

	NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);

	//nsIRDFObserver
	NS_IMETHOD OnAssert(nsIRDFResource* subject,
						nsIRDFResource* predicate,
						nsIRDFNode* object);

	NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

	//nsIMessageView
	NS_IMETHOD SetShowAll(PRBool showAll);
	NS_IMETHOD SetShowUnread(PRBool showUnread);
	NS_IMETHOD SetShowRead(PRBool showRead);
	NS_IMETHOD SetShowWatched(PRBool showWatched);

 
	// caching frequently used resources
protected:

	nsIRDFDataSource *mDataSource;
	PRUint32 mShowStatus;

	static nsIRDFResource* kNC_MessageChild;
	static nsIRDFResource* kNC_Status;
	static nsIRDFResource* kNC_Delete;

};

