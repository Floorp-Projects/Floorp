/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "inDOMDataSource.h"
#include "dsinfo.h"
#include "inLayoutUtils.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMAttr.h"
#include "nsIDOMElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDocument.h"
#include "nsIXBLBinding.h"
#include "nsIBindingManager.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "prprf.h"

////////////////////////////////////////////////////////////////////////
// globals and constants

#define RDFEVENT_CHANGE 0
#define RDFEVENT_ASSERT 1
#define RDFEVENT_UNASSERT 2

#define IN_RDF_DOMROOT       "inspector:DOMRoot"
#define IN_RDF_CHILD         IN_NAMESPACE_URI "Child"
#define IN_RDF_ATTRIBUTE     IN_NAMESPACE_URI "Attribute"
#define IN_RDF_NODENAME      IN_NAMESPACE_URI "nodeName"
#define IN_RDF_NODEVALUE     IN_NAMESPACE_URI "nodeValue"
#define IN_RDF_NODETYPE      IN_NAMESPACE_URI "nodeType"
#define IN_RDF_PREFIX        IN_NAMESPACE_URI "prefix"
#define IN_RDF_LOCALNAME     IN_NAMESPACE_URI "localName"
#define IN_RDF_NAMESPACEURI  IN_NAMESPACE_URI "namespaceURI"
#define IN_RDF_ANONYMOUS     IN_NAMESPACE_URI "Anonymous"
#define IN_RDF_HASCHILDREN   IN_NAMESPACE_URI "HasChildren"

static PRInt32 gCurrentId = 0;

////////////////////////////////////////////////////////////////////////

inDOMDataSource::inDOMDataSource() :
  mDocument(nsnull),
  mObservers(nsnull)
{
	NS_INIT_REFCNT();

  mShowAnonymousContent = PR_FALSE;
  mShowSubDocuments = PR_FALSE;

  // show all node types by default
  mFilters = 65535;

} 

nsresult
inDOMDataSource::Init()
{
//  printf("creating inDOMDataSource (%d) *************************\n", this);
  nsresult rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), getter_AddRefs(mRDFService));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
  if (NS_FAILED(rv)) return rv;

  mRDFService->GetResource(IN_RDF_DOMROOT, getter_AddRefs(kINS_DOMRoot));
  mRDFService->GetResource(IN_RDF_ATTRIBUTE, getter_AddRefs(kINS_Attribute));
  mRDFService->GetResource(IN_RDF_CHILD, getter_AddRefs(kINS_Child));
  mRDFService->GetResource(IN_RDF_NODEVALUE, getter_AddRefs(kINS_NodeValue));
  mRDFService->GetResource(IN_RDF_NODETYPE, getter_AddRefs(kINS_NodeType));
  mRDFService->GetResource(IN_RDF_PREFIX, getter_AddRefs(kINS_Prefix));
  mRDFService->GetResource(IN_RDF_LOCALNAME, getter_AddRefs(kINS_LocalName));
  mRDFService->GetResource(IN_RDF_NAMESPACEURI, getter_AddRefs(kINS_NamespaceURI));
  mRDFService->GetResource(IN_RDF_ANONYMOUS, getter_AddRefs(kINS_Anonymous));
  mRDFService->GetResource(IN_RDF_HASCHILDREN, getter_AddRefs(kINS_HasChildren));
  mRDFService->GetResource(IN_RDF_NODENAME, getter_AddRefs(kINS_NodeName));
  
  return NS_OK;
}

inDOMDataSource::~inDOMDataSource()
{
//  printf("destroying inDOMDataSource (%d) *************************\n", this);
  if (mRDFService)
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
  
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);  
    doc->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS3(inDOMDataSource,
                   inIDOMDataSource,
                   nsIRDFDataSource,
                   nsIDocumentObserver);

////////////////////////////////////////////////////////////////////////
// inIDOMDataSource

