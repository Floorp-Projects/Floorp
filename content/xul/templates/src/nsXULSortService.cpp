/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
  This file provides the implementation for the sort service manager.
 */

#include "nsIAtom.h"
#include "nsIXULSortService.h"
#include "nsString.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"

#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"

#include "nsVoidArray.h"
#include "rdf_qsort.h"

#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIRDFContentModelBuilder.h"

#include "rdf.h"


////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULSortServiceCID,      NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,     NS_IXULSORTSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,    NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);

static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kITextContentIID,        NS_ITEXT_CONTENT_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child);


////////////////////////////////////////////////////////////////////////
// ServiceImpl
//
//   This is the sort service.
//
class XULSortServiceImpl : public nsIXULSortService
{
protected:
    XULSortServiceImpl(void);
    virtual ~XULSortServiceImpl(void);

    static nsIXULSortService* gXULSortService; // The one-and-only sort service

private:
    static nsrefcnt	gRefCnt;
    static nsIAtom	*kTreeAtom;
    static nsIAtom	*kTreeBodyAtom;
    static nsIAtom	*kTreeCellAtom;
    static nsIAtom	*kTreeChildrenAtom;
    static nsIAtom	*kTreeColAtom;
    static nsIAtom	*kTreeItemAtom;
    static nsIAtom	*kResourceAtom;
    static nsIAtom	*kTreeContentsGeneratedAtom;
    static PRInt32	kNameSpaceID_XUL;
    static PRInt32	kNameSpaceID_RDF;

nsresult	FindTreeElement(nsIContent* aElement,nsIContent** aTreeElement);
nsresult	FindTreeBodyElement(nsIContent *tree, nsIContent **treeBody);
nsresult	GetSortColumnIndex(nsIContent *tree, nsString sortResource, PRInt32 *colIndex);
nsresult	GetTreeCell(nsIContent *node, PRInt32 colIndex, nsIContent **cell);
nsresult	GetTreeCellValue(nsIContent *node, nsString & value);
nsresult	RemoveAllChildren(nsIContent *node);
nsresult	SortTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel);
nsresult	PrintTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel);

public:

    static nsresult GetSortService(nsIXULSortService** result);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsISortService
    NS_IMETHOD DoSort(nsIDOMNode* node, const nsString& sortResource);
};

nsIXULSortService* XULSortServiceImpl::gXULSortService = nsnull;
nsrefcnt XULSortServiceImpl::gRefCnt = 0;

nsIAtom* XULSortServiceImpl::kTreeAtom;
nsIAtom* XULSortServiceImpl::kTreeBodyAtom;
nsIAtom* XULSortServiceImpl::kTreeCellAtom;
nsIAtom* XULSortServiceImpl::kTreeChildrenAtom;
nsIAtom* XULSortServiceImpl::kTreeColAtom;
nsIAtom* XULSortServiceImpl::kTreeItemAtom;
nsIAtom* XULSortServiceImpl::kResourceAtom;
nsIAtom* XULSortServiceImpl::kTreeContentsGeneratedAtom;
PRInt32  XULSortServiceImpl::kNameSpaceID_XUL;
PRInt32  XULSortServiceImpl::kNameSpaceID_RDF;

////////////////////////////////////////////////////////////////////////

XULSortServiceImpl::XULSortServiceImpl(void)
{
	NS_INIT_REFCNT();
	if (gRefCnt == 0)
	{
	        kTreeAtom            		= NS_NewAtom("tree");
	        kTreeBodyAtom        		= NS_NewAtom("treebody");
	        kTreeCellAtom        		= NS_NewAtom("treecell");
		kTreeChildrenAtom    		= NS_NewAtom("treechildren");
		kTreeColAtom         		= NS_NewAtom("treecol");
		kTreeItemAtom        		= NS_NewAtom("treeitem");
		kResourceAtom        		= NS_NewAtom("resource");
		kTreeContentsGeneratedAtom	= NS_NewAtom("treecontentsgenerated");
 
		nsresult rv;

	        // Register the XUL and RDF namespaces: these'll just retrieve
	        // the IDs if they've already been registered by someone else.
		nsINameSpaceManager* mgr;
		if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
		                           nsnull,
		                           kINameSpaceManagerIID,
		                           (void**) &mgr)))
		{

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

			rv = mgr->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
			NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

			rv = mgr->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
			NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");

			NS_RELEASE(mgr);
		}
		else
		{
			NS_ERROR("couldn't create namepsace manager");
		}
	}
	++gRefCnt;
}


