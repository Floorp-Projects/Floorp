/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsMsgRDFUtils.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"

 

static PRBool
peqWithParameter(nsIRDFResource *r1, nsIRDFResource *r2, const char *parameter)
{
    // STRING USE WARNING: is any conversion really required here at all?

	char *r1Str, *r2Str;
	nsAutoString r1nsStr, r2nsStr;

	r1->GetValue(&r1Str);
	r2->GetValue(&r2Str);

	r2nsStr.AssignWithConversion(r2Str);
	r1nsStr.AssignWithConversion(r1Str);
	nsMemory::Free(r2Str);
	nsMemory::Free(r1Str);

	//Look to see if there are any parameters
	PRInt32 paramStart = r2nsStr.FindChar('?');
	//If not, then it's not this parameter.
	if(paramStart == -1)
	{
		return PR_FALSE;
	}

	nsAutoString r2propStr;
	//Get the string before the '?"
	r2nsStr.Left(r2propStr, paramStart);
	//If the two properties are equal, then search parameters.
	if(r2propStr == r1nsStr)
	{
		nsAutoString params;
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

nsresult createNode(const PRUnichar *str, nsIRDFNode **node, nsIRDFService *rdfService)
{
  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> value;

  if (str) {
    rv = rdfService->GetLiteral(str, getter_AddRefs(value));
  } else {
	PRUnichar blankStr[] = { 0 };
	rv = rdfService->GetLiteral(blankStr, getter_AddRefs(value));
  }
  if (NS_SUCCEEDED(rv)) {
    *node = value;
    NS_IF_ADDREF(*node);
  }
  return rv;
}

nsresult createNode(nsString& str, nsIRDFNode **node, nsIRDFService *rdfService)
{
	*node = nsnull;
	nsresult rv; 
	if (!rdfService) return NS_ERROR_NULL_POINTER;  
	nsCOMPtr<nsIRDFLiteral> value;
	rv = rdfService->GetLiteral(str.get(), getter_AddRefs(value));
	if(NS_SUCCEEDED(rv)) {
		*node = value;
		NS_IF_ADDREF(*node);
	}
	return rv;
}

nsresult createNode(const char* charstr, nsIRDFNode **node, nsIRDFService *rdfService)
{
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  // use nsString to convert to unicode
	if (!rdfService) return NS_ERROR_NULL_POINTER;  
	nsCOMPtr<nsIRDFLiteral> value;
  nsAutoString str; str.AssignWithConversion(charstr);

  rv = rdfService->GetLiteral(NS_ConvertUTF8toUCS2(charstr).get(), getter_AddRefs(value));
  
	if(NS_SUCCEEDED(rv)) {
		*node = value;
		NS_IF_ADDREF(*node);
	}
  return rv;
}

nsresult createDateNode(PRTime time, nsIRDFNode **node, nsIRDFService *rdfService)
{
	*node = nsnull;
	nsresult rv; 
	if (!rdfService) return NS_ERROR_NULL_POINTER;  
	nsCOMPtr<nsIRDFDate> date;
	rv = rdfService->GetDateLiteral(time, getter_AddRefs(date));
	if(NS_SUCCEEDED(rv)) {
		*node = date;
		NS_IF_ADDREF(*node);
	}
	return rv;
}

nsresult createIntNode(PRInt32 value, nsIRDFNode **node, nsIRDFService *rdfService)
{
	*node = nsnull;
	nsresult rv; 
	if (!rdfService) return NS_ERROR_NULL_POINTER;  
	nsCOMPtr<nsIRDFInt> num;
	rv = rdfService->GetIntLiteral(value, getter_AddRefs(num));
	if(NS_SUCCEEDED(rv)) {
		*node = num;
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

