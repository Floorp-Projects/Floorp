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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMsgViewNavigationService.h"
#include "nsCOMPtr.h"
#include "nsIDOMNodeList.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsXPIDLString.h"
#include "nsIMessage.h"
#include "nsIMsgFolder.h"
#include "nsIMsgThread.h"
#include "nsIDocument.h"


typedef PRBool (*navigationFunction)(nsIDOMNode *message, navigationInfoPtr info);
typedef PRBool (*navigationResourceFunction)(nsIRDFResource *message, navigationInfoPtr info);
//struct for keeping track of all info related to a navigation request.
typedef struct infoStruct
{
	navigationFunction navFunction;
	navigationFunction navThreadFunction;
	navigationResourceFunction navResourceFunction;	
	PRBool isThreaded;
	nsCOMPtr<nsIDOMXULTreeElement> tree;
	nsCOMPtr<nsIDOMNode> originalMessage;
	PRBool wrapAround;
	PRBool checkStartMessage;
	PRInt32 type;
	nsCOMPtr<nsIRDFService> rdfService;
	nsCOMPtr<nsIDOMXULDocument> document;
} navigationInfo;

static nsresult
GetMessageFromNode(nsIDOMNode *aNode, nsIRDFService *rdf, nsIMessage **aResult)
{
    nsresult rv;

    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aNode, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoString id;
    xulElement->GetId(id);

    nsCOMPtr<nsIRDFResource> messageResource;
    rdf->GetUnicodeResource(id.GetUnicode(), getter_AddRefs(messageResource));

    return messageResource->QueryInterface(NS_GET_IID(nsIMessage),
                                           (void **)aResult);
}

//navigation functions
static PRBool AnyMessageNavigationFunction(nsIDOMNode *message, navigationInfoPtr info)
{
	return PR_TRUE;
}

static PRBool
UnreadMessageNavigationFunction(nsIDOMNode *messageNode,
                                navigationInfoPtr info)
{
    nsCOMPtr<nsIMessage> message;
    GetMessageFromNode(messageNode, info->rdfService,
                       getter_AddRefs(message));
    PRUint32 flags=0;
    message->GetFlags(&flags);
    
    // return FALSE if the message is read
    return ((flags & MSG_FLAG_READ) != MSG_FLAG_READ);
}

static PRBool
FlaggedMessageNavigationFunction(nsIDOMNode *messageNode,
                                 navigationInfoPtr info)
{
    nsCOMPtr<nsIMessage> message;
    GetMessageFromNode(messageNode, info->rdfService,
                       getter_AddRefs(message));
    
    PRUint32 flags=0;
    message->GetFlags(&flags);

    // return TRUE if the message is flagged
    return ((flags & MSG_FLAG_MARKED) == MSG_FLAG_MARKED);
}

static PRBool
NewMessageNavigationFunction(nsIDOMNode *messageNode, navigationInfoPtr info)
{
    nsCOMPtr<nsIMessage> message;
    GetMessageFromNode(messageNode, info->rdfService,
                       getter_AddRefs(message));
    
    PRUint32 flags=0;
    message->GetFlags(&flags);

    // return TRUE if the message is new
    return ((flags & MSG_FLAG_NEW) == MSG_FLAG_NEW);
}

//resource functions
static nsresult GetMessageValue(nsIRDFResource *message, nsString& propertyURI, nsString& value, navigationInfoPtr info)
{
	nsresult rv;

	nsCOMPtr<nsIRDFCompositeDataSource> db;
	rv = info->tree->GetDatabase(getter_AddRefs(db));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> propertyResource;
  nsCAutoString propertyuriC;
  propertyuriC.AssignWithConversion(propertyURI);
	rv = info->rdfService->GetResource(propertyuriC, getter_AddRefs(propertyResource));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFNode> node;
	rv = db->GetTarget(message, propertyResource, PR_TRUE, getter_AddRefs(node));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(node);
	if(!literal)
		return NS_ERROR_FAILURE;

	if(literal)
	{
		nsXPIDLString valueStr;

		rv = literal->GetValue(getter_Copies(valueStr));
		if(NS_SUCCEEDED(rv))
			value.Assign(valueStr);
	}
	return rv;
}

static PRBool AnyMessageNavigationResourceFunction(nsIRDFResource *message, navigationInfoPtr info)
{

	return PR_TRUE;
}

static PRBool UnreadMessageNavigationResourceFunction(nsIRDFResource *message, navigationInfoPtr info)
{
	nsresult rv;
	nsAutoString isUnreadValue;
	nsAutoString unreadProperty; unreadProperty.AssignWithConversion("http://home.netscape.com/NC-rdf#IsUnread");

	rv = GetMessageValue(message, unreadProperty, isUnreadValue, info);
	if(NS_FAILED(rv))
		return PR_FALSE;

	return(isUnreadValue.EqualsWithConversion("true"));
}

