/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
 * Contributors:
 *    Joe Hewitt <hewitt@netscape.com> (Original Author)
 *
 * Note: This code is originally based on Alec Flett's DOM Viewer
 * at /mozilla/rdf/tests/domds. I trimmed and pruned a bit to remove
 * stuff I didn't need, fixed stuff that was broken, and added new stuff.
 *
 */

#include "nsDOMDataSource.h"
#include "dsinfo.h"

////////////////////////////////////////////////////////////////////////
// globals and constants

#define RDFEVENT_CHANGE 0
#define RDFEVENT_ASSERT 1
#define RDFEVENT_UNASSERT 2

#define INS_RDF_DOMROOT       "inspector:DOMRoot"
#define INS_RDF_CHILD         INS_NAMESPACE_URI "Child"
#define INS_RDF_ATTRIBUTE     INS_NAMESPACE_URI "Attribute"
#define INS_RDF_NODENAME      INS_NAMESPACE_URI "nodeName"
#define INS_RDF_NODEVALUE     INS_NAMESPACE_URI "nodeValue"
#define INS_RDF_NODETYPE      INS_NAMESPACE_URI "nodeType"
#define INS_RDF_PREFIX        INS_NAMESPACE_URI "prefix"
#define INS_RDF_LOCALNAME     INS_NAMESPACE_URI "localName"
#define INS_RDF_NAMESPACEURI  INS_NAMESPACE_URI "namespaceURI"
#define INS_RDF_ANONYMOUS     INS_NAMESPACE_URI "Anonymous"
#define INS_RDF_HASCHILDREN   INS_NAMESPACE_URI "HasChildren"

static PRInt32 gCurrentId = 0;

nsIRDFResource*    kINS_DOMRoot;
nsIRDFResource*    kINS_Child;
nsIRDFResource*    kINS_Attribute;
nsIRDFResource*    kINS_NodeName;
nsIRDFResource*    kINS_NodeValue;
nsIRDFResource*    kINS_NodeType;
nsIRDFResource*    kINS_Prefix;
nsIRDFResource*    kINS_LocalName;
nsIRDFResource*    kINS_NamespaceURI;
nsIRDFResource*    kINS_Anonymous;
nsIRDFResource*    kINS_HasChildren;

////////////////////////////////////////////////////////////////////////

nsDOMDataSource::nsDOMDataSource() :
  mDocument(nsnull),
  mObservers(nsnull)
{
	NS_INIT_REFCNT();

  mShowAnonymousContent = new PRBool;
  *mShowAnonymousContent = PR_TRUE;

  // show all node types by default
  mFilters = new PRUint16;
  *mFilters = 65535;

} 

nsresult
nsDOMDataSource::Init()
{
  //printf("creating nsDOMDataSource (%d) *************************\n", this);
  nsresult rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
  if (NS_FAILED(rv)) return rv;

  mRDFService->GetResource(INS_RDF_DOMROOT,             &kINS_DOMRoot);
  mRDFService->GetResource(INS_RDF_ATTRIBUTE,           &kINS_Attribute);
  mRDFService->GetResource(INS_RDF_CHILD,               &kINS_Child);
  mRDFService->GetResource(INS_RDF_NODENAME,            &kINS_NodeName);
  mRDFService->GetResource(INS_RDF_NODEVALUE,           &kINS_NodeValue);
  mRDFService->GetResource(INS_RDF_NODETYPE,            &kINS_NodeType);
  mRDFService->GetResource(INS_RDF_PREFIX,              &kINS_Prefix);
  mRDFService->GetResource(INS_RDF_LOCALNAME,           &kINS_LocalName);
  mRDFService->GetResource(INS_RDF_NAMESPACEURI,        &kINS_NamespaceURI);
  mRDFService->GetResource(INS_RDF_ANONYMOUS,           &kINS_Anonymous);
  mRDFService->GetResource(INS_RDF_HASCHILDREN,         &kINS_HasChildren);
  
  return NS_OK;
}

nsDOMDataSource::~nsDOMDataSource()
{
  //printf("destroying nsDOMDataSource (%d) *************************\n", this);
  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }
  
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);  
    doc->RemoveObserver(this);
  }

  // commenting this out temporarily because it was causing crashes.  
  // TODO: figure out the right way to release these
