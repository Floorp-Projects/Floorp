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

#include "nsAbMDBDirProperty.h"	 
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsAddrDatabase.h"
#include "nsIAbCard.h"
#include "nsIAbListener.h"
#include "nsIAddrBookSession.h"
#include "nsIAddressBook.h"

#include "mdb.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
// static NS_DEFINE_CID(kAbCardCID, NS_ABCARD_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);

nsAbMDBDirProperty::nsAbMDBDirProperty(void)
{
}

nsAbMDBDirProperty::~nsAbMDBDirProperty(void)
{
}


NS_IMPL_ISUPPORTS_INHERITED1(nsAbMDBDirProperty, nsAbDirProperty, nsIAbMDBDirectory)

////////////////////////////////////////////////////////////////////////////////



// nsIAbMDBDirectory attributes

NS_IMETHODIMP nsAbMDBDirProperty::GetDbRowID(PRUint32 *aDbRowID)
{
	*aDbRowID = m_dbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirProperty::SetDbRowID(PRUint32 aDbRowID)
{
	m_dbRowID = aDbRowID;
	return NS_OK;
}


// nsIAbMDBDirectory methods

/* add mailing list to the parent directory */
NS_IMETHODIMP nsAbMDBDirProperty::AddMailListToDirectory(nsIAbDirectory *mailList)
{
	if (!m_AddressList)
		NS_NewISupportsArray(getter_AddRefs(m_AddressList));
	PRUint32 i, count;
	m_AddressList->Count(&count);
	for (i = 0; i < count; i++)
	{
		nsresult err;
		nsCOMPtr<nsISupports> pSupport =
		getter_AddRefs(m_AddressList->ElementAt(i));
		nsCOMPtr<nsIAbDirectory> pList(do_QueryInterface(pSupport, &err));
		if (mailList == pList.get())
			return NS_OK;
	}
	m_AddressList->AppendElement(mailList);
	return NS_OK;
}

/* add addresses to the mailing list */
NS_IMETHODIMP nsAbMDBDirProperty::AddAddressToList(nsIAbCard *card)
{

	if (!m_AddressList)
		NS_NewISupportsArray(getter_AddRefs(m_AddressList));
	PRUint32 i, count;
	m_AddressList->Count(&count);
	for (i = 0; i < count; i++)
	{
		nsresult err;
		nsCOMPtr<nsISupports> pSupport =
		getter_AddRefs(m_AddressList->ElementAt(i));
		nsCOMPtr<nsIAbCard> pCard(do_QueryInterface(pSupport, &err));
		if (card == pCard.get())
			return NS_OK;
	}
	m_AddressList->AppendElement(card);
	return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirProperty::CopyDBMailList(nsIAbMDBDirectory* srcListDB)
{
	nsresult err = NS_OK;
	nsCOMPtr<nsIAbDirectory> srcList(do_QueryInterface(srcListDB));
	if (NS_FAILED(err)) 
		return NS_ERROR_NULL_POINTER;

	CopyMailList (srcList);

	PRUint32 rowID;
	srcListDB->GetDbRowID(&rowID);
	SetDbRowID(rowID);

	return NS_OK;
}


// nsIAbMDBDirectory NOT IMPLEMENTED methods

/* nsIAbCard addChildCards (in string uriName); */
NS_IMETHODIMP nsAbMDBDirProperty::AddChildCards(const char *uriName, nsIAbCard **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAbDirectory addDirectory (in string uriName); */
NS_IMETHODIMP nsAbMDBDirProperty::AddDirectory(const char *uriName, nsIAbDirectory **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string getDirUri (); */
NS_IMETHODIMP nsAbMDBDirProperty::GetDirUri(char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void removeElementsFromAddressList (); */
NS_IMETHODIMP nsAbMDBDirProperty::RemoveElementsFromAddressList()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeEmailAddressAt (in unsigned long aIndex); */
NS_IMETHODIMP nsAbMDBDirProperty::RemoveEmailAddressAt(PRUint32 aIndex)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void notifyDirItemAdded (in nsISupports item); */
NS_IMETHODIMP nsAbMDBDirProperty::NotifyDirItemAdded(nsISupports *item)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void clearDatabase (); */
NS_IMETHODIMP nsAbMDBDirProperty::ClearDatabase()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