static PRBool FlaggedMessageNavigationResourceFunction(nsIRDFResource *message, navigationInfoPtr info)
{
	nsresult rv;
	nsAutoString flaggedValue;
	nsAutoString flaggedProperty; flaggedProperty.AssignWithConversion("http://home.netscape.com/NC-rdf#Flagged");

	rv = GetMessageValue(message, flaggedProperty, flaggedValue, info);
	if(NS_FAILED(rv))
		return PR_FALSE;

	return(flaggedValue.EqualsWithConversion("flagged"));
}

static PRBool NewMessageNavigationResourceFunction(nsIRDFResource *message, navigationInfoPtr info)
{
	nsresult rv;
	nsAutoString statusValue;
	nsAutoString statusProperty; statusProperty.AssignWithConversion("http://home.netscape.com/NC-rdf#Status");

	rv = GetMessageValue(message, statusProperty, statusValue, info);
	if(NS_FAILED(rv))
		return PR_FALSE;

	return(statusValue.EqualsWithConversion("new"));
}

//Thread navigation functions.
static PRBool
UnreadThreadNavigationFunction(nsIDOMNode *messageNode, navigationInfoPtr info)
{
	nsresult rv;

    nsCOMPtr<nsIMessage> message;
    rv = GetMessageFromNode(messageNode, info->rdfService,
                            getter_AddRefs(message));
    if (NS_FAILED(rv))
        return PR_FALSE;
    
	nsCOMPtr<nsIMsgFolder> folder;
	rv = message->GetMsgFolder(getter_AddRefs(folder));
	if(NS_FAILED(rv))
		return PR_FALSE;

	nsCOMPtr<nsIMsgThread> thread;
	rv = folder->GetThreadForMessage(message, getter_AddRefs(thread));
	if(NS_FAILED(rv))
		return PR_FALSE;

	PRUint32 numUnreadChildren;
	rv = thread->GetNumUnreadChildren(&numUnreadChildren);
	if(NS_FAILED(rv))
		return PR_FALSE;

	return(numUnreadChildren != 0);
}

NS_IMPL_ISUPPORTS1(nsMsgViewNavigationService, nsIMsgViewNavigationService)

nsMsgViewNavigationService::nsMsgViewNavigationService()
{
	NS_INIT_ISUPPORTS();
}

nsMsgViewNavigationService::~nsMsgViewNavigationService()
{
}

nsresult nsMsgViewNavigationService::Init()
{
	return NS_OK;
}

//Finds the next message after originalMessage based on the type
NS_IMETHODIMP
nsMsgViewNavigationService::FindNextMessage(PRInt32 type,
                                            nsIDOMXULTreeElement *tree,
                                            nsIDOMXULElement *originalMessage,
                                            nsIRDFService *rdfService,
                                            nsIDOMXULDocument *document,
                                            PRBool wrapAround,
                                            PRBool isThreaded,
                                            nsIDOMXULElement ** nextMessage)
{
	nsresult rv = NS_OK;

	PRBool checkStartMessage = PR_FALSE;
	nsCOMPtr<nsIDOMNode> originalMessageNode;

	if(originalMessage)
	{
		checkStartMessage = PR_FALSE;
		rv = originalMessage->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(originalMessageNode));
		if(NS_FAILED(rv))
			return rv;
	}
	else
	{
		nsCOMPtr<nsIDOMXULElement> firstMessage;
		rv = FindFirstMessage(tree, getter_AddRefs(firstMessage));
		if(NS_FAILED(rv))
			return rv;

		if(firstMessage)
		{
			originalMessageNode = do_QueryInterface(firstMessage);
			if(!originalMessageNode)
				return NS_ERROR_FAILURE;

			checkStartMessage = PR_TRUE;
		}
	}	

	*nextMessage = nsnull;

	//if there are no messages then originalMessageNode will be null;
	if(originalMessageNode)
	{
		navigationInfoPtr info;
		rv = CreateNavigationInfo(type, tree, originalMessageNode, rdfService, document, wrapAround, isThreaded, checkStartMessage, &info);
		if(NS_FAILED(rv))
			return rv;

		nsCOMPtr<nsIDOMNode> next;
		if(!isThreaded)
		{
			rv = FindNextMessageUnthreaded(info, getter_AddRefs(next));
		}
		else
		{
			rv = FindNextMessageInThreads(info->originalMessage, info, getter_AddRefs(next));
		}

		if(next)
		{
			rv = next->QueryInterface(NS_GET_IID(nsIDOMXULElement), (void**) nextMessage);
			if(NS_FAILED(rv))
			{
				delete info;
				return rv;
			}
		}

		delete info;
	}
	return rv;
}

