/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgViewNavigationService_h
#define _nsMsgViewNavigationService_h

#include "nsIMsgViewNavigationService.h"

typedef struct infoStruct *navigationInfoPtr;

class nsMsgViewNavigationService : public nsIMsgViewNavigationService {

public:

  NS_DECL_ISUPPORTS

	nsMsgViewNavigationService();
	virtual ~nsMsgViewNavigationService();
	nsresult Init();
	NS_DECL_NSIMSGVIEWNAVIGATIONSERVICE

protected:
	nsresult CreateNavigationInfo(PRInt32 type, nsIDOMXULTreeElement *tree, nsIDOMNode *originalMessage, nsIRDFService *rdfService, nsIDOMXULDocument *document, PRBool wrapAround, PRBool isThreaded, PRBool checkStartMessage, navigationInfoPtr *info);
	nsresult FindNextMessageUnthreaded(navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindNextMessageInThreads(nsIDOMNode *startMessage, navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindNextInChildren(nsIDOMNode *parent, navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindNextInChildrenResources(nsIRDFResource *parentResource, navigationInfoPtr info, nsIRDFResource **nextResource);
	nsresult GetParentMessage(nsIDOMNode *message, nsIDOMNode **parentMessage);
	nsresult GetNextThread(navigationInfoPtr info, nsIDOMNode **nextThread);
	nsresult GetNextInThread(navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindPreviousMessage(navigationInfoPtr info, nsIDOMNode **previousMessage);
	nsresult GetNextMessageByThread(nsIDOMXULElement *startElement, navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindNextInThreadSiblings(nsIDOMNode *startMessage, navigationInfoPtr info, nsIDOMNode **nextMessage);
	nsresult FindNextInAncestors(nsIDOMNode *startMessage, navigationInfoPtr info, nsIDOMNode **nextMessage);


};

#endif

