#include "nsPrincipalArray.h"

static NS_DEFINE_IID(kIPrincipalArrayIID, NS_IPRINCIPALARRAY_IID);

NS_IMPL_ISUPPORTS(nsPrincipalArray, kIPrincipalArrayIID);
nsPrincipalArray::nsPrincipalArray(void)
{
	nsPrincipalArray(0);
}

nsPrincipalArray::nsPrincipalArray(PRUint32 count)
{
	nsVector * itsArray = new nsVector();
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
	for (i; i-- > 0;) {
		other->GetPrincipalArrayElement(i,& prin);
		PrincipalKey prinKey(prin);
		p2Hashtable->Put(& prinKey, (void *)PR_TRUE);
	}
	this->GetPrincipalArraySize(& i);
	for (i; i-- > 0;) {
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
	for (i; i-- > 0;) {
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
	PRUint32 count = 0;
	nsIPrincipal * prin1, * prin2;
	PRUint32 i = 0, j=0;
	in->SetSize(thisLength, 1);
	PRUint32 inLength = in->GetSize();
	for (i=0; i < thisLength; i++) {
		for (j=0; j < otherLength; j++) {
			this->GetPrincipalArrayElement(i,& prin1);
			other->GetPrincipalArrayElement(j,& prin2);
			PRBool eq;
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
	PRBool doesIntersect;
	PR_ASSERT(inLength == thisLength);
	PR_ASSERT(inLength == inLength);
	for (i=0; i < inLength; i++) {
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