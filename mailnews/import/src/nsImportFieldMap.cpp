/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nscore.h"
#include "nsIStringBundle.h"
#include "nsImportFieldMap.h"
#include "nsImportStringBundle.h"
#include "nsReadableUtils.h"

#include "ImportDebug.h"



////////////////////////////////////////////////////////////////////////



NS_METHOD nsImportFieldMap::Create( nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsImportFieldMap *it = new nsImportFieldMap();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF( it);
  nsresult rv = it->QueryInterface( aIID, aResult);
  NS_RELEASE( it);
  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsImportFieldMap, nsIImportFieldMap)

nsImportFieldMap::nsImportFieldMap() 
{ 
	NS_INIT_ISUPPORTS(); 
	m_numFields = 0;
	m_pFields = nsnull;
	m_pActive = nsnull;
	m_allocated = 0;
	// need to init the description array
	m_mozFieldCount = 0;
	nsIStringBundle *pBundle = nsImportStringBundle::GetStringBundleProxy();

	nsString *pStr;
	for (PRInt32 i = IMPORT_FIELD_DESC_START; i <= IMPORT_FIELD_DESC_END; i++, m_mozFieldCount++) {
		pStr = new nsString();
		if (pBundle) {
			nsImportStringBundle::GetStringByID( i, *pStr, pBundle);	
		}
		else
			pStr->AppendInt( i);
		m_descriptions.AppendElement( (void *)pStr);
	}
	
	NS_IF_RELEASE( pBundle);
}

nsImportFieldMap::~nsImportFieldMap() 
{ 
	if (m_pFields)
		delete [] m_pFields;
	if (m_pActive)
		delete [] m_pActive;

	nsString *	pStr;
	for (PRInt32 i = 0; i < m_mozFieldCount; i++) {
		pStr = (nsString *) m_descriptions.ElementAt( i);
		delete pStr;
	}
	m_descriptions.Clear();
}