XULSortServiceImpl::~XULSortServiceImpl(void)
{
	--gRefCnt;
	if (gRefCnt == 0)
	{
		NS_RELEASE(kTreeAtom);
		NS_RELEASE(kTreeBodyAtom);
		NS_RELEASE(kTreeCellAtom);
	        NS_RELEASE(kTreeChildrenAtom);
	        NS_RELEASE(kTreeColAtom);
	        NS_RELEASE(kTreeItemAtom);
	        NS_RELEASE(kResourceAtom);
	        NS_RELEASE(kTreeContentsGeneratedAtom);

		nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
		gXULSortService = nsnull;
	}
}


nsresult
XULSortServiceImpl::GetSortService(nsIXULSortService** mgr)
{
    if (! gXULSortService) {
        gXULSortService = new XULSortServiceImpl();
        if (! gXULSortService)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(gXULSortService);
    *mgr = gXULSortService;
    return NS_OK;
}



NS_IMETHODIMP_(nsrefcnt)
XULSortServiceImpl::AddRef(void)
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt)
XULSortServiceImpl::Release(void)
{
    return 1;
}


NS_IMPL_QUERY_INTERFACE(XULSortServiceImpl, kIXULSortServiceIID);


////////////////////////////////////////////////////////////////////////

nsresult
XULSortServiceImpl::FindTreeElement(nsIContent *aElement, nsIContent **aTreeElement)
{
	nsresult rv;
	nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));

	while (element)
	{
		PRInt32 nameSpaceID;
		if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID)))	return rv;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeAtom)
			{
				*aTreeElement = element;
				NS_ADDREF(*aTreeElement);
				return NS_OK;
			}
		}
		nsCOMPtr<nsIContent> parent;
		element->GetParent(*getter_AddRefs(parent));
		element = parent;
	}
	return(NS_ERROR_FAILURE);
}

nsresult
XULSortServiceImpl::FindTreeBodyElement(nsIContent *tree, nsIContent **treeBody)
{
        nsCOMPtr<nsIContent>	child;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
	nsresult		rv;

	if (NS_FAILED(rv = tree->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = tree->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return rv;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeBodyAtom)
			{
				*treeBody = child;
				NS_ADDREF(*treeBody);
				return NS_OK;
			}
		}
	}
	return(NS_ERROR_FAILURE);
}

nsresult
XULSortServiceImpl::GetSortColumnIndex(nsIContent *tree, nsString sortResource, PRInt32 *colIndex)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	*colIndex = 0;
	if (NS_FAILED(rv = tree->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = tree->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			if (tag.get() == kTreeColAtom)
			{
				nsString	colResource;

				if (NS_OK == child->GetAttribute(kNameSpaceID_RDF, kResourceAtom, colResource))
				{
					if (colResource == sortResource)
					{
						found = PR_TRUE;
						break;
					}
				}
				++(*colIndex);
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}

nsresult
XULSortServiceImpl::GetTreeCell(nsIContent *node, PRInt32 cellIndex, nsIContent **cell)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = node->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = node->ChildAt(childIndex, *getter_AddRefs(child))))	break;;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeCellAtom)
			{
				if (cellIndex == 0)
				{
					found = PR_TRUE;
					*cell = child;
					NS_ADDREF(*cell);
					break;
				}
				--cellIndex;
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}


nsresult
XULSortServiceImpl::GetTreeCellValue(nsIContent *node, nsString & val)
{
	PRBool			found = PR_FALSE;
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = node->ChildCount(numChildren)))	return(rv);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = node->ChildAt(childIndex, *getter_AddRefs(child))))	break;;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			nsString	tagName;
			tag->ToString(tagName);
			printf("Child #%d: tagName='%s'\n", childIndex, tagName.ToNewCString());
		}
		else if (nameSpaceID != kNameSpaceID_XUL)
		{
			nsITextContent	*text = nsnull;
			if (NS_SUCCEEDED(rv = child->QueryInterface(kITextContentIID, (void **)&text)))
			{
				nsTextFragment	*textFrags;
				PRInt32		numTextFrags;
				if (NS_SUCCEEDED(rv = text->GetText(textFrags, numTextFrags)))
				{
					char *value = textFrags->Get1b();
					PRInt32 len = textFrags->GetLength();
					val.SetString(value, len);
					found = PR_TRUE;
					break;
				}
			}
		}
	}
	return((found == PR_TRUE) ? NS_OK : NS_ERROR_FAILURE);
}

