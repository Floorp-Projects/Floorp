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

#include "nsCSSDecDataSource.h"
#include "dsinfo.h"

////////////////////////////////////////////////////////////////////////
// globals and constants

#define INS_RDF_DECROOT               "inspector:DecRoot"
#define INS_RDF_PROPERTY              INS_NAMESPACE_URI "Property"
#define INS_RDF_PROPERTYNAME          INS_NAMESPACE_URI "PropertyName"
#define INS_RDF_PROPERTYVALUE         INS_NAMESPACE_URI "PropertyValue"
#define INS_RDF_PROPERTYPRIORITY      INS_NAMESPACE_URI "PropertyPriority"

static PRInt32 gCurrentId = 0;

nsIRDFResource		*kINS_DecRoot;
nsIRDFResource		*kINS_Property;
nsIRDFResource		*kINS_PropertyName;
nsIRDFResource		*kINS_PropertyValue;
nsIRDFResource		*kINS_PropertyPriority;

////////////////////////////////////////////////////////////////////////

nsCSSDecDataSource::nsCSSDecDataSource()
{
	NS_INIT_REFCNT();
} 


nsCSSDecDataSource::~nsCSSDecDataSource()
{
  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }

 /* NS_IF_RELEASE(kINS_DecRoot);
  NS_IF_RELEASE(kINS_Property);
  NS_IF_RELEASE(kINS_PropertyName);
  NS_IF_RELEASE(kINS_PropertyValue);
  NS_IF_RELEASE(kINS_PropertyPriority);
*/
}