NS_IMETHODIMP nsImportFieldMap::GetNumMozFields(PRInt32 *aNumFields)
{
    NS_PRECONDITION(aNumFields != nsnull, "null ptr");
	if (!aNumFields)
		return NS_ERROR_NULL_POINTER;

	*aNumFields = m_mozFieldCount;
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::GetMapSize(PRInt32 *aNumFields)
{
    NS_PRECONDITION(aNumFields != nsnull, "null ptr");
	if (!aNumFields)
		return NS_ERROR_NULL_POINTER;

	*aNumFields = m_numFields;
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::GetFieldDescription(PRInt32 index, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!_retval)
		return NS_ERROR_NULL_POINTER;
	
	*_retval = nsnull;
	if ((index < 0) || (index >= m_descriptions.Count()))
		return( NS_ERROR_FAILURE);
	
	*_retval = ToNewUnicode(*((nsString *)m_descriptions.ElementAt(index)));
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::SetFieldMapSize(PRInt32 size)
{
	nsresult rv = Allocate( size);
	if (NS_FAILED( rv))
		return( rv);
	
	m_numFields = size;

	return( NS_OK);
}


NS_IMETHODIMP nsImportFieldMap::DefaultFieldMap(PRInt32 size)
{
	nsresult rv = SetFieldMapSize( size);
	if (NS_FAILED( rv))
		return( rv);
	for (PRInt32 i = 0; i < size; i++) {
		m_pFields[i] = i;
		m_pActive[i] = PR_TRUE;
	}
	
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::GetFieldMap(PRInt32 index, PRInt32 *_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
	if (!_retval)
		return NS_ERROR_NULL_POINTER;
	
	
	if ((index < 0) || (index >= m_numFields))
		return( NS_ERROR_FAILURE);

	*_retval = m_pFields[index];
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::SetFieldMap(PRInt32 index, PRInt32 fieldNum)
{
	if (index == -1) {
		nsresult rv = Allocate( m_numFields + 1);
		if (NS_FAILED( rv))
			return( rv);
		index = m_numFields;
		m_numFields++;
	}
	else {
		if ((index < 0) || (index >= m_numFields))
			return( NS_ERROR_FAILURE);
	}
	
	if ((fieldNum != -1) && ((fieldNum < 0) || (fieldNum >= m_mozFieldCount)))
		return( NS_ERROR_FAILURE);
	
	m_pFields[index] = fieldNum;
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::SetFieldMapByDescription(PRInt32 index, const PRUnichar *fieldDesc)
{
    NS_PRECONDITION(fieldDesc != nsnull, "null ptr");
	if (!fieldDesc)
		return NS_ERROR_NULL_POINTER;

	PRInt32 i = FindFieldNum( fieldDesc);
	if (i == -1)
		return( NS_ERROR_FAILURE);

	return( SetFieldMap( index, i));
}


NS_IMETHODIMP nsImportFieldMap::GetFieldActive(PRInt32 index, PRBool *active)
{
    NS_PRECONDITION(active != nsnull, "null ptr");
	if (!active)
		return NS_ERROR_NULL_POINTER;
	if ((index < 0) || (index >= m_numFields))
		return( NS_ERROR_FAILURE);
		
	*active = m_pActive[index];
	return( NS_OK);
}

NS_IMETHODIMP nsImportFieldMap::SetFieldActive(PRInt32 index, PRBool active)
{
	if ((index < 0) || (index >= m_numFields))
		return( NS_ERROR_FAILURE);

	m_pActive[index] = active;
	return( NS_OK);
}


NS_IMETHODIMP nsImportFieldMap::SetFieldValue(nsIAddrDatabase *database, nsIMdbRow *row, PRInt32 fieldNum, const PRUnichar *value)
{
    NS_PRECONDITION(database != nsnull, "null ptr");
    NS_PRECONDITION(row != nsnull, "null ptr");
    NS_PRECONDITION(value != nsnull, "null ptr");
	if (!database || !row || !value)
		return NS_ERROR_NULL_POINTER;
	
	// Allow the special value for a null field
	if (fieldNum == -1)
		return( NS_OK);

	if ((fieldNum < 0) || (fieldNum >= m_mozFieldCount))
		return( NS_ERROR_FAILURE);
	
	// UGGG!!!!! lot's of typing here!
	nsresult rv;
	
	nsString str(value);
	char *pVal = ToNewUTF8String(str);

	switch( fieldNum) {
	case 0:
		rv = database->AddFirstName( row, pVal);
		break;
	case 1:
		rv = database->AddLastName( row, pVal);
		break;
	case 2:
		rv = database->AddDisplayName( row, pVal);
		break;
	case 3:
		rv = database->AddNickName( row, pVal);
		break;
	case 4:
		rv = database->AddPrimaryEmail( row, pVal);
		break;
	case 5:
		rv = database->Add2ndEmail( row, pVal);
		break;
	case 6:
		rv = database->AddWorkPhone( row, pVal);
		break;
	case 7:
		rv = database->AddHomePhone( row, pVal);
		break;
	case 8:
		rv = database->AddFaxNumber( row, pVal);
		break;
	case 9:
		rv = database->AddPagerNumber( row, pVal);
		break;
	case 10:
		rv = database->AddCellularNumber( row, pVal);
		break;
	case 11:
		rv = database->AddHomeAddress( row, pVal);
		break;
	case 12:
		rv = database->AddHomeAddress2( row, pVal);
		break;
	case 13:
		rv = database->AddHomeCity( row, pVal);
		break;
	case 14:
		rv = database->AddHomeState( row, pVal);
		break;
	case 15:
		rv = database->AddHomeZipCode( row, pVal);
		break;
	case 16:
		rv = database->AddHomeCountry( row, pVal);
		break;
	case 17:
		rv = database->AddWorkAddress( row, pVal);
		break;
	case 18:
		rv = database->AddWorkAddress2( row, pVal);
		break;
	case 19:
		rv = database->AddWorkCity( row, pVal);
		break;
	case 20:
		rv = database->AddWorkState( row, pVal);
		break;
	case 21:
		rv = database->AddWorkZipCode( row, pVal);
		break;
	case 22:
		rv = database->AddWorkCountry( row, pVal);
		break;
	case 23:
		rv = database->AddJobTitle(row, pVal);
		break;
	case 24:
		rv = database->AddDepartment(row, pVal);
		break;
	case 25:
		rv = database->AddCompany(row, pVal);
		break;
	case 26:
		rv = database->AddWebPage1(row, pVal);
		break;
	case 27:
		rv = database->AddWebPage2(row, pVal);
		break;
	case 28:
		rv = database->AddBirthYear(row, pVal);
		break;
	case 29:
		rv = database->AddBirthMonth(row, pVal);
		break;
	case 30:
		rv = database->AddBirthDay(row, pVal);
		break;
	case 31:
		rv = database->AddCustom1(row, pVal);
		break;
	case 32:
		rv = database->AddCustom2(row, pVal);
		break;
	case 33:
		rv = database->AddCustom3(row, pVal);
		break;
	case 34:
		rv = database->AddCustom4(row, pVal);
		break;
	case 35:
		rv = database->AddNotes(row, pVal);
		break;
	default:
		/* Get the field description, and add it as an anonymous attr? */
		/* OR WHAT???? */
		{
			rv = NS_ERROR_FAILURE;
		}
	}
	
	nsCRT::free( pVal);

	return( rv);
}


NS_IMETHODIMP nsImportFieldMap::SetFieldValueByDescription(nsIAddrDatabase *database, nsIMdbRow *row, const PRUnichar *fieldDesc, const PRUnichar *value)
{
    NS_PRECONDITION(fieldDesc != nsnull, "null ptr");
	if (!fieldDesc)
		return NS_ERROR_NULL_POINTER;
	PRInt32 i = FindFieldNum( fieldDesc);
	if (i == -1)
		return( NS_ERROR_FAILURE);
	return( SetFieldValue( database, row, i, value));
}


NS_IMETHODIMP nsImportFieldMap::GetFieldValue(nsIAbCard *card, PRInt32 fieldNum, PRUnichar **_retval)
{
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    NS_PRECONDITION(card != nsnull, "null ptr");
	if (!_retval || !card)
		return NS_ERROR_NULL_POINTER;
	
	if (fieldNum == -1) {
		PRUnichar	c = 0;
		*_retval = nsCRT::strdup( &c);
		return( NS_OK);
	}

	if ((fieldNum < 0) || (fieldNum >= m_mozFieldCount))
		return( NS_ERROR_FAILURE);

	// ARRGGG!!! Lots of typing again
	// get the field from the card
	nsresult	rv;
	PRUnichar *	pVal = nsnull;

	switch (fieldNum) {
	case 0:
		rv = card->GetFirstName( &pVal);
		break;
	case 1:
		rv = card->GetLastName( &pVal);
		break;
	case 2:
		rv = card->GetDisplayName( &pVal);
		break;
	case 3:
		rv = card->GetNickName( &pVal);
		break;
	case 4:
		rv = card->GetPrimaryEmail( &pVal);
		break;
	case 5:
		rv = card->GetSecondEmail( &pVal);
		break;
	case 6:
		rv = card->GetWorkPhone( &pVal);
		break;
	case 7:
		rv = card->GetHomePhone( &pVal);
		break;
	case 8:
		rv = card->GetFaxNumber( &pVal);
		break;
	case 9:
		rv = card->GetPagerNumber( &pVal);
		break;
	case 10:
		rv = card->GetCellularNumber( &pVal);
		break;
	case 11:
		rv = card->GetHomeAddress( &pVal);
		break;
	case 12:
		rv = card->GetHomeAddress2( &pVal);
		break;
	case 13:
		rv = card->GetHomeCity( &pVal);
		break;
	case 14:
		rv = card->GetHomeState( &pVal);
		break;
	case 15:
		rv = card->GetHomeZipCode( &pVal);
		break;
	case 16:
		rv = card->GetHomeCountry( &pVal);
		break;
	case 17:
		rv = card->GetWorkAddress( &pVal);
		break;
	case 18:
		rv = card->GetWorkAddress2( &pVal);
		break;
	case 19:
		rv = card->GetWorkCity( &pVal);
		break;
	case 20:
		rv = card->GetWorkState( &pVal);
		break;
	case 21:
		rv = card->GetWorkZipCode( &pVal);
		break;
	case 22:
		rv = card->GetWorkCountry( &pVal);
		break;
	case 23:
		rv = card->GetJobTitle( &pVal);
		break;
	case 24:
		rv = card->GetDepartment( &pVal);
		break;
	case 25:
		rv = card->GetCompany( &pVal);
		break;
	case 26:
		rv = card->GetWebPage1( &pVal);
		break;
	case 27:
		rv = card->GetWebPage2( &pVal);
		break;
	case 28:
		rv = card->GetBirthYear( &pVal);
		break;
	case 29:
		rv = card->GetBirthMonth( &pVal);
		break;
	case 30:
		rv = card->GetBirthDay( &pVal);
		break;
	case 31:
		rv = card->GetCustom1( &pVal);
		break;
	case 32:
		rv = card->GetCustom2( &pVal);
		break;
	case 33:
		rv = card->GetCustom3( &pVal);
		break;
	case 34:
		rv = card->GetCustom4( &pVal);
		break;
	case 35:
		rv = card->GetNotes( &pVal);
		break;
	default:
		/* Get the field description, and add it as an anonymous attr? */
		/* OR WHAT???? */
		{
			rv = NS_ERROR_FAILURE;
		}
	}
	
	*_retval = pVal;

	return( rv);
}

NS_IMETHODIMP nsImportFieldMap::GetFieldValueByDescription(nsIAbCard *card, const PRUnichar *fieldDesc, PRUnichar **_retval)
{
    NS_PRECONDITION(fieldDesc != nsnull, "null ptr");
	if (!fieldDesc)
		return NS_ERROR_NULL_POINTER;
	PRInt32 i = FindFieldNum( fieldDesc);
	if (i == -1)
		return( NS_ERROR_FAILURE);
	return( GetFieldValue( card, i, _retval));
}


nsresult nsImportFieldMap::Allocate( PRInt32 newSize)
{
	if (newSize <= m_allocated)
		return( NS_OK);

	PRInt32 sz = m_allocated;
	while (sz < newSize)
		sz += 30;

	PRInt32	*pData = new PRInt32[ sz];
	if (!pData)
		return( NS_ERROR_FAILURE);
	PRBool *pActive = new PRBool[sz];
	if (!pActive)
		return( NS_ERROR_FAILURE);


	PRInt32	i;
	for (i = 0; i < sz; i++) {
		pData[i] = -1;
		pActive[i] = PR_TRUE;
	}
	if (m_numFields) {
		for (i = 0; i < m_numFields; i++) {
			pData[i] = m_pFields[i];
			pActive[i] = m_pActive[i];
		}
		delete [] m_pFields;
		delete [] m_pActive;
	}
	m_allocated = sz;
	m_pFields = pData;
	m_pActive = pActive;
	return( NS_OK);
}

PRInt32 nsImportFieldMap::FindFieldNum( const PRUnichar *pDesc)
{
	nsString *	pStr;
	for (PRInt32 i = 0; i < m_mozFieldCount; i++) {
		pStr = (nsString *)m_descriptions.ElementAt( i);
		if (!pStr->Equals(pDesc))
			return( i);
	}

	return( -1);
}