//Finds the previous message before the original message based on the type.
NS_IMETHODIMP
nsMsgViewNavigationService::FindPreviousMessage(PRInt32 type,
                                                nsIDOMXULTreeElement *tree,
                                                nsIDOMXULElement *originalMessage,
                                                nsIRDFService *rdfService,
                                                nsIDOMXULDocument *document,
                                                PRBool wrapAround,
                                                PRBool isThreaded,
                                                nsIDOMXULElement ** previousMessage)
{
	nsresult rv = NS_OK;

	PRBool checkStartMessage=PR_FALSE;
	nsCOMPtr<nsIDOMNode> originalMessageNode;

	if(originalMessage)
	{
		checkStartMessage = PR_FALSE;
		rv = originalMessage->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(originalMessageNode));
		if(NS_FAILED(rv))
			return rv;
	}
	
	navigationInfoPtr info;
	rv = CreateNavigationInfo(type, tree, originalMessageNode, rdfService, document, wrapAround, isThreaded, checkStartMessage, &info);
	if(NS_FAILED(rv))
		return rv;
	*previousMessage = nsnull;

	nsCOMPtr<nsIDOMNode> previous;

	rv = FindPreviousMessage(info, getter_AddRefs(previous));

	if(previous)
	{
		rv = previous->QueryInterface(NS_GET_IID(nsIDOMXULElement), (void**) previousMessage);
		if(NS_FAILED(rv))
		{
			delete info;
			return rv;
		}
	}

	delete info;
	return rv;
}

//Finds the next thread after the thread the original Message is in based on the type.
NS_IMETHODIMP
nsMsgViewNavigationService::FindNextThread(PRInt32 type,
                                           nsIDOMXULTreeElement *tree,
                                           nsIDOMXULElement *originalMessage,
                                           nsIRDFService *rdfService,
                                           nsIDOMXULDocument *document,
                                           PRBool wrapAround,
                                           PRBool checkOriginalMessage,
                                           nsIDOMXULElement ** nextThread)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIDOMNode> originalMessageNode = do_QueryInterface(originalMessage);
	if(!originalMessageNode)
		return NS_ERROR_FAILURE;

	navigationInfoPtr info;
	rv = CreateNavigationInfo(type, tree, originalMessageNode, rdfService, document, wrapAround, PR_TRUE, checkOriginalMessage, &info);
	if(NS_FAILED(rv))
		return rv;
	*nextThread = nsnull;

	nsCOMPtr<nsIDOMNode> next;
	rv = GetNextThread(info, getter_AddRefs(next));
	if(NS_FAILED(rv))
	{
		delete info;
		return rv;
	}

	if(next)
	{
		rv = next->QueryInterface(NS_GET_IID(nsIDOMXULElement), (void**)nextThread);
		if(NS_FAILED(rv))
		{
			delete info;
			return rv;
		}
	}
	delete info;
	return NS_OK;

}

//Given a top level message, returns the first message in the thread that matches the type.
NS_IMETHODIMP
nsMsgViewNavigationService::FindNextInThread(PRInt32 type,
                                             nsIDOMXULTreeElement *tree,
                                             nsIDOMXULElement *originalMessage,
                                             nsIRDFService *rdfService,
                                             nsIDOMXULDocument *document,
                                             nsIDOMXULElement ** nextMessage)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIDOMNode> originalMessageNode = do_QueryInterface(originalMessage);
	if(!originalMessageNode)
		return NS_ERROR_FAILURE;

	navigationInfoPtr info;
	rv = CreateNavigationInfo(type, tree, originalMessageNode, rdfService, document, PR_FALSE, PR_TRUE, PR_TRUE, &info);
	if(NS_FAILED(rv))
		return rv;
	*nextMessage = nsnull;

	nsCOMPtr<nsIDOMNode> next;
	rv = GetNextInThread(info, getter_AddRefs(next));
	if(NS_FAILED(rv))
	{
		delete info;
		return rv;
	}

	if(next)
	{
		rv = next->QueryInterface(NS_GET_IID(nsIDOMXULElement), (void**)nextMessage);
		if(NS_FAILED(rv))
		{
			delete info;
			return rv;
		}
	}
	delete info;
	return NS_OK;
}

//Finds the first message in the tree.
NS_IMETHODIMP
nsMsgViewNavigationService::FindFirstMessage(nsIDOMXULTreeElement *tree,
                                             nsIDOMXULElement ** firstMessage)
{
	nsresult rv;
	nsCOMPtr<nsIDOMNodeList> children;
	PRUint32 numChildren;

	rv = tree->GetChildNodes(getter_AddRefs(children));
	if(NS_FAILED(rv))
		return rv;

	rv = children->GetLength(&numChildren);
	if(NS_FAILED(rv))
		return rv;

	for(PRUint32 i = 0; i < numChildren; i++)
	{
		nsCOMPtr<nsIDOMNode> child;
		rv = children->Item(i, getter_AddRefs(child));
		if(NS_FAILED(rv))
			return rv;

		nsAutoString nodeName;
		rv = child->GetNodeName(nodeName);
		if(NS_FAILED(rv))
			return rv;

		if(nodeName.EqualsWithConversion("treechildren"))
		{
			//Get the treechildren's first child.  This is the first message
			nsCOMPtr<nsIDOMNode> firstChild;
			rv = child->GetFirstChild(getter_AddRefs(firstChild));
			if(NS_FAILED(rv))
				return rv;

			if(firstChild)
			{
				rv = firstChild->QueryInterface(NS_GET_IID(nsIDOMXULElement), (void**)firstMessage);
				return rv;
			}
		}

	}

	*firstMessage = nsnull;
	return NS_OK;	


}

