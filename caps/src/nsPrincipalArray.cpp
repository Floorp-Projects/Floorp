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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*used to pass principals through xpcom in arrays*/
#include "nsPrincipalArray.h"

static NS_DEFINE_IID(kIPrincipalArrayIID, NS_IPRINCIPALARRAY_IID);

NS_IMPL_ISUPPORTS(nsPrincipalArray, kIPrincipalArrayIID);
nsPrincipalArray::nsPrincipalArray(void)
{
	nsPrincipalArray(0);
}

nsPrincipalArray::nsPrincipalArray(PRUint32 count)
{
	itsArray = new nsVector();
	if(itsArray)
		itsArray->SetSize(count, 1);
}
nsPrincipalArray::~nsPrincipalArray(void)
{
	this->FreePrincipalArray();
}

NS_IMETHODIMP
nsPrincipalArray::SetPrincipalArrayElement(PRUint32 index, nsIPrincipal * principal)
{
	if (itsArray != NULL) this->itsArray->Set(index, principal);
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalArray::GetPrincipalArraySize(PRUint32 * size)
{
	* size = (itsArray == NULL) ? 0 : itsArray->GetSize();
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalArray::FreePrincipalArray()
{
	if (itsArray) {
		itsArray->RemoveAll();
		delete itsArray;
	}
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalArray::AddPrincipalArrayElement(nsIPrincipal * principal)
{
	if(!itsArray) itsArray = new nsVector();
	itsArray->Add(principal);
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalArray::GetPrincipalArrayElement(PRUint32 index, nsIPrincipal * * result)
{
	* result = (itsArray == NULL) ? NULL : (nsIPrincipal *) itsArray->Get(index);
	return NS_OK;
}

NS_IMETHODIMP
nsPrincipalArray::ComparePrincipalArray(nsIPrincipalArray * other, PRInt16 * comparisonType)
{
	nsHashtable * p2Hashtable = new nsHashtable();
	PRBool value;
	nsIPrincipal * prin;
	PRUint32 i;
	other->GetPrincipalArraySize(& i);
	while (i-- > 0) {
		other->GetPrincipalArrayElement(i,& prin);
		PrincipalKey prinKey(prin);
		p2Hashtable->Put(& prinKey, (void *)PR_TRUE);
	}
	this->GetPrincipalArraySize(& i);
	while (i-- > 0) {
		this->GetPrincipalArrayElement(i,& prin);
		PrincipalKey prinKey(prin);
		value = (PRBool)p2Hashtable->Get(&prinKey);
		if (!value)
		{
			* comparisonType = nsPrincipalArray::SetComparisonType_NoSubset;
			return NS_OK;
		}
		if (value == PR_TRUE) p2Hashtable->Put(&prinKey, (void *)PR_FALSE);
	}
	other->GetPrincipalArraySize(& i);
	while(i-- > 0) {
		other->GetPrincipalArrayElement(i,& prin);
		PrincipalKey prinKey(prin);
		value = (PRBool)p2Hashtable->Get(&prinKey);
		if (value == PR_TRUE)
		{
			* comparisonType = nsPrincipalArray::SetComparisonType_ProperSubset;
			return NS_OK;
		}
	}
	* comparisonType = nsPrincipalArray::SetComparisonType_Equal;
	return NS_OK;
}

NS_METHOD
nsPrincipalArray::IntersectPrincipalArray(nsIPrincipalArray * other, nsIPrincipalArray * * result)
{
	PRUint32 thisLength = 0, otherLength = 0;
	this->GetPrincipalArraySize(& thisLength);
	other->GetPrincipalArraySize(& otherLength);
	nsVector * in = new nsVector();
	nsIPrincipal * prin1 = NULL, * prin2 = NULL;
	PRUint32 i = 0, j = 0, count = 0;
	in->SetSize(thisLength, 1);
	PRUint32 inLength = in->GetSize();
	PRBool doesIntersect = PR_FALSE, eq = PR_FALSE;
	for (i=0; i < thisLength; i++) {
		for (j=0; j < otherLength; j++) {
			this->GetPrincipalArrayElement(i,& prin1);
			other->GetPrincipalArrayElement(j,& prin2);

			prin1->Equals(prin2, & eq);
			if (eq) {
				in->Set(i, (void *)PR_TRUE);
				count++;
				break;
			} else {
				in->Set(i, (void *)PR_FALSE);
			}
		}
	}
	* result = new nsPrincipalArray(count);
	PR_ASSERT(inLength == thisLength);
	PR_ASSERT(inLength == inLength);
	for (i = 0; i < inLength; i++) {
		doesIntersect = (PRBool)in->Get(i);
		if (doesIntersect) {
			PR_ASSERT(j < count);
			this->GetPrincipalArrayElement(i,& prin1);
			(* result)->SetPrincipalArrayElement(j, prin1);
			j++;
		}
	}
	return NS_OK;
}
