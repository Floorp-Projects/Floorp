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
 */

#include "nsCSSRuleDataSource.h"
#include "dsinfo.h"

////////////////////////////////////////////////////////////////////////
// globals and constants

#define INS_RDF_RULEROOT      "inspector:RuleRoot"
#define INS_RDF_RULE          INS_NAMESPACE_URI "Rule"
#define INS_RDF_SELECTOR      INS_NAMESPACE_URI "Selector"
#define INS_RDF_WEIGHT        INS_NAMESPACE_URI "Weight"
#define INS_RDF_FILEURL       INS_NAMESPACE_URI "FileURL"
#define INS_RDF_LINENUM       INS_NAMESPACE_URI "LineNum"

static PRInt32 gCurrentId = 0;

nsIRDFResource		*kINS_RuleRoot;
nsIRDFResource		*kINS_Rule;
nsIRDFResource		*kINS_Selector;
nsIRDFResource		*kINS_Weight;
nsIRDFResource		*kINS_FileURL;
nsIRDFResource		*kINS_LineNum;

////////////////////////////////////////////////////////////////////////

nsCSSRuleDataSource::nsCSSRuleDataSource()
{
	NS_INIT_REFCNT();
} 


nsCSSRuleDataSource::~nsCSSRuleDataSource()
{
  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }

 /* NS_IF_RELEASE(kINS_RuleRoot);
  NS_IF_RELEASE(kINS_Rule);
  NS_IF_RELEASE(kINS_Selector);
  NS_IF_RELEASE(kINS_Weight);
  NS_IF_RELEASE(kINS_FileURL);
  NS_IF_RELEASE(kINS_LineNum);
*/
}