nsresult
XULSortServiceImpl::RemoveAllChildren(nsIContent *container)
{
        nsCOMPtr<nsIContent>	child;
	PRInt32			childIndex, numChildren;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
	if (numChildren == 0)	return(NS_OK);

	for (childIndex=numChildren-1; childIndex >= 0; childIndex--)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	break;
		container->RemoveChildAt(childIndex, PR_TRUE);
	}
	return(rv);
}


typedef	struct	_sortStruct	{
    PRBool			descendingSort;
} sortStruct, *sortPtr;


int	inplaceSortCallback(const void *data1, const void *data2, void *sortData);

int
inplaceSortCallback(const void *data1, const void *data2, void *sortData)
{
	char		*name1, *name2;
	name1 = *(char **)data1;
	name2 = *(char **)data2;
	_sortStruct	*sortPtr = (_sortStruct *)sortData;

	PRInt32		sortOrder=0;
	nsAutoString	str1(name1), str2(name2);

	sortOrder = str1.Compare(name2, PR_TRUE);
	if (sortPtr->descendingSort == PR_TRUE)
	{
		sortOrder = -sortOrder;
	}
	return(sortOrder);
}

nsresult
XULSortServiceImpl::SortTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel)
{
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;
	nsVoidArray		*childArray;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);

        if ((childArray = new nsVoidArray()) == nsnull)	return (NS_ERROR_OUT_OF_MEMORY);

	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	break;
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	break;
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;
			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))	return rv;
			if (tag.get() == kTreeItemAtom)
			{
			        nsIContent	*cell;
				if (NS_SUCCEEDED(rv = GetTreeCell(child, colIndex, &cell)))
				{
					if (cell)
					{
						nsString	val;
						if (NS_SUCCEEDED(rv = GetTreeCellValue(cell, val)))
						{
							// hack:  always append cell value 1st as
							//        that's what sort callback expects
							childArray->AppendElement(val.ToNewCString());
							childArray->AppendElement(child);
						}
					}
				}
			}
		}
	}
	unsigned long numElements = childArray->Count();
	if (numElements > 1)
	{
		nsIContent ** flatArray = new nsIContent*[numElements];
		if (flatArray)
		{
			_sortStruct		sortInfo;

			sortInfo.descendingSort = PR_TRUE;

			// flatten array of resources, sort them, then add as tree elements
			unsigned long loop;
		        for (loop=0; loop<numElements; loop++)
				flatArray[loop] = (nsIContent *)childArray->ElementAt(loop);
			rdf_qsort((void *)flatArray, numElements/2, 2 * sizeof(void *),
				inplaceSortCallback, (void *)&sortInfo);

			RemoveAllChildren(container);
			if (NS_FAILED(rv = container->UnsetAttribute(kNameSpaceID_None,
			                        kTreeContentsGeneratedAtom,
			                        PR_FALSE)))
			{
				printf("unable to clear contents-generated attribute\n");
			}
			
			// insert sorted children			
			numChildren = 0;
			for (loop=0; loop<numElements; loop+=2)
			{
				container->InsertChildAt((nsIContent *)flatArray[loop+1], numChildren++, PR_TRUE);
			}
		}
	}
	delete(childArray);
	return(rv);
}