/*
  NS_IF_RELEASE(kINS_DOMRoot);
  NS_IF_RELEASE(kINS_Attribute);
  NS_IF_RELEASE(kINS_Child);
  NS_IF_RELEASE(kINS_NodeName);
  NS_IF_RELEASE(kINS_NodeValue);
  NS_IF_RELEASE(kINS_NodeType);
  NS_IF_RELEASE(kINS_LocalName);
  NS_IF_RELEASE(kINS_NamespaceURI);
  NS_IF_RELEASE(kINS_Anonymous);
  NS_IF_RELEASE(kINS_HasChildren);
*/
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS3(nsDOMDataSource,
                   nsIInsDOMDataSource,
                   nsIRDFDataSource,
                   nsIDocumentObserver);

////////////////////////////////////////////////////////////////////////
// nsIInsDOMDataSource

NS_IMETHODIMP
nsDOMDataSource::GetDocument(nsIDOMDocument **aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::SetDocument(nsIDOMDocument* aDocument)
{
  nsresult rv;

  nsCOMPtr<nsIDocument> doc;
  // remove old document observer
  if (mDocument) {
    doc = do_QueryInterface(mDocument, &rv);
    if (NS_FAILED(rv)) return rv;
    doc->RemoveObserver(this);
  }

  mDocument = aDocument;

  if (aDocument) {
    doc = do_QueryInterface(mDocument, &rv);
    // cache the binding manager
    rv = doc->GetBindingManager(&mBindingManager);
    if (NS_FAILED(rv)) return rv;
    // add new document observer
    doc = do_QueryInterface(mDocument, &rv);
    if (NS_FAILED(rv)) return rv;
    doc->AddObserver(this);
  } else {
    mBindingManager = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMDataSource::GetShowAnonymousContent(PRBool *aShowAnonymousContent)
{
  *aShowAnonymousContent = *mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMDataSource::SetShowAnonymousContent(PRBool aShowAnonymousContent)
{
  *mShowAnonymousContent = aShowAnonymousContent ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::AddFilterByType(PRUint16 aType, PRBool aExclusive)
{
  PRUint16 key = GetNodeTypeKey(aType);

  if (aExclusive) {
    *mFilters = key;
  } else {
    *mFilters |= key;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::RemoveFilterByType(PRUint16 aType)
{
  PRUint16 key = GetNodeTypeKey(aType);
  *mFilters -= key;
  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::IsFiltered(PRUint16 aType, PRBool* _retval)
{
  PRUint16 key = GetNodeTypeKey(aType);
  *_retval = *mFilters & key;

  return NS_OK;
}

nsresult
nsDOMDataSource::GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval)
{
  nsresult rv;
  nsISupportsKey* objKey;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aObject, &rv);
  if (NS_FAILED(rv)) 
    objKey = new nsISupportsKey(aObject);
  else 
    objKey = new nsISupportsKey(node);
  nsCOMPtr<nsISupports> supports = (nsISupports*) gDOMObjectTable.Get(objKey);
  
  if (supports) {
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports, &rv);
    *_retval = res;
  } else {
    // it's not in the hash table, so add it.
    char *uri = PR_smprintf(NS_DOMDSRESOURCE_ID "://%8.8X", gCurrentId++);
    
    rv = GetRDFService()->GetResource(uri, _retval);
    if (NS_FAILED(rv)) return rv;

    // add it to the hash table
    gDOMObjectTable.Put(objKey, *_retval);
    
    // now fill in the resource stuff
    nsCOMPtr<nsIDOMDSResource> nodeContainer = do_QueryInterface(*_retval, &rv);
    if (NS_FAILED(rv)) return rv;
    nodeContainer->SetObject(aObject);
  }

  return NS_OK;
}

PRBool
nsDOMDataSource::IsObjectInCache(nsISupports* aObject)
{
  nsISupportsKey* objKey = new nsISupportsKey(aObject);
  nsCOMPtr<nsISupports> supports = (nsISupports*) gDOMObjectTable.Get(objKey);
  
  if (supports) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }

}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsDOMDataSource::GetURI(char** aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (!aURI) return NS_ERROR_NULL_POINTER;

  *aURI = nsXPIDLCString::Copy("rdf:ins_domds");
	if (!*aURI) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
  *_retval = nsnull;
  
  nsresult rv;

  if (aSource == kINS_DOMRoot) {
    // It's the root, so it gets to be treated special
    return CreateRootTarget(aSource, aProperty, _retval);
  } else {
    // It's not the root, so it's just a regular node
    nsCOMPtr<nsIDOMDSResource> nodeContainer = do_QueryInterface(aSource);

    if (nodeContainer) {
      nsCOMPtr<nsISupports> supports;
	    nodeContainer->GetObject(getter_AddRefs(supports));

      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports, &rv);
      if (NS_SUCCEEDED(rv))
        return CreateDOMNodeTarget(node, aProperty, _retval);
    }
  }

  return rv;
}


NS_IMETHODIMP
nsDOMDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  nsArrayEnumerator* cursor = new nsArrayEnumerator(arcs);
  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;

  *_retval = cursor;
  NS_ADDREF(*_retval);

  if (!mDocument) return NS_OK;
  
  if (aSource == kINS_DOMRoot) {
    // start at the root...
    rv = GetTargetsForKnownObject(mDocument, aProperty, *mShowAnonymousContent, arcs);
  } else if (aProperty == kINS_Child) {
    // start with a node...
    nsCOMPtr<nsIDOMDSResource> dsRes;
    dsRes = do_QueryInterface(aSource, &rv);

    if (NS_SUCCEEDED(rv) && dsRes) {
      nsCOMPtr<nsISupports> supports;
      dsRes->GetObject(getter_AddRefs(supports));
      rv = GetTargetsForKnownObject(supports, aProperty, *mShowAnonymousContent, arcs);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::Change(nsIRDFResource *, nsIRDFResource *,
                           nsIRDFNode *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDOMDataSource::Move(nsIRDFResource *, nsIRDFResource *,
                         nsIRDFResource *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDOMDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
  nsresult rv;
  *_retval = PR_FALSE;

  nsCOMPtr<nsIDOMDSResource> sourceRes = do_QueryInterface(aSource, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMDSResource> targetRes = do_QueryInterface(aTarget, &rv);
  if (NS_FAILED(rv)) return rv;

  if (aProperty == kINS_DOMRoot) {
    *_retval = PR_TRUE;
  } else if (aProperty == kINS_Child) {
    nsCOMPtr<nsISupports> supports;
    sourceRes->GetObject(getter_AddRefs(supports));
    nsCOMPtr<nsIDOMNode> source = do_QueryInterface(supports);
    rv = targetRes->GetObject(getter_AddRefs(supports));
    if (!supports || NS_FAILED(rv)) return NS_OK;
    nsCOMPtr<nsIDOMNode> target = do_QueryInterface(supports);
    nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(supports);
    if (attr) {
      *_retval = HasAttribute(source, target) == aTruthValue;
      //printf("HasAssertion(%d) -> %d\n", aTruthValue, *_retval);
    } else 
      *_retval = HasChild(source, target) == aTruthValue;
  } else {
    *_retval = aTruthValue &&
               (aProperty == kINS_NodeName ||
                aProperty == kINS_NodeValue ||
                aProperty == kINS_NodeType ||
                aProperty == kINS_Prefix ||
                aProperty == kINS_LocalName ||
                aProperty == kINS_NamespaceURI ||
                aProperty == kINS_Anonymous ||
                aProperty == kINS_HasChildren);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers) {
    if ((mObservers = new nsVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mObservers->AppendElement(aObserver);
  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(aObserver);
  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  arcs->AppendElement(kINS_NodeName);
  arcs->AppendElement(kINS_NodeValue);
  arcs->AppendElement(kINS_NodeType);
  arcs->AppendElement(kINS_HasChildren);
  arcs->AppendElement(kINS_Child);
  
  nsArrayEnumerator* cursor = new nsArrayEnumerator(arcs);

  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *_retval = cursor;

  return NS_OK;
}


NS_IMETHODIMP
nsDOMDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsDOMDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP 
nsDOMDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
nsDOMDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  if (aArc == kINS_HasChildren) {
    nsCOMPtr<nsIDOMDSResource> nodeContainer = do_QueryInterface(aSource);
    nsCOMPtr<nsISupports> supports;
    nodeContainer->GetObject(getter_AddRefs(supports));
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(supports);    
    *result = HasChildren(node);
  } else {
    *result = aArc == kINS_DOMRoot ||
              aArc == kINS_NodeName ||
              aArc == kINS_NodeValue ||
              aArc == kINS_NodeType ||
              aArc == kINS_LocalName ||
              aArc == kINS_Prefix ||
              aArc == kINS_NamespaceURI ||
              aArc == kINS_Anonymous ||
              aArc == kINS_Attribute ||
              aArc == kINS_Child;
  }
    
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMETHODIMP
nsDOMDataSource::AttributeChanged(nsIDocument *aDocument, nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aHint)
{
  nsresult rv;

  PRBool filtered;
  rv = IsFiltered(2, &filtered);
  if (filtered) {
    // get the attribute node from the content node
    nsAutoString attrName;
    aAttribute->ToString(attrName);
    nsCOMPtr<nsIDOMElement> el = do_QueryInterface(aContent);
    nsCOMPtr<nsIDOMAttr> attr;
    el->GetAttributeNode(attrName, getter_AddRefs(attr));
    
    // the attribute is being changed or added
    if (attr) {
      PRBool exists = IsObjectInCache(attr);

      // get the resource for the attribute node
      nsCOMPtr<nsIRDFResource> attrRes;
      GetResourceForObject(attr, getter_AddRefs(attrRes));
      
      if (exists) {
        // the attribute's value is being changed
        //printf("CHANGING ATTRIBUTE\n");
        // create the literal node for the new property value
        nsCOMPtr<nsIRDFNode> propValue;
        rv = CreateDOMNodeTarget(attr, kINS_NodeValue, getter_AddRefs(propValue));

        NotifyObservers(attrRes, kINS_NodeValue, propValue, RDFEVENT_CHANGE);
      } else {
        // the attribute is new and was just added
        //printf("ADDING NEW ATTRIBUTE\n");

        nsCOMPtr<nsIRDFResource> contentRes;
        GetResourceForObject(aContent, getter_AddRefs(contentRes));

        NotifyObservers(contentRes, kINS_Child, attrRes, RDFEVENT_ASSERT);
      }
    } else {
      // the attribute is being removed
      //printf("REMOVING ATTRIBUTE\n");

      nsCOMPtr<nsIRDFResource> contentRes;
      GetResourceForObject(aContent, getter_AddRefs(contentRes));

      nsCOMPtr<nsIRDFResource> attrRes;
      FindAttrRes(aContent, aNameSpaceID, aAttribute, getter_AddRefs(attrRes));
      if (attrRes) {
        nsCOMPtr<nsIDOMDSResource> dsres = do_QueryInterface(attrRes);
        nsCOMPtr<nsIDOMAttr> attr;
        dsres->GetObject(getter_AddRefs(attr));
        RemoveResourceForObject(attr);
        NotifyObservers(contentRes, kINS_Child, attrRes, RDFEVENT_UNASSERT);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer, PRInt32 aNewIndexInContainer)
{
  nsresult rv;

  nsCOMPtr<nsIRDFResource> containerRes;
  rv = GetResourceForObject(aContainer, getter_AddRefs(containerRes));
  
  nsIContent* child;
  aContainer->ChildAt(aNewIndexInContainer, child);
  
  nsCOMPtr<nsIRDFResource> childRes;
  rv = GetResourceForObject(child, getter_AddRefs(childRes));

  NotifyObservers(containerRes, kINS_Child, childRes, RDFEVENT_ASSERT);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
{
  nsresult rv;

  nsCOMPtr<nsIRDFResource> containerRes;
  rv = GetResourceForObject(aContainer, getter_AddRefs(containerRes));
  
  nsCOMPtr<nsIRDFResource> childRes;
  rv = GetResourceForObject(aChild, getter_AddRefs(childRes));

  NotifyObservers(containerRes, kINS_Child, childRes, RDFEVENT_ASSERT);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::ContentReplaced(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aOldChild, nsIContent* aNewChild, PRInt32 aIndexInContainer)
{
  //printf("=== CONTENT REPLACED ===\n");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
{
  nsAutoString nodeName;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aContainer);
  node->GetNodeName(nodeName);
  //printf("=== CONTENT REMOVED (%s) ===\n", nodeName.ToNewCString());
  nsresult rv;

  nsCOMPtr<nsIRDFResource> containerRes;
  rv = GetResourceForObject(aContainer, getter_AddRefs(containerRes));

  nsCOMPtr<nsIRDFResource> childRes;
  rv = GetResourceForObject(aChild, getter_AddRefs(childRes));

  RemoveResourceForObject(aChild);
  NotifyObservers(containerRes, kINS_Child, childRes, RDFEVENT_UNASSERT);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataSource::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// nsDOMDataSource

nsIRDFService*
nsDOMDataSource::GetRDFService()
{
  if (!mRDFService) {
    nsresult rv;
    rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
    if (NS_FAILED(rv)) return nsnull;
  }
    
  return mRDFService;
}

/////////// 

nsresult
nsDOMDataSource::GetTargetsForKnownObject(nsISupports *aObject, nsIRDFResource *aProperty, PRBool aShowAnon, nsISupportsArray *aArcs)
{
  if (!aObject) return NS_ERROR_NULL_POINTER;
  nsresult rv;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aObject, &rv);
  if (NS_FAILED(rv)) return rv;
  return CreateDOMNodeArcs(node, aShowAnon, aArcs);
}

nsresult
nsDOMDataSource::CreateDOMNodeArcs(nsIDOMNode *aNode, PRBool aShowAnon, nsISupportsArray* aArcs)
{
  if (!aNode) return NS_OK;
  nsresult rv;

  // Need to do this test to prevent unfortunate NYI assertion 
  // on nsXULAttribute::GetChildNodes
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode, &rv);
  if (NS_FAILED(rv)) {
    // attribute nodes
    nsCOMPtr<nsIDOMNamedNodeMap> attrs;
    rv = aNode->GetAttributes(getter_AddRefs(attrs));
    if (NS_FAILED(rv)) return rv;
    rv = CreateDOMNamedNodeMapArcs(attrs, aArcs);

    // try to get the anonymous content
    if (aShowAnon) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode, &rv);
      if (content) {
        nsCOMPtr<nsIDOMNodeList> kids;
        mBindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
        if (!kids)
          mBindingManager->GetContentListFor(content, getter_AddRefs(kids));
        if (kids)
          CreateDOMNodeListArcs(kids, aArcs);
      } else {
        CreateChildNodeArcs(aNode, aArcs);
      }
    } else {
      CreateChildNodeArcs(aNode, aArcs);
    }
  }

  return rv;
}

nsresult
nsDOMDataSource::CreateChildNodeArcs(nsIDOMNode *aNode, nsISupportsArray *aArcs)
{
  nsresult rv;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(rv)) return rv;
  return CreateDOMNodeListArcs(childNodes, aArcs);
}

nsresult
nsDOMDataSource::CreateDOMNodeListArcs(nsIDOMNodeList *aNodeList, nsISupportsArray *aArcs)
{
  if (!aNodeList) return NS_OK;
  PRUint32 length=0;
  nsresult rv = aNodeList->GetLength(&length);
  if (NS_FAILED(rv)) return rv;
   
  PRUint32 i;
  PRUint16 type;
  PRBool filtered;
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = aNodeList->Item(i, getter_AddRefs(node));
    rv = node->GetNodeType(&type);
    rv = IsFiltered(type, &filtered);
    if (filtered) {
      nsCOMPtr<nsIRDFResource> resource;
      rv = GetResourceForObject(node, getter_AddRefs(resource));
      rv = aArcs->AppendElement(resource);
    }
  }

  return rv;
}

nsresult
nsDOMDataSource::CreateDOMNamedNodeMapArcs(nsIDOMNamedNodeMap *aNodeMap, nsISupportsArray *aArcs)
{
  if (!aNodeMap) return NS_OK;
  PRUint32 length=0;
  nsresult rv = aNodeMap->GetLength(&length);
  if (NS_FAILED(rv)) return rv;
   
  PRUint32 i;
  PRUint16 type;
  PRBool filtered;
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = aNodeMap->Item(i, getter_AddRefs(node));
    rv = node->GetNodeType(&type);
    rv = IsFiltered(type, &filtered);
    if (filtered) {
      nsCOMPtr<nsIRDFResource> resource;
      rv = GetResourceForObject(node, getter_AddRefs(resource));
      rv = aArcs->AppendElement(resource);
    }
  }

  return rv;
}

/////////// 

nsresult
nsDOMDataSource::CreateRootTarget(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode **aResult)
{
  nsAutoString str;
  str.AssignWithConversion("unknown");
  return CreateLiteral(str, aResult);
}

nsresult
nsDOMDataSource::CreateDOMNodeTarget(nsIDOMNode *aNode, nsIRDFResource *aProperty, nsIRDFNode **aResult)
{
  nsAutoString str;

  if (aProperty == kINS_NodeName) {
    aNode->GetNodeName(str);
  } else if (aProperty == kINS_NodeValue) {
    aNode->GetNodeValue(str);
  } else if (aProperty == kINS_NodeType) {
    PRUint16 type;
    aNode->GetNodeType(&type);
    str.AppendInt(PRInt32(type));
  } else if (aProperty == kINS_LocalName) {
    aNode->GetLocalName(str);
  } else if (aProperty == kINS_Prefix) {
    aNode->GetPrefix(str);
  } else if (aProperty == kINS_NamespaceURI) {
    aNode->GetNamespaceURI(str);
  } else if (aProperty == kINS_Anonymous) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      nsCOMPtr<nsIContent> bparent;
      content->GetBindingParent(getter_AddRefs(bparent));
      str.AssignWithConversion(bparent ? "true" : "false");
    } else {
      str.AssignWithConversion("false");
    }
  } else if (aProperty == kINS_HasChildren) {
    if (HasChildren(aNode))
      str.AssignWithConversion("true");
  } else {
    // can't figure out what this is... so dig into resource url
    nsAutoString fieldName;
    GetFieldNameFromRes(aProperty, &fieldName);
    
    // maybe it's an attribute reference?
    if (!fieldName.Find("@", PR_FALSE, 0, 1)) {
      // first make sure we're looking at an element node...
      nsCOMPtr<nsIDOMElement> el = do_QueryInterface(aNode);
      if (el) {
        nsAutoString attrName;
        fieldName.Right(attrName, fieldName.Length()-1);
        el->GetAttribute(attrName, str);
      }
    }
  }
  
  if (str.Length() > 0)
    return CreateLiteral(str, aResult);
  else
    return NS_OK;
}

/////////// 

nsresult
nsDOMDataSource::GetFieldNameFromRes(nsIRDFResource* aProperty, nsAutoString* aResult)
{
  char* resval;
  aProperty->GetValue(&resval);
  nsAutoString val;
  val.AssignWithConversion(resval);
  PRInt32 offset = val.Find(INS_NAMESPACE_URI, PR_FALSE, 0, 1);
  if (!offset) {
    PRInt32 len = strlen(INS_NAMESPACE_URI);
    val.Right(*aResult, val.Length() - len);
  }

  return NS_OK;
}

nsresult
nsDOMDataSource::CreateLiteral(nsString& str, nsIRDFNode **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> literal;
   
  PRUnichar* uniStr = str.ToNewUnicode();
  rv = GetRDFService()->GetLiteral(uniStr, getter_AddRefs(literal));
  nsMemory::Free(uniStr);
    
  *aResult = literal;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

PRUint16
nsDOMDataSource::GetNodeTypeKey(PRUint16 aType)
{
  PRUint16 result;
  switch (aType) {
    case nsIDOMNode::ELEMENT_NODE:
      result = 1;
      break;
    case nsIDOMNode::ATTRIBUTE_NODE:
      result = 2;
      break;
    case nsIDOMNode::TEXT_NODE:
      result = 4;
      break;
    case nsIDOMNode::CDATA_SECTION_NODE:
      result = 8;
      break;
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
      result = 61;
      break;
    case nsIDOMNode::ENTITY_NODE:
      result = 32;
      break;
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      result = 64;
      break;
    case nsIDOMNode::COMMENT_NODE:
      result = 128;
      break;
    case nsIDOMNode::DOCUMENT_NODE:
      result = 256;
      break;
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
      result = 512;
      break;
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      result = 1024;
      break;
    case nsIDOMNode::NOTATION_NODE:
      result = 2048;
      break;
    default:
      result = 0;
  }

  return result;
}

/////// Observer Notification

nsresult
nsDOMDataSource::NotifyObservers(nsIRDFResource *aSubject, nsIRDFResource *aProperty, nsIRDFNode *aObject, PRUint32 aType)
{
	if(mObservers)
	{
		nsDOMDSNotification note = { this, aSubject, aProperty, aObject };
		if (aType == RDFEVENT_CHANGE) {
			mObservers->EnumerateForwards(ChangeEnumFunc, &note);
		} else if (aType == RDFEVENT_ASSERT) {
			mObservers->EnumerateForwards(AssertEnumFunc, &note);
		} else if (aType == RDFEVENT_UNASSERT) {
			mObservers->EnumerateForwards(UnassertEnumFunc, &note);
    }
  }
	return NS_OK;
}

PRBool
nsDOMDataSource::ChangeEnumFunc(void *aElement, void *aData)
{
  nsDOMDSNotification *note = (nsDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnChange(note->datasource, note->subject, note->property, nsnull, note->object);
  return PR_TRUE;
}

PRBool
nsDOMDataSource::AssertEnumFunc(void *aElement, void *aData)
{
  nsDOMDSNotification *note = (nsDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnAssert(note->datasource, note->subject, note->property, note->object);
  return PR_TRUE;
}

PRBool
nsDOMDataSource::UnassertEnumFunc(void *aElement, void *aData)
{
  nsDOMDSNotification* note = (nsDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->datasource, note->subject, note->property, note->object);
  return PR_TRUE;
}

void 
nsDOMDataSource::DumpResourceValue(nsIRDFResource* aRes)
{
  const char* str;
  aRes->GetValueConst(&str);
  printf("Resource Value: %s\n", str);
}

nsresult 
nsDOMDataSource::FindAttrRes(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aAttrRes)
{
  nsDOMDSFindAttrInfo info = { nsnull, aContent, aNameSpaceID, aAttribute, aAttrRes };
  gDOMObjectTable.Enumerate(FindAttrResEnumFunc, &info);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// Since AttributeChanged (stupidly) gives me attributes by their
// namespace and atom, instead of the actual attribute node, we have to 
// search the object table for an attribute node that seems to match.  
// This is really nasty but an unfortunate consequence of the crappy 
// AttributeChanged signature.
///////////////////////////////////////////////////////////////////////////

PRBool
nsDOMDataSource::FindAttrResEnumFunc(nsHashKey *aKey, void *aData, void* closure)
{
  nsDOMDSFindAttrInfo* info = (nsDOMDSFindAttrInfo*)closure;
  nsCOMPtr<nsISupports> supports = (nsISupports*) gDOMObjectTable.Get(aKey);
  nsCOMPtr<nsIDOMDSResource> dsres = do_QueryInterface(supports);
  dsres->GetObject(getter_AddRefs(supports));
  if (supports) {
    nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(supports);
    if (attr) {
      nsCOMPtr<nsIDOMElement> parent;
      attr->GetOwnerElement(getter_AddRefs(parent));
      nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
      // make sure owner content element of attributes match
      if (info->mContent == content.get()) {
        nsCOMPtr<nsIRDFResource> res = do_QueryInterface(dsres);
        nsAutoString name1;
        info->mAttribute->ToString(name1);
        nsAutoString name2;
        attr->GetName(name2);
        // make sure the attribute matches
        // ToDo: make sure the namespace matches
        if (name1.Equals(name2))
          *info->mAttrRes = res;
      }
    }
  }
  return PR_TRUE;
}

PRBool
nsDOMDataSource::HasChild(nsIDOMNode* aContainer, nsIDOMNode* aChild)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = aContainer->GetChildNodes(getter_AddRefs(childNodes));
  PRUint32 length = 0;
  rv = childNodes->GetLength(&length);
  if (NS_FAILED(rv)) return rv;
   
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = childNodes->Item(i, getter_AddRefs(node));
    if (node.get() == aChild)
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsDOMDataSource::HasAttribute(nsIDOMNode* aContainer, nsIDOMNode* aAttr)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNamedNodeMap> attrs;
  rv = aContainer->GetAttributes(getter_AddRefs(attrs));
  PRUint32 length = 0;
  rv = attrs->GetLength(&length);
  if (NS_FAILED(rv)) return rv;
   
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = attrs->Item(i, getter_AddRefs(node));
    if (node.get() == aAttr)
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsDOMDataSource::HasChildren(nsIDOMNode* aNode)
{
  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    nsCOMPtr<nsIDOMNodeList> kids;
    if (*mShowAnonymousContent) {
      mBindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
      if (!kids)
        mBindingManager->GetContentListFor(content, getter_AddRefs(kids));
    } else {
      aNode->GetChildNodes(getter_AddRefs(kids));
    }
    if (kids) {
      PRUint32 count;
      kids->GetLength(&count);
      if (count > 0)
        return PR_TRUE;
    }
  } 

  return PR_FALSE;
}

nsresult 
nsDOMDataSource::RemoveResourceForObject(nsISupports* aObject)
{
  nsISupportsKey* objKey = new nsISupportsKey(aObject);
  gDOMObjectTable.Remove(objKey);
  return NS_OK;
}