//Creates the navigationInfo structure that contains all of the data necessary to do the navigation.
nsresult nsMsgViewNavigationService::CreateNavigationInfo(PRInt32 type, nsIDOMXULTreeElement *tree, nsIDOMNode *originalMessage,
														  nsIRDFService *rdfService, nsIDOMXULDocument *document, 
														  PRBool wrapAround, PRBool isThreaded, PRBool checkStartMessage,
														  navigationInfoPtr *info)
{

	navigationInfoPtr navInfo = new navigationInfo;
	if(!navInfo)
		return NS_ERROR_OUT_OF_MEMORY;

	navInfo->type = type;
	navInfo->tree = tree;

	navInfo->wrapAround = wrapAround;
	navInfo->isThreaded = isThreaded;
	navInfo->navFunction = nsnull;
	navInfo->navThreadFunction = nsnull;
	navInfo->navResourceFunction = nsnull;
	navInfo->rdfService = rdfService;
	navInfo->document = document;
	navInfo->checkStartMessage = checkStartMessage;
	navInfo->originalMessage = originalMessage;

	if(type == nsIMsgViewNavigationService::eNavigateUnread)
	{
		navInfo->navFunction = UnreadMessageNavigationFunction;
		navInfo->navResourceFunction = UnreadMessageNavigationResourceFunction;
		navInfo->navThreadFunction = UnreadThreadNavigationFunction;
	}
	else if(type == nsIMsgViewNavigationService::eNavigateNew)
	{
		navInfo->navFunction = NewMessageNavigationFunction;
		navInfo->navResourceFunction = NewMessageNavigationResourceFunction;
	}
	else if(type == nsIMsgViewNavigationService::eNavigateFlagged)
	{
		navInfo->navFunction = FlaggedMessageNavigationFunction;
		navInfo->navResourceFunction = FlaggedMessageNavigationResourceFunction;
	}
	else if(type == nsIMsgViewNavigationService::eNavigateAny)
	{
		navInfo->navFunction = AnyMessageNavigationFunction;
		navInfo->navResourceFunction = AnyMessageNavigationResourceFunction;
	}

	*info = navInfo;
	return NS_OK;
}

//Finds the next message in an unthreaded view.
nsresult nsMsgViewNavigationService::FindNextMessageUnthreaded(navigationInfoPtr info, nsIDOMNode **nextMessage)
{

	nsCOMPtr<nsIDOMNode> next;
	nsresult rv;

	//if we have to check the start message, check it now.
	if(info->checkStartMessage)
	{
		if(info->navFunction(info->originalMessage, info))
		{
			*nextMessage = info->originalMessage;
			NS_IF_ADDREF(*nextMessage);
			return NS_OK;
		}
	}

	rv = info->originalMessage->GetNextSibling(getter_AddRefs(next));
	if(NS_FAILED(rv))
		return rv;

		//In case we are currently the bottom message
	if(!next && info->wrapAround)
	{
		nsCOMPtr<nsIDOMNode> parent;
		rv =  info->originalMessage->GetParentNode(getter_AddRefs(parent));
		if(NS_FAILED(rv))
			return rv;

		rv = parent->GetFirstChild(getter_AddRefs(next));
		if(NS_FAILED(rv))
			return rv;
	}

	while(next && (next.get() != info->originalMessage.get()))
	{
		if(info->navFunction(next, info))
		 break;

		nsCOMPtr<nsIDOMNode> nextSibling;
		rv = next->GetNextSibling(getter_AddRefs(nextSibling));
		if(NS_FAILED(rv))
			return rv;
		next = nextSibling;

		/*If there's no nextMessage we may have to start from top.*/
		if(!next && (next.get()!= info->originalMessage.get()) && info->wrapAround)
		{
			nsCOMPtr<nsIDOMNode> parent;
			rv =  info->originalMessage->GetParentNode(getter_AddRefs(parent));
			if(NS_FAILED(rv))
				return rv;

			rv = parent->GetFirstChild(getter_AddRefs(next));
			if(NS_FAILED(rv))
				return rv;
		}
	}

	if(next.get() != info->originalMessage.get())
		*nextMessage = next;
	NS_IF_ADDREF(*nextMessage);
	return NS_OK;
}

NS_IMETHODIMP nsMsgViewNavigationService::EnsureDocumentIsLoaded(nsIDOMXULDocument *xulDocument)
{
	nsresult rv;

	nsCOMPtr<nsIDocument> document = do_QueryInterface(xulDocument);
	if(!document)
		return NS_ERROR_FAILURE;

	rv = document->FlushPendingNotifications();

	return rv;

}