nsresult
XULSortServiceImpl::PrintTreeChildren(nsIContent *container, PRInt32 colIndex, PRInt32 indentLevel)
{
	PRInt32			childIndex = 0, numChildren = 0, nameSpaceID;
        nsCOMPtr<nsIContent>	child;
	nsresult		rv;

	if (NS_FAILED(rv = container->ChildCount(numChildren)))	return(rv);
	for (childIndex=0; childIndex<numChildren; childIndex++)
	{
		if (NS_FAILED(rv = container->ChildAt(childIndex, *getter_AddRefs(child))))	return(rv);
		if (NS_FAILED(rv = child->GetNameSpaceID(nameSpaceID)))	return(rv);
		if (nameSpaceID == kNameSpaceID_XUL)
		{
			nsCOMPtr<nsIAtom> tag;

			if (NS_FAILED(rv = child->GetTag(*getter_AddRefs(tag))))
				return rv;
			nsString	tagName;
			tag->ToString(tagName);
			for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
			printf("Child #%d: tagName='%s'\n", childIndex, tagName.ToNewCString());

			PRInt32		attribIndex, numAttribs;
			child->GetAttributeCount(numAttribs);
			for (attribIndex = 0; attribIndex < numAttribs; attribIndex++)
			{
				PRInt32			attribNameSpaceID;
				nsCOMPtr<nsIAtom> 	attribAtom;

				if (NS_SUCCEEDED(rv = child->GetAttributeNameAt(attribIndex, attribNameSpaceID,
					*getter_AddRefs(attribAtom))))
				{
					nsString	attribName, attribValue;
					attribAtom->ToString(attribName);
					rv = child->GetAttribute(attribNameSpaceID, attribAtom, attribValue);
					if (rv == NS_CONTENT_ATTR_HAS_VALUE)
					{
						for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
						printf("Attrib #%d: name='%s' value='%s'\n", attribIndex,
							attribName.ToNewCString(),
							attribValue.ToNewCString());
					}
				}
			}
			if ((tag.get() == kTreeItemAtom) || (tag.get() == kTreeChildrenAtom) || (tag.get() == kTreeCellAtom))
			{
				PrintTreeChildren(child, colIndex, indentLevel+1);
			}
		}
		else
		{
			for (PRInt32 loop=0; loop<indentLevel; loop++) printf("    ");
			printf("(Non-XUL node)  ");
			nsITextContent	*text = nsnull;
			if (NS_SUCCEEDED(rv = child->QueryInterface(kITextContentIID, (void **)&text)))
			{
				for (PRInt32 indentLoop=0; indentLoop<indentLevel; indentLoop++) printf("    ");
				printf("(kITextContentIID)  ");

				nsTextFragment	*textFrags;
				PRInt32		numTextFrags;
				if (NS_SUCCEEDED(rv = text->GetText(textFrags, numTextFrags)))
				{
					char *val = textFrags->Get1b();
					PRInt32 len = textFrags->GetLength();
					if (val)	printf("value='%.*s'", len, val);
				}
			}
			printf("\n");
		}
	}
	return(NS_OK);
}

NS_IMETHODIMP
XULSortServiceImpl::DoSort(nsIDOMNode* node, const nsString& sortResource)
{
	printf("XULSortServiceImpl::DoSort entered!!!\n");

	PRInt32		childIndex, colIndex, numChildren, treeBodyIndex;
	nsIContent	*child, *contentNode, *treeNode, *treeBody, *treeParent;
	nsresult	rv;

	if (NS_FAILED(rv = node->QueryInterface(kIContentIID, (void**)&contentNode)))	return(rv);
	printf("Success converting dom node to content node.\n");

	if (NS_FAILED(rv = FindTreeElement(contentNode, &treeNode)))	return(rv);
	printf("found tree element\n");

	if (NS_FAILED(rv = GetSortColumnIndex(treeNode, sortResource, &colIndex)))	return(rv);
	printf("Sort column index is %d.\n", (int)colIndex);
				
	if (NS_FAILED(rv = FindTreeBodyElement(treeNode, &treeBody)))	return(rv);
	printf("Found tree body.\n");
	
	if (NS_FAILED(rv = treeBody->GetParent(treeParent)))	return(rv);
	printf("Found tree parent.\n");

	if (NS_FAILED(rv = treeParent->IndexOf(treeBody, treeBodyIndex)))	return(rv);
	printf("Found index of tree body in tree parent.\n");

	if (NS_FAILED(rv = treeParent->RemoveChildAt(treeBodyIndex, PR_TRUE)))	return(rv);
	printf("Removed tree body from parent.\n");

	if (NS_SUCCEEDED(rv = SortTreeChildren(treeBody, colIndex, 0)))
	{
		printf("Tree sorted.\n");
	}

	if (NS_SUCCEEDED(rv = treeBody->UnsetAttribute(kNameSpaceID_None,
		kTreeContentsGeneratedAtom,PR_FALSE)))
	{
	}
	if (NS_SUCCEEDED(rv = treeParent->UnsetAttribute(kNameSpaceID_None,
		kTreeContentsGeneratedAtom,PR_FALSE)))
	{
	}

	if (NS_FAILED(rv = treeParent->AppendChildTo(treeBody, PR_TRUE)))	return(rv);
	printf("Added tree body back into parent.\n");

#if 0
	if (NS_FAILED(rv = PrintTreeChildren(treeBody, colIndex, 0)))	return(rv);
	printf("Tree printed.\nDone.\n");
#endif
	return(NS_OK);
}

nsresult
NS_NewXULSortService(nsIXULSortService** mgr)
{
    return XULSortServiceImpl::GetSortService(mgr);
}