NS_IMETHODIMP
inDOMDataSource::GetDocument(nsIDOMDocument **aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::SetDocument(nsIDOMDocument* aDocument)
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
    if (NS_FAILED(rv)) return rv;
    // add new document observer
    doc = do_QueryInterface(mDocument, &rv);
    if (NS_FAILED(rv)) return rv;
    doc->AddObserver(this);
  }

  return NS_OK;
}

NS_IMETHODIMP 
inDOMDataSource::GetShowAnonymousContent(PRBool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP 
inDOMDataSource::SetShowAnonymousContent(PRBool aShowAnonymousContent)
{
  mShowAnonymousContent = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::GetShowSubDocuments(PRBool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::SetShowSubDocuments(PRBool aShowSubDocuments)
{
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::AddFilterByType(PRUint16 aType, PRBool aExclusive)
{
  PRUint16 key = GetNodeTypeKey(aType);

  if (aExclusive) {
    mFilters = key;
  } else {
    mFilters |= key;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::RemoveFilterByType(PRUint16 aType)
{
  PRUint16 key = GetNodeTypeKey(aType);
  mFilters -= key;
  return NS_OK;
}


NS_IMETHODIMP
inDOMDataSource::IsFiltered(PRUint16 aType, PRBool* _retval)
{
  PRUint16 key = GetNodeTypeKey(aType);
  *_retval = mFilters & key;

  return NS_OK;
}

nsresult
inDOMDataSource::GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval)
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
    char *uri = PR_smprintf(IN_DOMRDFRESOURCE_ID "://%8.8X", gCurrentId++);
    
    rv = GetRDFService()->GetResource(uri, _retval);
    if (NS_FAILED(rv)) return rv;

    // add it to the hash table
    gDOMObjectTable.Put(objKey, *_retval);
    
    // now fill in the resource stuff
    nsCOMPtr<inIDOMRDFResource> nodeContainer = do_QueryInterface(*_retval, &rv);
    if (NS_FAILED(rv)) return rv;
    nodeContainer->SetObject(aObject);
  }

  return NS_OK;
}

PRBool
inDOMDataSource::IsObjectInCache(nsISupports* aObject)
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
inDOMDataSource::GetURI(char** aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (!aURI) return NS_ERROR_NULL_POINTER;

  *aURI = nsCRT::strdup("rdf:ins_domds");
  if (!*aURI) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
  *_retval = nsnull;
  
  nsresult rv;

  if (aSource == kINS_DOMRoot) {
    // It's the root, so it gets to be treated special
    return CreateRootTarget(aSource, aProperty, _retval);
  } else {
    // It's not the root, so it's just a regular node
    nsCOMPtr<inIDOMRDFResource> nodeContainer = do_QueryInterface(aSource);

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
inDOMDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
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
    rv = GetTargetsForKnownObject(mDocument, aProperty, mShowAnonymousContent, arcs);
  } else if (aProperty == kINS_Child) {
    // start with a node...
    nsCOMPtr<inIDOMRDFResource> dsRes;
    dsRes = do_QueryInterface(aSource, &rv);

    if (NS_SUCCEEDED(rv) && dsRes) {
      nsCOMPtr<nsISupports> supports;
      dsRes->GetObject(getter_AddRefs(supports));
      rv = GetTargetsForKnownObject(supports, aProperty, mShowAnonymousContent, arcs);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
inDOMDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::Change(nsIRDFResource *, nsIRDFResource *,
                           nsIRDFNode *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
inDOMDataSource::Move(nsIRDFResource *, nsIRDFResource *,
                         nsIRDFResource *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
inDOMDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
  nsresult rv;
  *_retval = PR_FALSE;

  nsCOMPtr<inIDOMRDFResource> sourceRes = do_QueryInterface(aSource, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<inIDOMRDFResource> targetRes = do_QueryInterface(aTarget, &rv);
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
inDOMDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers) {
    if ((mObservers = new nsAutoVoidArray()) == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  mObservers->AppendElement(aObserver);
  NS_IF_ADDREF(aObserver);
  return NS_OK;
}


NS_IMETHODIMP
inDOMDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(aObserver);
  NS_IF_RELEASE(aObserver);
  return NS_OK;
}


NS_IMETHODIMP
inDOMDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
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
inDOMDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
inDOMDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP 
inDOMDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
inDOMDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  if (aArc == kINS_HasChildren) {
    nsCOMPtr<inIDOMRDFResource> nodeContainer = do_QueryInterface(aSource);
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
inDOMDataSource::AttributeChanged(nsIDocument *aDocument,
                                  nsIContent* aContent,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aModType,
                                  PRInt32 aHint)
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
        nsCOMPtr<inIDOMRDFResource> dsres = do_QueryInterface(attrRes);
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
inDOMDataSource::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer, PRInt32 aNewIndexInContainer)
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
inDOMDataSource::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
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
inDOMDataSource::ContentReplaced(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aOldChild, nsIContent* aNewChild, PRInt32 aIndexInContainer)
{
  //printf("=== CONTENT REPLACED ===\n");
  ContentRemoved(aDocument, aContainer, aOldChild, aIndexInContainer);
  ContentInserted(aDocument, aContainer, aOldChild, aIndexInContainer);
  return NS_OK;
}

NS_IMETHODIMP
inDOMDataSource::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
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
inDOMDataSource::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// inDOMDataSource

nsIRDFService*
inDOMDataSource::GetRDFService()
{
  if (!mRDFService) {
    nsresult rv;
    rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), getter_AddRefs(mRDFService));
    if (NS_FAILED(rv)) return nsnull;
  }
    
  return mRDFService;
}

/////////// 

nsresult
inDOMDataSource::GetTargetsForKnownObject(nsISupports *aObject, nsIRDFResource *aProperty, PRBool aShowAnon, nsISupportsArray *aArcs)
{
  if (!aObject) return NS_ERROR_NULL_POINTER;
  nsresult rv;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aObject, &rv);
  if (NS_FAILED(rv)) return rv;
  return CreateDOMNodeArcs(node, aShowAnon, aArcs);
}

nsresult
inDOMDataSource::CreateDOMNodeArcs(nsIDOMNode *aNode, PRBool aShowAnon, nsISupportsArray* aArcs)
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

    if (mShowSubDocuments) {
      nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aNode);
      if (domdoc) {
        nsCOMPtr<nsIDOMNodeList> kids;
        domdoc->GetChildNodes(getter_AddRefs(kids));
        CreateDOMNodeListArcs(kids, aArcs);
        return NS_OK;
      }
    }

    // try to get the anonymous content
    if (aShowAnon) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode, &rv);
      if (content) {
        nsCOMPtr<nsIDOMNodeList> kids;
        nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
        bindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
        if (!kids)
          bindingManager->GetContentListFor(content, getter_AddRefs(kids));
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
inDOMDataSource::CreateChildNodeArcs(nsIDOMNode *aNode, nsISupportsArray *aArcs)
{
  nsresult rv;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(rv)) return rv;
  return CreateDOMNodeListArcs(childNodes, aArcs);
}

nsresult
inDOMDataSource::CreateDOMNodeListArcs(nsIDOMNodeList *aNodeList, nsISupportsArray *aArcs)
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
inDOMDataSource::CreateDOMNamedNodeMapArcs(nsIDOMNamedNodeMap *aNodeMap, nsISupportsArray *aArcs)
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
inDOMDataSource::CreateRootTarget(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode **aResult)
{
  nsAutoString str;
  str.AssignWithConversion("unknown");
  return CreateLiteral(str, aResult);
}

nsresult
inDOMDataSource::CreateDOMNodeTarget(nsIDOMNode *aNode, nsIRDFResource *aProperty, nsIRDFNode **aResult)
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
    PRUint16 type;
    aNode->GetNodeType(&type);
    if (type == 1 || type == 2)
      aNode->GetLocalName(str);
    else
      aNode->GetNodeName(str);
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
inDOMDataSource::GetFieldNameFromRes(nsIRDFResource* aProperty, nsAutoString* aResult)
{
  char* resval;
  aProperty->GetValue(&resval);
  nsAutoString val;
  val.AssignWithConversion(resval);
  PRInt32 offset = val.Find(IN_NAMESPACE_URI, PR_FALSE, 0, 1);
  if (!offset) {
    PRInt32 len = strlen(IN_NAMESPACE_URI);
    val.Right(*aResult, val.Length() - len);
  }

  return NS_OK;
}

nsresult
inDOMDataSource::CreateLiteral(nsString& str, nsIRDFNode **aResult)
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
inDOMDataSource::GetNodeTypeKey(PRUint16 aType)
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
inDOMDataSource::NotifyObservers(nsIRDFResource *aSubject, nsIRDFResource *aProperty, nsIRDFNode *aObject, PRUint32 aType)
{
	if(mObservers)
	{
		inDOMDSNotification note = { this, aSubject, aProperty, aObject };
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
inDOMDataSource::ChangeEnumFunc(void *aElement, void *aData)
{
  inDOMDSNotification *note = (inDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnChange(note->datasource, note->subject, note->property, nsnull, note->object);
  return PR_TRUE;
}

PRBool
inDOMDataSource::AssertEnumFunc(void *aElement, void *aData)
{
  inDOMDSNotification *note = (inDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnAssert(note->datasource, note->subject, note->property, note->object);
  return PR_TRUE;
}

PRBool
inDOMDataSource::UnassertEnumFunc(void *aElement, void *aData)
{
  inDOMDSNotification* note = (inDOMDSNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->datasource, note->subject, note->property, note->object);
  return PR_TRUE;
}

void 
inDOMDataSource::DumpResourceValue(nsIRDFResource* aRes)
{
  const char* str;
  aRes->GetValueConst(&str);
  printf("Resource Value: %s\n", str);
}

nsresult 
inDOMDataSource::FindAttrRes(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aAttrRes)
{
  inDOMDSFindAttrInfo info = { nsnull, aContent, aNameSpaceID, aAttribute, aAttrRes };
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
inDOMDataSource::FindAttrResEnumFunc(nsHashKey *aKey, void *aData, void* closure)
{
  inDOMDSFindAttrInfo* info = (inDOMDSFindAttrInfo*)closure;
  nsCOMPtr<nsISupports> supports = (nsISupports*) gDOMObjectTable.Get(aKey);
  nsCOMPtr<inIDOMRDFResource> dsres = do_QueryInterface(supports);
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
        if (name1.Equals(name2)) {
          *info->mAttrRes = res;
          NS_ADDREF(*info->mAttrRes);
        }
        
      }
    }
  }
  return PR_TRUE;
}

PRBool
inDOMDataSource::HasChild(nsIDOMNode* aContainer, nsIDOMNode* aChild)
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
inDOMDataSource::HasAttribute(nsIDOMNode* aContainer, nsIDOMNode* aAttr)
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
inDOMDataSource::HasChildren(nsIDOMNode* aNode)
{
  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    
    if (mShowSubDocuments) {
      nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aNode);
      if (domdoc)
        return PR_TRUE;
    }

    nsCOMPtr<nsIDOMNodeList> kids;
    if (mShowAnonymousContent) {
      nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
      bindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
      if (!kids)
        bindingManager->GetContentListFor(content, getter_AddRefs(kids));
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
inDOMDataSource::RemoveResourceForObject(nsISupports* aObject)
{
  nsISupportsKey* objKey = new nsISupportsKey(aObject);
  gDOMObjectTable.Remove(objKey);
  return NS_OK;
}