//Finds the next message in a threaded view.
nsresult nsMsgViewNavigationService::FindNextMessageInThreads(nsIDOMNode *startMessage, navigationInfoPtr info, nsIDOMNode **nextMessage)
{
	nsCOMPtr<nsIDOMNode> next;
	nsCOMPtr<nsIDOMNode> nextChildMessage;
	nsresult rv;

	//First check startMessage if we are supposed to
	if(info->checkStartMessage)
	{
		if(info->navFunction(startMessage, info))
		{
			*nextMessage = startMessage;
			NS_IF_ADDREF(*nextMessage);
			return NS_OK;
		}
	}

	nsCOMPtr<nsIDOMNode> parent;
	rv = GetParentMessage(startMessage, getter_AddRefs(parent));
	if(NS_FAILED(rv))
		return rv;

	nsAutoString parentNodeName;
	rv = parent->GetNodeName(parentNodeName);
	if(NS_FAILED(rv))
		return rv;

	//if we're on the top level and a thread function has been passed in, we might be able to search faster.
	if(!parentNodeName.EqualsWithConversion("treeitem") && info->navThreadFunction)
	{
        nsCOMPtr<nsIDOMXULElement> startElement = do_QueryInterface(startMessage);
        if(!startElement)
            return NS_ERROR_FAILURE;

		return GetNextMessageByThread(startElement, info, nextMessage);

	}
	//Next, search the current messages children.
	rv = FindNextInChildren(startMessage, info, getter_AddRefs(nextChildMessage));
	if(NS_FAILED(rv))
		return rv;

	if(nextChildMessage)
	{
		*nextMessage = nextChildMessage;
		NS_IF_ADDREF(*nextMessage);
		return NS_OK;
	}

    //Next we need to search the current messages siblings

	rv = FindNextInThreadSiblings(startMessage, info, nextMessage);
	if(NS_FAILED(rv))
		return rv;

	if(*nextMessage)
		return NS_OK;


	//Finally, we need to find the next of the start message's ancestors that has a sibling
	rv = FindNextInAncestors(startMessage, info, nextMessage);
	if(NS_FAILED(rv))
		return rv;

	if(*nextMessage)
		return NS_OK;

	//otherwise it's the tree so we need to stop and potentially start from the beginning
	if(info->wrapAround)
	{
		nsCOMPtr<nsIDOMXULElement> firstElement;
		rv = FindFirstMessage(info->tree, getter_AddRefs(firstElement));
		if(NS_FAILED(rv))
			return rv;

		info->wrapAround = PR_FALSE;
		info->checkStartMessage = PR_TRUE;

		nsCOMPtr<nsIDOMNode> firstMessage = do_QueryInterface(firstElement);
		if(!firstMessage)
			return NS_ERROR_FAILURE;

		rv = FindNextMessageInThreads(firstMessage, info, nextMessage);
		return rv;
	}
	return NS_OK;
}

//Finds the next message in a message's children. Has to take different steps depending on if parent is open or closed.
nsresult nsMsgViewNavigationService::FindNextInChildren(nsIDOMNode *parent, navigationInfoPtr info, nsIDOMNode **nextMessage)
{
	nsresult rv;

	*nextMessage = nsnull;

	nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(parent);
	if(!parentElement)
		return NS_ERROR_FAILURE;

	PRBool isParentOpen;
	nsAutoString openAttr; openAttr.AssignWithConversion("open");
	nsAutoString openResult;

	rv = parentElement->GetAttribute(openAttr, openResult);
	if(NS_FAILED(rv))
		return rv;

	isParentOpen = (openResult.EqualsWithConversion("true"));
	//First we'll deal with the case where the parent is open.  In this case we can use DOM calls.
	if(isParentOpen)
	{
		nsCOMPtr<nsIDOMNodeList> children;
		rv = parent->GetChildNodes(getter_AddRefs(children));
		if(NS_FAILED(rv))
			return rv;

		PRUint32 numChildren;
		rv = children->GetLength(&numChildren);
		if(NS_FAILED(rv))
			return rv;

		//In this case we have treechildren
		if(numChildren == 2)
		{
			nsCOMPtr<nsIDOMNode> treechildren;
			
			rv = children->Item(1, getter_AddRefs(treechildren));
			if(NS_FAILED(rv))
				return rv;

			nsCOMPtr<nsIDOMNodeList> childMessages;
			rv = treechildren->GetChildNodes(getter_AddRefs(childMessages));
			PRUint32 numChildMessages;

			rv = childMessages->GetLength(&numChildMessages);
			if(NS_FAILED(rv))
				return rv;

			for(PRUint32 i = 0; i < numChildMessages; i++)
			{
				nsCOMPtr<nsIDOMNode> childMessage;
				rv = childMessages->Item(i, getter_AddRefs(childMessage));
				if(NS_FAILED(rv))
					return rv;

				//If we're at the original message again then stop.
				if(childMessage.get() == info->originalMessage.get())
				{
					*nextMessage = childMessage;
					NS_IF_ADDREF(*nextMessage);
					return NS_OK;
				}

				if(info->navFunction(childMessage, info))
				{
					*nextMessage = childMessage;
					NS_IF_ADDREF(*nextMessage);
					return NS_OK;
				}
				else
				{
					//if this child isn't the message, perhaps one of its children is.
					rv = FindNextInChildren(childMessage, info, nextMessage);
					if(NS_FAILED(rv))
						return rv;

					if(*nextMessage)
						return NS_OK;
				}
			}
		}
	}
	else
	{
		//We need to traverse the graph in rdf looking for the next resource that fits what we're searching for.
		nsAutoString idAttribute; idAttribute.AssignWithConversion("id");
		nsAutoString parentURI;

		rv = parentElement->GetAttribute(idAttribute, parentURI);
		if(NS_FAILED(rv))
			return rv;

		nsCOMPtr<nsIRDFResource> parentResource;
    nsCAutoString parenturiC;
    parenturiC.AssignWithConversion(parentURI);
		rv = info->rdfService->GetResource(parenturiC, getter_AddRefs(parentResource));
		if(NS_FAILED(rv))
			return rv;

		//If we find one, then we get the id and open up the parent and all of it's children.  Then we find the element
		//with the id in the document and return that.
		if(parentResource)
		{
			nsCOMPtr<nsIRDFResource> nextResource;
			rv = FindNextInChildrenResources(parentResource, info, getter_AddRefs(nextResource));
			if(NS_FAILED(rv))
				return rv;

			if(nextResource)
			{
				rv = OpenTreeitemAndDescendants(parentElement);
				if(NS_FAILED(rv))
					return rv;

				nsXPIDLCString nextURI;
				rv = nextResource->GetValue(getter_Copies(nextURI));
				if(NS_FAILED(rv))
					return rv;

				nsCOMPtr<nsIDOMElement> nextElement;
				rv = info->document->GetElementById(NS_ConvertASCIItoUCS2((const char*)nextURI), getter_AddRefs(nextElement));
				if(NS_FAILED(rv))
					return rv;
				if(nextElement)
				{
					rv = nextElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)nextMessage);
					return rv;
				}
			}
		}
	}
	return NS_OK;
}

