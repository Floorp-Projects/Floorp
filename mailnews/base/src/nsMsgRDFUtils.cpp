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

nsresult createNode(const PRUnichar *str, nsIRDFNode **node, nsIRDFService *rdfService)
{
  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> value;

  if (str) {
    rv = rdfService->GetLiteral(str, getter_AddRefs(value));
  } 
  else {
    rv = rdfService->GetLiteral(NS_LITERAL_STRING("").get(), getter_AddRefs(value));
  }

  if (NS_SUCCEEDED(rv)) {
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