nsresult
nsCSSRuleDataSource::Init()
{
  nsresult rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
  if (NS_FAILED(rv)) return rv;

  mRDFService->GetResource(INS_RDF_RULEROOT,            &kINS_RuleRoot);
  mRDFService->GetResource(INS_RDF_RULE,                &kINS_Rule);
  mRDFService->GetResource(INS_RDF_SELECTOR,            &kINS_Selector);
  mRDFService->GetResource(INS_RDF_WEIGHT,              &kINS_Weight);
  mRDFService->GetResource(INS_RDF_FILEURL,             &kINS_FileURL);
  mRDFService->GetResource(INS_RDF_LINENUM,             &kINS_LineNum);

   return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS2(nsCSSRuleDataSource,
                   nsICSSRuleDataSource,
                   nsIRDFDataSource);

////////////////////////////////////////////////////////////////////////
// nsICSSRuleDataSource

NS_IMETHODIMP
nsCSSRuleDataSource::SetElement(nsIDOMElement* aElement)
{
  mElement = aElement;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSRuleDataSource::GetElement(nsIDOMElement** aElement)
{
  *aElement = mElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK; 
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsCSSRuleDataSource::GetURI(char* *aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (! aURI) return NS_ERROR_NULL_POINTER;

  *aURI = nsCRT::strdup("rdf:" NS_CSSRULEDATASOURCE_ID);
	if (! *aURI) return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

NS_IMETHODIMP
nsCSSRuleDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
  *_retval = nsnull;
  
  nsresult rv;

  if (aSource == kINS_RuleRoot) {
    // It's the root, so we ignore it
    return NS_OK;
  } else {
    // It's not the root, so we proceed to find the target node
    nsCOMPtr<nsIDOMDSResource> domRes = do_QueryInterface(aSource);

    if (domRes) {
      nsCOMPtr<nsISupports> supports;
	    domRes->GetObject(getter_AddRefs(supports));

      nsCOMPtr<nsICSSStyleRule> rule = do_QueryInterface(supports, &rv);
      if (NS_SUCCEEDED(rv))
        return CreateStyleRuleTarget(rule, aProperty, _retval);
    }
  }

  return rv;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  nsArrayEnumerator* cursor = new nsArrayEnumerator(arcs);
  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;
    
  *_retval = cursor;
  NS_ADDREF(*_retval);

  if (!mElement) return NS_OK;

  // The only node with targets is the root, since this this data source
  // is just a flat list of rules
  if (aSource == kINS_RuleRoot) {
    rv = GetTargetsForElement(mElement, aProperty, arcs);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsCSSRuleDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::Change(nsIRDFResource *, nsIRDFResource *,
                           nsIRDFNode *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCSSRuleDataSource::Move(nsIRDFResource *, nsIRDFResource *,
                         nsIRDFResource *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCSSRuleDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSRuleDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSRuleDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSRuleDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSRuleDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP 
nsCSSRuleDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
nsCSSRuleDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsCSSRuleDataSource

nsIRDFService*
nsCSSRuleDataSource::GetRDFService()
{
  if (!mRDFService) {
    nsresult rv;
    rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
    if (NS_FAILED(rv)) return nsnull;
  }
    
  return mRDFService;
}

nsresult
nsCSSRuleDataSource::GetTargetsForElement(nsIDOMElement* aElement, nsIRDFResource* aProperty, nsISupportsArray* aArcs)
{
  // query to a content node
  nsCOMPtr<nsIContent> content;
  content = do_QueryInterface(aElement);

  // get the document
  nsCOMPtr<nsIDOMDocument> doc1;
  aElement->GetOwnerDocument(getter_AddRefs(doc1));
  nsCOMPtr<nsIDocument> doc;
  doc = do_QueryInterface(doc1);

  // loop through the PresShells
  nsresult rv;
  PRInt32 num = doc->GetNumberOfShells();
  nsCOMPtr<nsIPresShell> shell;
  nsIFrame* frame;
  nsCOMPtr<nsIStyleContext> styleContext;
  for (PRInt32 i = 0; i < num; i++) {
    // get the style context
    shell = doc->GetShellAt(i);
    rv = shell->GetPrimaryFrameFor(content, &frame);
    if (NS_FAILED(rv) || !frame) return rv;
    shell->GetStyleContextFor(frame, getter_AddRefs(styleContext));
    if (NS_FAILED(rv) || !styleContext) return rv;

    // create a resource for all the style rules from the 
    // context and add them to aArcs
    nsCOMPtr<nsIRuleNode> ruleNode;
    styleContext->GetRuleNode(getter_AddRefs(ruleNode));
    
    nsCOMPtr<nsIStyleRule> srule;
    nsCOMPtr<nsICSSStyleRule> rule;
    nsCOMPtr<nsIRuleNode> ruleNodeTmp;
    nsCOMPtr<nsIRDFResource> resource;
    while (ruleNode) {
      ruleNode->GetRule(getter_AddRefs(srule));
      rule = do_QueryInterface(srule);

      rv = GetResourceForObject(rule, getter_AddRefs(resource));
      // since tree of nodes is from top down, insert in reverse order
      rv = aArcs->InsertElementAt(resource, 0); 
      
      ruleNode->GetParent(getter_AddRefs(ruleNodeTmp));
      ruleNode = ruleNodeTmp;
      
      // don't be adding that there root node
      PRBool isRoot;
      ruleNode->IsRoot(&isRoot);
      if (isRoot) break;
    }
  }

  return NS_OK;
}

nsresult
nsCSSRuleDataSource::GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval)
{
  nsresult rv;
  nsISupportsKey* objKey = new nsISupportsKey(aObject);
  nsCOMPtr<nsISupports> supports = (nsISupports*) mObjectTable.Get(objKey);
  
  if (supports) {
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports, &rv);
    *_retval = res;
  } else {
    // it's not in the hash table, so add it.
    char *uri = PR_smprintf(NS_DOMDSRESOURCE_ID "://CSSRULE%8.8X", gCurrentId++);
    
    // have the resource created by the resource factory
    rv = GetRDFService()->GetResource(uri, _retval);
    if (NS_FAILED(rv)) return rv;

    // add it to the hash table
    mObjectTable.Put(objKey, *_retval);
    
    // now fill in the resource stuff
    nsCOMPtr<nsIDOMDSResource> nodeContainer = do_QueryInterface(*_retval, &rv);
    if (NS_FAILED(rv)) return rv;
    nodeContainer->SetObject(aObject);
  }

  return NS_OK;
}

nsresult 
nsCSSRuleDataSource::CreateStyleRuleTarget(nsICSSStyleRule* aRule, nsIRDFResource* aProperty, nsIRDFNode **aResult)
{
  nsresult rv;
  nsAutoString str;
  if (aProperty == kINS_Selector) {
    aRule->GetSourceSelectorText(str);
  } else if (aProperty == kINS_Weight) {
    PRUint32 weight = aRule->GetWeight();
    char* ln = PR_smprintf("%d", weight);
    str.AssignWithConversion(ln);
  } else if (aProperty == kINS_FileURL) {
    nsCOMPtr<nsIStyleSheet> sheet;
    nsCOMPtr<nsIURI> uri;
    char* spec;
    rv = aRule->GetStyleSheet(*getter_AddRefs(sheet));
    if (NS_FAILED(rv) || !sheet) return rv;
    rv = sheet->GetURL(*getter_AddRefs(uri));
    if (NS_FAILED(rv) || !uri) return rv;
    uri->GetSpec(&spec);
    str.AssignWithConversion(spec);
  } else if (aProperty == kINS_LineNum) {
    PRUint32 linenum = aRule->GetLineNumber();
    char* ln = PR_smprintf("%d", linenum);
    str.AssignWithConversion(ln);
  }
  return CreateLiteral(str, aResult);
}

nsresult
nsCSSRuleDataSource::CreateLiteral(nsString& str, nsIRDFNode **aResult)
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