//Finds the next message in the parentResource's children using RDF interfaces.
nsresult nsMsgViewNavigationService::FindNextInChildrenResources(nsIRDFResource *parentResource, navigationInfoPtr info, nsIRDFResource **nextResource)
{

	nsresult rv;

	*nextResource = nsnull;

	nsCOMPtr<nsIRDFCompositeDataSource> db;
	rv = info->tree->GetDatabase(getter_AddRefs(db));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> childrenResource;
	rv = info->rdfService->GetResource("http://home.netscape.com/NC-rdf#MessageChild", getter_AddRefs(childrenResource));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsISimpleEnumerator> childrenEnumerator;
	rv = db->GetTargets(parentResource, childrenResource, PR_TRUE, getter_AddRefs(childrenEnumerator));
	if(NS_FAILED(rv))
		return rv;

	if(childrenEnumerator)
	{
		PRBool hasMoreElements;
		while(NS_SUCCEEDED(childrenEnumerator->HasMoreElements(&hasMoreElements)) && hasMoreElements)
		{
			nsCOMPtr<nsISupports> childSupports;
			rv = childrenEnumerator->GetNext(getter_AddRefs(childSupports));
			if(NS_FAILED(rv))
				return rv;

			nsCOMPtr<nsIRDFResource> childResource = do_QueryInterface(childSupports);
			if(!childResource)
				return NS_ERROR_FAILURE;

			if(info->navResourceFunction(childResource, info))
			{
				*nextResource = childResource;
				NS_IF_ADDREF(*nextResource);
				return NS_OK;
			}

			rv = FindNextInChildrenResources(childResource, info, nextResource);
			if(NS_FAILED(rv))
				return rv;
			if(*nextResource)
				return NS_OK;

		}

	}

	return NS_OK;
}