nsresult
nsCSSDecDataSource::Init()
{
  nsresult rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
  if (NS_FAILED(rv)) return rv;

  mRDFService->GetResource(INS_RDF_DECROOT,            &kINS_DecRoot);
  mRDFService->GetResource(INS_RDF_PROPERTY,           &kINS_Property);
  mRDFService->GetResource(INS_RDF_PROPERTYNAME,       &kINS_PropertyName);
  mRDFService->GetResource(INS_RDF_PROPERTYVALUE,      &kINS_PropertyValue);
  mRDFService->GetResource(INS_RDF_PROPERTYPRIORITY,   &kINS_PropertyPriority);

   return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS2(nsCSSDecDataSource,
                   nsICSSDecDataSource,
                   nsIRDFDataSource);

////////////////////////////////////////////////////////////////////////
// nsICSSRuleDataSource

NS_IMETHODIMP
nsCSSDecDataSource::GetRefResource(nsIRDFResource** aRefResource)
{
  *aRefResource = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsCSSDecDataSource::SetRefResource(nsIRDFResource* aRefResource)
{
  nsresult rv;
  nsCOMPtr<nsIDOMDSResource> domRes = do_QueryInterface(aRefResource);
  
  nsCOMPtr<nsISupports> supports;
  rv = domRes->GetObject(getter_AddRefs(supports));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMCSSStyleRule> rule = do_QueryInterface(supports, &rv);
  if (NS_FAILED(rv)) return rv;

  mStyleRule = rule;
  return NS_OK;
}

NS_IMETHODIMP nsCSSDecDataSource::SetCSSProperty(const PRUnichar* aName, const PRUnichar* aValue, const PRUnichar* aPriority)
{
  nsresult rv;
  // get the declaration
  nsCOMPtr<nsIDOMCSSStyleDeclaration> dec;
  rv = mStyleRule->GetStyle(getter_AddRefs(dec));
  
  nsAutoString name; name.Assign(aName);
  nsAutoString value; value.Assign(aValue);
  nsAutoString priority; priority.Assign(aPriority);
  return dec->SetProperty(name, value, priority);
}


NS_IMETHODIMP nsCSSDecDataSource::RemoveCSSProperty(const PRUnichar* aName)
{
  nsresult rv;
  // get the declaration
  nsCOMPtr<nsIDOMCSSStyleDeclaration> dec;
  rv = mStyleRule->GetStyle(getter_AddRefs(dec));
  
  nsAutoString ret;
  nsAutoString name; name.Assign(aName);
  return dec->RemoveProperty(name, ret);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsCSSDecDataSource::GetURI(char* *aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (! aURI) return NS_ERROR_NULL_POINTER;

  *aURI = nsCRT::strdup("rdf:" NS_CSSDECDATASOURCE_ID);
	if (! *aURI) return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

NS_IMETHODIMP
nsCSSDecDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
  *_retval = nsnull;
  
  nsresult rv;

  if (aSource == kINS_DecRoot) {
    // It's the root, so we ignore it
    return NS_OK;
  } else {
    // It's not the root, so we proceed to find the target node
    nsCOMPtr<nsIDOMDSResource> domRes = do_QueryInterface(aSource);

    if (domRes) {
      nsCOMPtr<nsISupports> supports;
	    domRes->GetObject(getter_AddRefs(supports));

      nsCOMPtr<nsCSSDecIntHolder> rule = do_QueryInterface(supports, &rv);
      if (NS_SUCCEEDED(rv))
        return CreatePropertyTarget(rule->mInt, aProperty, _retval);
    }
  }

  return rv;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
  nsresult rv;

  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  nsArrayEnumerator* cursor = new nsArrayEnumerator(arcs);
  if (!cursor) return NS_ERROR_OUT_OF_MEMORY;

  *_retval = cursor;
  NS_ADDREF(*_retval);

  if (!mStyleRule) return NS_OK;

  // The only node with targets is the root, since this this data source
  // is just a flat list of properties
  if (aSource == kINS_DecRoot && aProperty == kINS_Property) {
    rv = GetTargetsForStyleRule(mStyleRule, aProperty, arcs);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsCSSDecDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::Change(nsIRDFResource *, nsIRDFResource *,
                           nsIRDFNode *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCSSDecDataSource::Move(nsIRDFResource *, nsIRDFResource *,
                         nsIRDFResource *, nsIRDFNode*)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCSSDecDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsCSSDecDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSDecDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSDecDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
  return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
  return NS_OK;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsCSSDecDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP 
nsCSSDecDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
nsCSSDecDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsCSSDecDataSource

nsIRDFService*
nsCSSDecDataSource::GetRDFService()
{
  if (!mRDFService) {
    nsresult rv;
    rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), (nsISupports**) &mRDFService);
    if (NS_FAILED(rv)) return nsnull;
  }
    
  return mRDFService;
}

nsresult
nsCSSDecDataSource::GetTargetsForStyleRule(nsIDOMCSSStyleRule* aRule, nsIRDFResource* aProperty, nsISupportsArray* aArcs)
{
  nsresult rv;

  // get the declaration
  nsCOMPtr<nsIDOMCSSStyleDeclaration> dec;
  rv = aRule->GetStyle(getter_AddRefs(dec));
  // loop through the properties
  PRUint32 count;
  dec->GetLength(&count);

  nsCSSDecIntHolder* holder;
  for (PRUint32 i = 0; i < count; i++) {
    holder = new nsCSSDecIntHolder(i);
    nsCOMPtr<nsIRDFResource> resource;
    rv = GetResourceForObject(holder, getter_AddRefs(resource));
    rv = aArcs->AppendElement(resource);
  }
  return NS_OK;
}

nsresult
nsCSSDecDataSource::GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval)
{
  nsresult rv;
  nsISupportsKey* objKey = new nsISupportsKey(aObject);
  nsCOMPtr<nsISupports> supports = (nsISupports*) mObjectTable.Get(objKey);
  
  if (supports) {
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports, &rv);
    *_retval = res;
  } else {
    // it's not in the hash table, so add it.
    char *uri = PR_smprintf(NS_DOMDSRESOURCE_ID "://CSSPROP%8.8X", gCurrentId++);
    
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
nsCSSDecDataSource::CreatePropertyTarget(PRUint32 aIndex, nsIRDFResource* aProperty, nsIRDFNode **aResult)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> dec;
  nsresult rv = mStyleRule->GetStyle(getter_AddRefs(dec));
  nsAutoString name;
  dec->Item(aIndex, name);
  
  nsAutoString str;
  if (aProperty == kINS_PropertyName) {
    str = name;
  } else if (aProperty == kINS_PropertyValue) {
    dec->GetPropertyValue(name, str);
  } else if (aProperty == kINS_PropertyPriority) {
    dec->GetPropertyPriority(name, str);
  }
  return CreateLiteral(str, aResult);
}

nsresult
nsCSSDecDataSource::CreateLiteral(nsString& str, nsIRDFNode **aResult)
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

