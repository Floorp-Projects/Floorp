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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsMsgRDFUtils.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "nsCOMPtr.h"
#include "nsIAllocator.h"

 
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

static PRBool
peqWithParameter(nsIRDFResource *r1, nsIRDFResource *r2, const char *parameter)
{
	char *r1Str, *r2Str;
	nsString r1nsStr, r2nsStr;

	r1->GetValue(&r1Str);
	r2->GetValue(&r2Str);

	r2nsStr = r2Str;
	r1nsStr = r1Str;
	nsAllocator::Free(r2Str);
	nsAllocator::Free(r1Str);

	//Look to see if there are any parameters
	PRInt32 paramStart = r2nsStr.FindChar('?');
	//If not, then it's not this parameter.
	if(paramStart == -1)
	{
		return PR_FALSE;
	}

	nsString r2propStr;
	//Get the string before the '?"
	r2nsStr.Left(r2propStr, paramStart);
	//If the two properties are equal, then search parameters.
	if(r2propStr == r1nsStr)
	{
		nsString params;
		r2nsStr.Right(params, r2nsStr.Length() - 1 - paramStart);
		PRInt32 parameterPos = params.Find(parameter);
		return (parameterPos != -1);
	}
	//Otherwise the properties aren't equal.
	else
	{
		return PR_FALSE;
	}
	return PR_FALSE;
}

PRBool
peqCollationSort(nsIRDFResource *r1, nsIRDFResource *r2)
{
	return peqWithParameter(r1, r2, "collation=true");
}

PRBool
peqSort(nsIRDFResource* r1, nsIRDFResource* r2)
{
	return peqWithParameter(r1, r2, "sort=true");
}

nsresult createNode(nsString& str, nsIRDFNode **node)
{
	*node = nsnull;
	nsresult rv; 
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
	if (NS_FAILED(rv)) return rv;  
	nsCOMPtr<nsIRDFLiteral> value;
	rv = rdf->GetLiteral(str.GetUnicode(), getter_AddRefs(value));
	if(NS_SUCCEEDED(rv)) {
		*node = value;
		NS_IF_ADDREF(*node);
	}
	return rv;
}

nsresult createNode(PRUint32 value, nsIRDFNode **node)
{
	nsresult rv;
	char *valueStr = PR_smprintf("%d", value);
	nsString str(valueStr);
	PR_smprintf_free(valueStr);
	rv = createNode(str, node);
	return rv;
}

nsresult createNode(const char* charstr, nsIRDFNode **node)
{
  nsresult rv;
  // use nsString to convert to unicode
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
	if (NS_FAILED(rv)) return rv;  
	nsCOMPtr<nsIRDFLiteral> value;
  nsString str(charstr);
  PRUnichar *ucharstr = str.ToNewUnicode();
	rv = rdf->GetLiteral(ucharstr, getter_AddRefs(value));
	if(NS_SUCCEEDED(rv)) {
		*node = value;
		NS_IF_ADDREF(*node);
	}
  return rv;
}

nsresult createDateNode(PRTime time, nsIRDFNode **node)
{
	*node = nsnull;
	nsresult rv; 
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 
	if (NS_FAILED(rv)) return rv;  
	nsCOMPtr<nsIRDFDate> date;
	rv = rdf->GetDateLiteral(time, getter_AddRefs(date));
	if(NS_SUCCEEDED(rv)) {
		*node = date;
		NS_IF_ADDREF(*node);
	}
	return rv;
}

nsresult GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* folderResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion)
{
	nsresult rv;
	if(!hasAssertion)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFNode> currentTarget;

	rv = dataSource->GetTarget(folderResource, property,tv, getter_AddRefs(currentTarget));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral> value1(do_QueryInterface(target));
		nsCOMPtr<nsIRDFLiteral> value2(do_QueryInterface(currentTarget));
		if(value1 && value2)
			//If the two values are equal then it has this assertion
			*hasAssertion = (value1 == value2);
	}
	else
		rv = NS_NOINTERFACE;

	return rv;

}