//Given a treeitem, opens up the treeitem and opens up all of its descendants that have children.
NS_IMETHODIMP nsMsgViewNavigationService::OpenTreeitemAndDescendants(nsIDOMNode *treeitem)
{
	nsresult rv;
	nsCOMPtr<nsIDOMElement> treeitemElement = do_QueryInterface(treeitem);
	if(!treeitemElement)
		return NS_ERROR_FAILURE;

	nsAutoString openAttribute; openAttribute.AssignWithConversion("open");
	nsAutoString openResult;
	nsAutoString trueValue; trueValue.AssignWithConversion("true");

	rv = treeitemElement->GetAttribute(openAttribute, openResult);
	if(NS_FAILED(rv))
		return rv;

	if(openResult != trueValue)
	{
		rv = treeitemElement->SetAttribute(openAttribute, trueValue);
		if(NS_FAILED(rv))
			return rv;
	}

	nsCOMPtr<nsIDOMNodeList> treeitemChildNodes;
	rv = treeitem->GetChildNodes(getter_AddRefs(treeitemChildNodes));
	if(NS_FAILED(rv))
		return rv;

	PRUint32 numTreeitemChildren;
	rv = treeitemChildNodes->GetLength(&numTreeitemChildren);

	//if there's only one child then there are no treechildren so close it.
	if(numTreeitemChildren == 1)
	{
		rv = treeitemElement->SetAttribute(openAttribute, nsAutoString());
		if(NS_FAILED(rv))
			return rv;
	}
	else
	{
		for(PRUint32 i = 0; i < numTreeitemChildren; i++)
		{
			nsCOMPtr<nsIDOMNode> treeitemChild;
			rv = treeitemChildNodes->Item(i, getter_AddRefs(treeitemChild));
			if(NS_FAILED(rv))
				return rv;

			nsAutoString nodeName;
			rv = treeitemChild->GetNodeName(nodeName);
			if(NS_FAILED(rv))
				return rv;

			if(nodeName.EqualsWithConversion("treechildren"))
			{
				nsCOMPtr<nsIDOMNodeList> treechildrenChildNodes;
				rv = treeitemChild->GetChildNodes(getter_AddRefs(treechildrenChildNodes));
				if(NS_FAILED(rv))
					return rv;

				PRUint32 numTreechildrenChildren;
				rv = treechildrenChildNodes->GetLength(&numTreechildrenChildren);
				if(NS_FAILED(rv))
					return rv;
				
				for(PRUint32 j = 0; j < numTreechildrenChildren; j++)
				{
					nsCOMPtr<nsIDOMNode> treechildrenChild;
					rv = treechildrenChildNodes->Item(j, getter_AddRefs(treechildrenChild));
					if(NS_FAILED(rv))
						return rv;

					nsAutoString treechildrenChildNodeName;
					rv = treechildrenChild->GetNodeName(treechildrenChildNodeName);
					if(treechildrenChildNodeName.EqualsWithConversion("treeitem"))
					{
						nsCOMPtr<nsIDOMElement> childElement = do_QueryInterface(treechildrenChild);
						if(!childElement)
							return NS_ERROR_FAILURE;

						rv = childElement->SetAttribute(openAttribute, trueValue);
						if(NS_FAILED(rv))
							return rv;
						//Open up all of this items
						rv = OpenTreeitemAndDescendants(treechildrenChild);
					}
				}
			}
		}
	}


	return NS_OK;
}

//Given a message, returns its parent message.
nsresult nsMsgViewNavigationService::GetParentMessage(nsIDOMNode *message, nsIDOMNode **parentMessage)
{
	nsresult rv;
	//The message parent is the parent node of the parent node of the message
	nsCOMPtr<nsIDOMNode> firstParent;
	rv = message->GetParentNode(getter_AddRefs(firstParent));
	if(NS_FAILED(rv))
		return rv;

	rv = firstParent->GetParentNode(parentMessage);

	return rv;
}

//Finds the next thread based on info.
nsresult nsMsgViewNavigationService::GetNextThread(navigationInfoPtr info, nsIDOMNode **nextThread)
{
	nsresult rv;
	*nextThread = nsnull;
	if(info->checkStartMessage)
	{

		if(info->navThreadFunction(info->originalMessage, info))
		{
			*nextThread = info->originalMessage;
			NS_IF_ADDREF(*nextThread);
			return NS_OK;
		}
	}

	nsCOMPtr<nsIDOMNode> next;
	rv = info->originalMessage->GetNextSibling(getter_AddRefs(next));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIDOMNode> parent;
	nsCOMPtr<nsIDOMNode> firstChild;

	//Get the parent and first child of the parent in case we need to use it later.
	rv = info->originalMessage->GetParentNode(getter_AddRefs(parent));
	if(NS_FAILED(rv))
		return rv;

	rv = parent->GetFirstChild(getter_AddRefs(firstChild));
	if(NS_FAILED(rv))
		return rv;

	//In case we are currently the bottom message
	if(!next && info->wrapAround)
	{
		next = firstChild;
	}


	while(next && (next.get() != info->originalMessage.get()))
	{
		nsCOMPtr<nsIDOMXULElement> nextElement = do_QueryInterface(next);
		if(!nextElement)
			return NS_ERROR_FAILURE;

		if(info->navThreadFunction(nextElement, info))
		{
			break;
		}

		nsCOMPtr<nsIDOMNode> nextSibling;
		rv = next->GetNextSibling(getter_AddRefs(nextSibling));
		if(NS_FAILED(rv))
			return rv;
		next = nextSibling;

		//If there's no nextMessage we may have to start from top.
		if(!next && (next.get()!= info->originalMessage.get()) && info->wrapAround)
		{
			next = firstChild;
		}
	}

	*nextThread = next;
	NS_IF_ADDREF(*nextThread);
	return NS_OK;

}

//Finds the next message in a thread based on info.
nsresult nsMsgViewNavigationService::GetNextInThread(navigationInfoPtr info, nsIDOMNode **nextMessage)
{
	nsresult rv;
	*nextMessage = nsnull;

	if(info->navFunction(info->originalMessage, info))
	{
		*nextMessage = info->originalMessage;
		NS_IF_ADDREF(*nextMessage);
		return NS_OK;
	}
	else
	{
		rv = FindNextInChildren(info->originalMessage, info, nextMessage);
		if(NS_FAILED(rv)) 
			return rv;
	}

	return NS_OK;	
}

//Finds the previous message based on info.
nsresult nsMsgViewNavigationService::FindPreviousMessage(navigationInfoPtr info, nsIDOMNode **previousMessage)
{
	nsresult rv;
	*previousMessage = nsnull;

	nsCOMPtr<nsIDOMNode> previous;
	rv = info->originalMessage->GetPreviousSibling(getter_AddRefs(previous));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIDOMNode> parent;
	nsCOMPtr<nsIDOMNode> lastChild;

	rv = info->originalMessage->GetParentNode(getter_AddRefs(parent));
	if(NS_FAILED(rv))
		return rv;

	rv = parent->GetLastChild(getter_AddRefs(lastChild));
	if(NS_FAILED(rv))
		return rv;

	//In case we're already at the top
	if(!previous && info->wrapAround)
	{
		previous = lastChild;
	}

	while(previous && (previous.get() != info->originalMessage.get()))
	{
		if(info->navFunction(previous, info))
			break;

		nsCOMPtr<nsIDOMNode> previousSibling;
		rv = previous->GetPreviousSibling(getter_AddRefs(previousSibling));
		if(NS_FAILED(rv))
			return rv;
		previous = previousSibling;

		if(!previous && info->wrapAround)
		{
			previous = lastChild;
		}
	}

	*previousMessage = previous;
	NS_IF_ADDREF(*previousMessage);
	return NS_OK;

}

//Finds the next message by looking at each thread and determining if a thread has that type of message.  If it does,
//it returns the first message in the thread that fits criteria specified in info.
nsresult nsMsgViewNavigationService::GetNextMessageByThread(nsIDOMXULElement *startElement, navigationInfoPtr info,
															nsIDOMNode **nextMessage)
{
	nsresult rv;
	nsCOMPtr<nsIDOMXULElement> nextTopMessage;
	rv = FindNextThread(info->type, info->tree, startElement, info->rdfService,
						info->document, info->wrapAround, PR_TRUE, getter_AddRefs(nextTopMessage));
	if(NS_FAILED(rv))
		return rv;

	if(nextTopMessage)
	{
		nsCOMPtr<nsIDOMXULElement> nextMessageElement;

		rv = FindNextInThread(info->type, info->tree, nextTopMessage, info->rdfService,
							 info->document, getter_AddRefs(nextMessageElement));
		if(NS_FAILED(rv))
			return rv;

		if(nextMessageElement)
		{
			rv = nextMessageElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)nextMessage);
			return rv;
		}
	}

	return NS_OK;

}

//Given a message, searches from siblings onward to find the next message. This is for the threaded view.
nsresult nsMsgViewNavigationService::FindNextInThreadSiblings(nsIDOMNode *startMessage, navigationInfoPtr info,
															  nsIDOMNode **nextMessage)
{
	nsresult rv;
	*nextMessage = nsnull;

	nsCOMPtr<nsIDOMNode> next;
	nsCOMPtr<nsIDOMNode> nextChildMessage;

	rv = startMessage->GetNextSibling(getter_AddRefs(next));
	if(NS_FAILED(rv))
		return rv;

	while(next)
	{
		//In case we've already been here before
		if(next.get() == info->originalMessage.get())
		{
			*nextMessage = next;
			NS_IF_ADDREF(*nextMessage);
			return NS_OK;
		}

		if(info->navFunction(next, info))
		{
			*nextMessage = next;
			NS_IF_ADDREF(*nextMessage);
			return NS_OK;
		}

		rv = FindNextInChildren(next, info, getter_AddRefs(nextChildMessage));
		if(NS_FAILED(rv))
			return rv;

		if(nextChildMessage)
		{
			*nextMessage = nextChildMessage;
			NS_IF_ADDREF(*nextMessage);
			return NS_OK;
		}

		nsCOMPtr<nsIDOMNode> nextSibling;
		rv = next->GetNextSibling(getter_AddRefs(nextSibling));
		if(NS_FAILED(rv))
			return rv;
		next = nextSibling;
	}
	return NS_OK;
}

//Looks through startMessage's ancestors to find the next message.
nsresult nsMsgViewNavigationService::FindNextInAncestors(nsIDOMNode *startMessage, navigationInfoPtr info, nsIDOMNode **nextMessage)
{
	nsresult rv;
	nsCOMPtr<nsIDOMNode> parent;
	rv = GetParentMessage(startMessage, getter_AddRefs(parent));
	if(NS_FAILED(rv))
		return rv;

	nsAutoString parentNodeName;
	rv = parent->GetNodeName(parentNodeName);
	if(NS_FAILED(rv))
		return rv;

	while(parentNodeName.EqualsWithConversion("treeitem"))
	{
		nsCOMPtr<nsIDOMNode> nextSibling;
		rv = parent->GetNextSibling(getter_AddRefs(nextSibling));
		if(NS_FAILED(rv))
			return rv;

		if(nextSibling)
		{
			info->checkStartMessage = PR_TRUE;
			rv = FindNextMessageInThreads(nextSibling, info, nextMessage);
			return rv;
		}

		nsCOMPtr<nsIDOMNode> newParent;
		rv = GetParentMessage(parent, getter_AddRefs(newParent));
		if(NS_FAILED(rv))
			return rv;

		parent = newParent;
		rv = parent->GetNodeName(parentNodeName);
		if(NS_FAILED(rv))
			return rv;
	}

	return NS_OK;
}

