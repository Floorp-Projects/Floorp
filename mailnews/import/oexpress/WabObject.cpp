/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsCRT.h"
#include "wabobject.h"



enum {
    ieidPR_DISPLAY_NAME = 0,
    ieidPR_ENTRYID,
	ieidPR_OBJECT_TYPE,
    ieidMax
};

static const SizedSPropTagArray(ieidMax, ptaEid)=
{
    ieidMax,
    {
        PR_DISPLAY_NAME,
        PR_ENTRYID,
		PR_OBJECT_TYPE,
    }
};


enum {
    iemailPR_DISPLAY_NAME = 0,
    iemailPR_ENTRYID,
    iemailPR_EMAIL_ADDRESS,
    iemailPR_OBJECT_TYPE,
    iemailMax
};
static const SizedSPropTagArray(iemailMax, ptaEmail)=
{
    iemailMax,
    {
        PR_DISPLAY_NAME,
        PR_ENTRYID,
        PR_EMAIL_ADDRESS,
        PR_OBJECT_TYPE
    }
};

typedef struct {
	PRBool		multiLine;
	ULONG		tag;
	char *		pLDIF;
} AddrImportField;

#define	kExtraUserFields	10
AddrImportField		extraUserFields[kExtraUserFields] = {
	{PR_TRUE, PR_COMMENT, "description:"},
	{PR_FALSE, PR_BUSINESS_TELEPHONE_NUMBER, "telephonenumber:"},
	{PR_FALSE, PR_HOME_TELEPHONE_NUMBER, "homephone:"},
	{PR_FALSE, PR_COMPANY_NAME, "o:"},
	{PR_FALSE, PR_TITLE, "title:"},
	{PR_FALSE, PR_BUSINESS_FAX_NUMBER, "facsimiletelephonenumber:"},
	{PR_FALSE, PR_LOCALITY, "locality:"},
	{PR_FALSE, PR_STATE_OR_PROVINCE, "st:"},
	{PR_TRUE, PR_STREET_ADDRESS, "streetaddress:"},
	{PR_FALSE, PR_POSTAL_CODE, "postalcode:"}
};

#define	kWhitespace	" \t\b\r\n"

#define TR_OUTPUT_EOL	"\r\n"

#define	kLDIFPerson		"objectclass: top" TR_OUTPUT_EOL "objectclass: person" TR_OUTPUT_EOL
#define kLDIFGroup		"objectclass: top" TR_OUTPUT_EOL "objectclass: groupOfNames" TR_OUTPUT_EOL

/*********************************************************************************/


// contructor for CWAB object
//
// pszFileName - FileName of WAB file to open
//          if no file name is specified, opens the default
//
CWAB::CWAB(nsIFileSpec *file)
{
    // Here we load the WAB Object and initialize it
    m_pUniBuff = NULL;
	m_uniBuffLen = 0;

	m_bInitialized = PR_FALSE;
	m_lpAdrBook = NULL;
	m_lpWABObject = NULL;
	m_hinstWAB = NULL;

    {
        TCHAR  szWABDllPath[MAX_PATH];
        DWORD  dwType = 0;
        ULONG  cbData = sizeof(szWABDllPath);
        HKEY hKey = NULL;

        *szWABDllPath = '\0';
        
        // First we look under the default WAB DLL path location in the
        // Registry. 
        // WAB_DLL_PATH_KEY is defined in wabapi.h
        //
        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, WAB_DLL_PATH_KEY, 0, KEY_READ, &hKey))
            RegQueryValueEx( hKey, "", NULL, &dwType, (LPBYTE) szWABDllPath, &cbData);

        if(hKey) RegCloseKey(hKey);

        // if the Registry came up blank, we do a loadlibrary on the wab32.dll
        // WAB_DLL_NAME is defined in wabapi.h
        //
        m_hinstWAB = LoadLibrary( (lstrlen(szWABDllPath)) ? szWABDllPath : WAB_DLL_NAME );
    }

    if(m_hinstWAB)
    {
        // if we loaded the dll, get the entry point 
        //
        m_lpfnWABOpen = (LPWABOPEN) GetProcAddress(m_hinstWAB, "WABOpen");

        if(m_lpfnWABOpen)
        {
        	char		fName[2] = {0, 0};
        	char *		pPath = nsnull;
            HRESULT hr = E_FAIL;
            WAB_PARAM wp = {0};
            wp.cbSize = sizeof(WAB_PARAM);
            if (file != nsnull) {
            	file->GetNativePath( &pPath);	
            	wp.szFileName = (LPTSTR) pPath;
            }
            else
            	wp.szFileName = (LPTSTR) fName;
        
            // if we choose not to pass in a WAB_PARAM object, 
            // the default WAB file will be opened up
            //
            hr = m_lpfnWABOpen(&m_lpAdrBook,&m_lpWABObject,&wp,0);

            if(!hr)
                m_bInitialized = TRUE;
            
            if (pPath)
            	nsCRT::free( pPath);
        }
    }

}


// Destructor
//
CWAB::~CWAB()
{
	if (m_pUniBuff)
		delete [] m_pUniBuff;

    if(m_bInitialized)
    {
        if(m_lpAdrBook)
            m_lpAdrBook->Release();

        if(m_lpWABObject)
            m_lpWABObject->Release();

        if(m_hinstWAB)
            FreeLibrary(m_hinstWAB);
    }
}


HRESULT CWAB::IterateWABContents(CWabIterator *pIter, int *pDone)
{
	if (!m_bInitialized || !m_lpAdrBook)
		return( E_FAIL);

    ULONG			ulObjType =   0;
	LPMAPITABLE		lpAB =  NULL;
    ULONG			cRows =       0;
 	LPSRowSet		lpRowAB = NULL;
    LPABCONT		lpContainer = NULL;
	int				cNumRows = 0;
	PRBool			keepGoing = PR_TRUE;

    HRESULT			hr = E_FAIL;

    ULONG			lpcbEID = 0;
	LPENTRYID		lpEID = NULL;
	ULONG			rowCount = 0;
	ULONG			curCount = 0;

	nsString		uniStr;

    // Get the entryid of the root PAB container
    //
    hr = m_lpAdrBook->GetPAB( &lpcbEID, &lpEID);

	if (HR_FAILED( hr))
		goto exit;

	ulObjType = 0;

    // Open the root PAB container
    // This is where all the WAB contents reside
    //
    hr = m_lpAdrBook->OpenEntry(lpcbEID,
					    		(LPENTRYID)lpEID,
						    	NULL,
							    0,
							    &ulObjType,
							    (LPUNKNOWN *)&lpContainer);

	m_lpWABObject->FreeBuffer(lpEID);

	lpEID = NULL;
	
    if(HR_FAILED(hr))
        goto exit;

    // Get a contents table of all the contents in the
    // WABs root container
    //
    hr = lpContainer->GetContentsTable( 0,
            							&lpAB);

    if(HR_FAILED(hr))
        goto exit;
	
	hr = lpAB->GetRowCount( 0, &rowCount);
	if (HR_FAILED(hr))
		rowCount = 100;
	if (rowCount == 0)
		rowCount = 1;

    // Order the columns in the ContentsTable to conform to the
    // ones we want - which are mainly DisplayName, EntryID and
    // ObjectType
    // The table is gauranteed to set the columns in the order 
    // requested
    //
	hr =lpAB->SetColumns( (LPSPropTagArray)&ptaEid, 0 );

    if(HR_FAILED(hr))
        goto exit;


    // Reset to the beginning of the table
    //
	hr = lpAB->SeekRow( BOOKMARK_BEGINNING, 0, NULL );

    if(HR_FAILED(hr))
        goto exit;

    // Read all the rows of the table one by one
    //

	do {

		hr = lpAB->QueryRows(1,	0, &lpRowAB);

        if(HR_FAILED(hr))
            break;

        if(lpRowAB)
        {
            cNumRows = lpRowAB->cRows;

		    if (cNumRows)
		    {
                LPTSTR lpsz = lpRowAB->aRow[0].lpProps[ieidPR_DISPLAY_NAME].Value.lpszA;
                LPENTRYID lpEID = (LPENTRYID) lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
                ULONG cbEID = lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;

                // There are 2 kinds of objects - the MAPI_MAILUSER contact object
                // and the MAPI_DISTLIST contact object
                // For the purposes of this sample, we will only consider MAILUSER
                // objects
                //
                if(lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_MAILUSER)
                {
                    // We will now take the entry-id of each object and cache it
                    // on the listview item representing that object. This enables
                    // us to uniquely identify the object later if we need to
                    //
					CStrToUnicode( lpsz, uniStr);
					keepGoing = pIter->EnumUser( uniStr.get(), lpEID, cbEID);
					curCount++;
					if (pDone) {
						*pDone = (curCount * 100) / rowCount;
						if (*pDone > 100)
							*pDone = 100;
					}
                }
		    }
		    FreeProws(lpRowAB );		
        }
		

	} while ( SUCCEEDED(hr) && cNumRows && lpRowAB && keepGoing)  ;

	hr = lpAB->SeekRow( BOOKMARK_BEGINNING, 0, NULL );

    if(HR_FAILED(hr))
        goto exit;

    // Read all the rows of the table one by one
    //
	keepGoing = TRUE;
	do {

		hr = lpAB->QueryRows(1,	0, &lpRowAB);

        if(HR_FAILED(hr))
            break;

        if(lpRowAB)
        {
            cNumRows = lpRowAB->cRows;

		    if (cNumRows)
		    {
                LPTSTR lpsz = lpRowAB->aRow[0].lpProps[ieidPR_DISPLAY_NAME].Value.lpszA;
                LPENTRYID lpEID = (LPENTRYID) lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
                ULONG cbEID = lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;

                // There are 2 kinds of objects - the MAPI_MAILUSER contact object
                // and the MAPI_DISTLIST contact object
                // For the purposes of this sample, we will only consider MAILUSER
                // objects
                //
                if(lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_DISTLIST)
                {
                    // We will now take the entry-id of each object and cache it
                    // on the listview item representing that object. This enables
                    // us to uniquely identify the object later if we need to
                    //
					CStrToUnicode( lpsz, uniStr);
					keepGoing = pIter->EnumList( uniStr.get(), lpEID, cbEID);
					curCount++;
					if (pDone) {
						*pDone = (curCount * 100) / rowCount;
						if (*pDone > 100)
							*pDone = 100;
					}
                 }
		    }
		    FreeProws(lpRowAB );		
        }

	} while ( SUCCEEDED(hr) && cNumRows && lpRowAB && keepGoing)  ;


exit:

	if ( lpContainer )
		lpContainer->Release();

	if ( lpAB )
		lpAB->Release();

    return hr;
}






void CWAB::FreeProws(LPSRowSet prows)
{
	ULONG		irow;
	if (!prows)
		return;
	for (irow = 0; irow < prows->cRows; ++irow)
		m_lpWABObject->FreeBuffer(prows->aRow[irow].lpProps);
	m_lpWABObject->FreeBuffer(prows);
}


LPDISTLIST CWAB::GetDistList( ULONG cbEid, LPENTRYID pEid)
{
	if (!m_bInitialized || !m_lpAdrBook)
		return( NULL);
	
	LPDISTLIST	lpDistList = NULL;
	ULONG		ulObjType;

	m_lpAdrBook->OpenEntry( cbEid, pEid, NULL, 0, &ulObjType, (LPUNKNOWN *)&lpDistList);
	return( lpDistList);
}

LPSPropValue CWAB::GetListProperty( LPDISTLIST pUser, ULONG tag)
{
	if (!pUser)
		return( NULL);

	int	sz = CbNewSPropTagArray( 1);
	SPropTagArray *pTag = (SPropTagArray *) new char[sz];
	pTag->cValues = 1;
	pTag->aulPropTag[0] = tag;
	LPSPropValue	lpProp = NULL;
	ULONG	cValues = 0;
	HRESULT hr = pUser->GetProps( pTag, 0, &cValues, &lpProp);
	delete pTag;
	if (HR_FAILED( hr) || (cValues != 1)) {
		if (lpProp)
			m_lpWABObject->FreeBuffer( lpProp);
		return( NULL);
	}
	return( lpProp);
}

LPMAILUSER CWAB::GetUser( ULONG cbEid, LPENTRYID pEid)
{
	if (!m_bInitialized || !m_lpAdrBook)
		return( NULL);
	
	LPMAILUSER	lpMailUser = NULL;
	ULONG		ulObjType;

	m_lpAdrBook->OpenEntry( cbEid, pEid, NULL, 0, &ulObjType, (LPUNKNOWN *)&lpMailUser);
	return( lpMailUser);
}

LPSPropValue CWAB::GetUserProperty( LPMAILUSER pUser, ULONG tag)
{
	if (!pUser)
		return( NULL);

	ULONG	uTag = tag;
	/* 
		Getting Unicode does not help with getting the right
		international charset.  Windoze bloze.
	*/
	/*
	if (PROP_TYPE( uTag) == PT_STRING8) {
		uTag = CHANGE_PROP_TYPE( tag, PT_UNICODE);
	}
	*/

	int	sz = CbNewSPropTagArray( 1);
	SPropTagArray *pTag = (SPropTagArray *) new char[sz];
	pTag->cValues = 1;
	pTag->aulPropTag[0] = uTag;
	LPSPropValue	lpProp = NULL;
	ULONG	cValues = 0;
	HRESULT hr = pUser->GetProps( pTag, 0, &cValues, &lpProp);
	if (HR_FAILED( hr) || (cValues != 1)) {
		if (lpProp)
			m_lpWABObject->FreeBuffer( lpProp);
		lpProp = NULL;
		if (uTag != tag) {
			pTag->cValues = 1;
			pTag->aulPropTag[0] = tag;
			cValues = 0;
			hr = pUser->GetProps( pTag, 0, &cValues, &lpProp);
			if (HR_FAILED( hr) || (cValues != 1)) {
				if (lpProp)
					m_lpWABObject->FreeBuffer( lpProp);
				lpProp = NULL;
			}
		}
	}
	delete pTag;
	return( lpProp);
}

void CWAB::CStrToUnicode( const char *pStr, nsString& result)
{
	result.Truncate( 0);
	int wLen = MultiByteToWideChar( CP_ACP, 0, pStr, -1, m_pUniBuff, 0);
	if (wLen >= m_uniBuffLen) {
		if (m_pUniBuff)
			delete [] m_pUniBuff;
		m_pUniBuff = new PRUnichar[wLen + 64];
		m_uniBuffLen = wLen + 64;
	}
	if (wLen) {
		MultiByteToWideChar( CP_ACP, 0, pStr, -1, m_pUniBuff, m_uniBuffLen);
		result = m_pUniBuff;
	}
}

// If the value is a string, get it...
void CWAB::GetValueString( LPSPropValue pVal, nsString& val)
{
	val.Truncate( 0);
	
	if (!pVal)
		return;

    switch( PROP_TYPE( pVal->ulPropTag)) {
	case PT_STRING8: {
			CStrToUnicode( (const char *) (pVal->Value.lpszA), val);
		}
        break;
		case PT_UNICODE:
			val = (PRUnichar *) (pVal->Value.lpszW);
		break;
		case PT_MV_STRING8: {
			nsString	tmp;
            ULONG	j;
            for(j = 0; j < pVal->Value.MVszA.cValues; j++) {
				CStrToUnicode( (const char *) (pVal->Value.MVszA.lppszA[j]), tmp);
                val += tmp;
                val.AppendWithConversion(TR_OUTPUT_EOL);
            }
        }
        break;
		case PT_MV_UNICODE: {
            ULONG	j;
            for(j = 0; j < pVal->Value.MVszW.cValues; j++) {
                val += (PRUnichar *) (pVal->Value.MVszW.lppszW[j]);
                val.AppendWithConversion(TR_OUTPUT_EOL);
            }
        }
        break;

		case PT_I2:
		case PT_LONG:
		case PT_R4:
		case PT_DOUBLE:
		case PT_BOOLEAN: {    
			/*
			TCHAR sz[256];
            wsprintf(sz,"%d", pVal->Value.l);
            val = sz;
			*/
        }
        break;

		case PT_BINARY:
		break;

		default:
        break;
    }

	val.Trim( kWhitespace, PR_TRUE, PR_TRUE);
}





/*
BOOL CWabIterateProcess::SanitizeMultiLine( CString& val)
{
	val.TrimLeft();
	val.TrimRight();
	int idx = val.FindOneOf( "\x0D\x0A");
	if (idx == -1)
		return( FALSE);

	// needs encoding
	U32 bufSz = UMimeEncode::GetBufferSize( val.GetLength());
	P_U8 pBuf = new U8[bufSz];
	U32 len = UMimeEncode::ConvertBuffer( (PC_U8)((PC_S8)val), val.GetLength(), pBuf, 66, 52, "\x0D\x0A ");
	pBuf[len] = 0;
	val = pBuf;
	delete pBuf;
	return( TRUE);
}

BOOL CWabIterateProcess::EnumUser( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid)
{
	TRACE1( "User: %s\n", pName);

	LPMAILUSER	pUser = m_pWab->GetUser( cbEid, pEid);
	
	// Get the "required" strings first
	CString		lastName;
	CString		firstName;
	CString		eMail;
	CString		nickName;
	CString		middleName;

	if (!pUser) {
		UDialogs::ErrMessage1( IDS_ENTRY_ERROR, pName);
		return( FALSE);
	}

	LPSPropValue	pProp = m_pWab->GetUserProperty( pUser, PR_EMAIL_ADDRESS);
	if (pProp) {
		m_pWab->GetValueString( pProp, eMail);
		SanitizeValue( eMail);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_GIVEN_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, firstName);
		SanitizeValue( firstName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_SURNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, lastName);
		SanitizeValue( lastName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_MIDDLE_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, middleName);
		SanitizeValue( middleName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_NICKNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, nickName);
		SanitizeValue( nickName);
		m_pWab->FreeProperty( pProp);
	}
	if (nickName.IsEmpty())
		nickName = pName;
	if (firstName.IsEmpty()) {
		firstName = nickName;
		middleName.Empty();
		lastName.Empty();
	}
	if (lastName.IsEmpty())
		middleName.Empty();

	if (eMail.IsEmpty())
		eMail = nickName;


	// We now have the required fields
	// write them out followed by any optional fields!
	BOOL	result = TRUE;

	if (m_recordsDone)
		result = m_out.WriteEol();

	CString		line;
	CString		header;
	line.LoadString( IDS_LDIF_DN_START);
	line += firstName;
	if (!middleName.IsEmpty()) {
		line += ' ';
		line += middleName;
	}
	if (!lastName.IsEmpty()) {
		line += ' ';
		line += lastName;
	}
	header.LoadString( IDS_LDIF_DN_MIDDLE);
	line += header;
	line += eMail;
	result = result && m_out.WriteStr( line);
	result = result && m_out.WriteEol();

	line.LoadString( IDS_FIELD_LDIF_FULLNAME);
	line += ' ';
	line += firstName;
	if (!middleName.IsEmpty()) {
		line += ' ';
		line += middleName;
	}
	if (!lastName.IsEmpty()) {
		line += ' ';
		line += lastName;
	}
	result = result && m_out.WriteStr( line);
	result = result && m_out.WriteEol();


	line.LoadString( IDS_FIELD_LDIF_GIVENNAME);
	line += ' ';
	line += firstName;
	result = result && m_out.WriteStr( line);
	result = result && m_out.WriteEol();

	if (!lastName.IsEmpty()) {
		line.LoadString( IDS_FIELD_LDIF_LASTNAME);
		if (!middleName.IsEmpty()) {
			line += ' ';
			line += middleName;
		}
		line += ' ';
		line += lastName;
		result = result && m_out.WriteStr( line);
		result = result && m_out.WriteEol();
	}

	result = result && m_out.WriteStr( kLDIFPerson);
	
	line.LoadString( IDS_FIELD_LDIF_EMAIL);
	line += ' ';
	line += eMail;
	result = result && m_out.WriteStr( line);
	result = result && m_out.WriteEol();

	line.LoadString( IDS_FIELD_LDIF_NICKNAME);
	line += ' ';
	line += nickName;
	result = result && m_out.WriteStr( line);
	result = result && m_out.WriteEol();

	// Do all of the extra fields!
	CString	value;
	BOOL	encoded = FALSE;
	for (int i = 0; i < kExtraUserFields; i++) {
		value.Empty();
		pProp = m_pWab->GetUserProperty( pUser, extraUserFields[i].tag);
		if (pProp) {
			m_pWab->GetValueString( pProp, value);
			m_pWab->FreeProperty( pProp);
		}
		if (extraUserFields[i].multiLine) {
			encoded = SanitizeMultiLine( value);
		}
		else
			SanitizeValue( value);
		if (!value.IsEmpty()) {
			line = extraUserFields[i].pLDIF;
			if (encoded) {
				line += ": ";
				encoded = FALSE;
			}
			else
				line += ' ';
			line += value;
			result = result && m_out.WriteStr( line);
			result = result && m_out.WriteEol();
		}
	}

	m_pWab->ReleaseUser( pUser);

	if (!result) {
		UDialogs::ErrMessage0( IDS_ADDRESS_SAVE_ERROR);
	}

	m_totalDone += kValuePerUser;
	m_recordsDone++;

	return( result);
}
*/


 

/*
BOOL CWabIterateProcess::EnumList( LPCTSTR pName, LPENTRYID pEid, ULONG cbEid)
{
	TRACE1( "List: %s\n", pName);

	LPDISTLIST		pList = m_pWab->GetDistList( cbEid, pEid);
	if (!pList) {
		UDialogs::ErrMessage1( IDS_ENTRY_ERROR, pName);
		return( FALSE);
	}

	// Find out if this is just a regular entry or a true list...
	CString			eMail;
	LPSPropValue	pProp = m_pWab->GetListProperty( pList, PR_EMAIL_ADDRESS);
	if (pProp) {
		m_pWab->GetValueString( pProp, eMail);
		SanitizeValue( eMail);
		m_pWab->FreeProperty( pProp);
		// Treat this like a regular entry...
		if (!eMail.IsEmpty()) {
			m_pWab->ReleaseDistList( pList);
			return( WriteListUserEntry( pName, eMail));
		}
	}

	// This may very well be a list, find the entries...
	m_pListTable = OpenDistList( pList);
	if (m_pListTable) {
		m_pList = pList;
		m_listName = pName;
		m_listDone = 0;
		m_listHeaderDone = FALSE;
		m_state = kEnumListState;
	}
	else {
		m_pWab->ReleaseDistList( pList);
		m_recordsDone++;
		m_totalDone += kValuePerUser;
	}

	return( TRUE);
}

BOOL CWabIterateProcess::EnumNextListUser( BOOL *pDone)
{
	HRESULT			hr;
	int				cNumRows = 0;
	LPSRowSet		lpRowAB = NULL;
	BOOL			keepGoing = TRUE;

	if (!m_pListTable)
		return( FALSE);

	hr = m_pListTable->QueryRows( 1, 0, &lpRowAB);

	if(HR_FAILED(hr)) {
		UDialogs::ErrMessage0( IDS_ERROR_READING_WAB);
		return( FALSE);
	}

	if(lpRowAB) {
		cNumRows = lpRowAB->cRows;

		if (cNumRows) {
			LPTSTR lpsz = lpRowAB->aRow[0].lpProps[ieidPR_DISPLAY_NAME].Value.lpszA;
			LPENTRYID lpEID = (LPENTRYID) lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
			ULONG cbEID = lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;
            if(lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_DISTLIST) {
				keepGoing = HandleListList( lpsz, lpEID, cbEID);     
		    }
			else if (lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_MAILUSER) {
				keepGoing = HandleListUser( lpsz, lpEID, cbEID);     
			}
		}
		m_pWab->FreeProws( lpRowAB);		
 	}

	if (!cNumRows || !lpRowAB) {
		*pDone = TRUE;
		m_pListTable->Release();
		m_pListTable = NULL;
		if (m_pList)
			m_pWab->ReleaseDistList( m_pList);
		m_pList = NULL;
		if (m_listDone < kValuePerUser)
			m_totalDone += (kValuePerUser - m_listDone);
		m_recordsDone++;
		return( keepGoing);
	}

	if (!keepGoing)
		return( FALSE);

	if (m_listDone < kValuePerUser) {
		m_listDone++;
		m_totalDone++;
	}

	return( TRUE);
}

BOOL CWabIterateProcess::HandleListList( LPCTSTR pName, LPENTRYID lpEid, ULONG cbEid)
{
	BOOL			result;
	LPDISTLIST		pList = m_pWab->GetDistList( cbEid, lpEid);
	if (!pList) {
		UDialogs::ErrMessage1( IDS_ENTRY_ERROR, pName);
		return( FALSE);
	}

	CString			eMail;
	LPSPropValue	pProp = m_pWab->GetListProperty( pList, PR_EMAIL_ADDRESS);
	if (pProp) {
		m_pWab->GetValueString( pProp, eMail);
		SanitizeValue( eMail);
		m_pWab->FreeProperty( pProp);
		// Treat this like a regular entry...
		if (!eMail.IsEmpty()) {
			// write out a member based on pName and eMail
			result = WriteGroupMember( pName, eMail);
			m_pWab->ReleaseDistList( pList);
			return( result);
		}
	}

	// iterate the list and add each member to the top level list
	LPMAPITABLE	pTable = OpenDistList( pList);
	if (!pTable) {
		TRACE0( "Error opening table for list\n");
		m_pWab->ReleaseDistList( pList);
		UDialogs::ErrMessage1( IDS_ENTRY_ERROR, pName);
		return( FALSE);
	}

	int				cNumRows = 0;
	LPSRowSet		lpRowAB = NULL;
	HRESULT			hr;
	BOOL			keepGoing = TRUE;

	do {
		hr = pTable->QueryRows( 1, 0, &lpRowAB);

		if(HR_FAILED(hr)) {
			UDialogs::ErrMessage0( IDS_ERROR_READING_WAB);
			pTable->Release();
			m_pWab->ReleaseDistList( pList);
			return( FALSE);
		}

		if(lpRowAB) {
			cNumRows = lpRowAB->cRows;

			if (cNumRows) {
				LPTSTR lpsz = lpRowAB->aRow[0].lpProps[ieidPR_DISPLAY_NAME].Value.lpszA;
				LPENTRYID lpEID = (LPENTRYID) lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
				ULONG cbEID = lpRowAB->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;
				if(lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_DISTLIST) {
					keepGoing = HandleListList( lpsz, lpEID, cbEID);     
				}
				else if (lpRowAB->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.l == MAPI_MAILUSER) {
					keepGoing = HandleListUser( lpsz, lpEID, cbEID);     
				}
			}
			m_pWab->FreeProws( lpRowAB);		
 		}
	}
	while (keepGoing && cNumRows && lpRowAB);

	pTable->Release();
	m_pWab->ReleaseDistList( pList);
	return( keepGoing);
}

BOOL CWabIterateProcess::HandleListUser( LPCTSTR pName, LPENTRYID lpEid, ULONG cbEid)
{
	// Get the basic properties for building the member line
	LPMAILUSER	pUser = m_pWab->GetUser( cbEid, lpEid);
	
	// Get the "required" strings first
	CString		lastName;
	CString		firstName;
	CString		eMail;
	CString		nickName;
	CString		middleName;

	if (!pUser) {
		UDialogs::ErrMessage1( IDS_ENTRY_ERROR, pName);
		return( FALSE);
	}

	LPSPropValue	pProp = m_pWab->GetUserProperty( pUser, PR_EMAIL_ADDRESS);
	if (pProp) {
		m_pWab->GetValueString( pProp, eMail);
		SanitizeValue( eMail);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_GIVEN_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, firstName);
		SanitizeValue( firstName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_SURNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, lastName);
		SanitizeValue( lastName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_MIDDLE_NAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, middleName);
		SanitizeValue( middleName);
		m_pWab->FreeProperty( pProp);
	}
	pProp = m_pWab->GetUserProperty( pUser, PR_NICKNAME);
	if (pProp) {
		m_pWab->GetValueString( pProp, nickName);
		SanitizeValue( nickName);
		m_pWab->FreeProperty( pProp);
	}
	if (nickName.IsEmpty())
		nickName = pName;
	if (firstName.IsEmpty()) {
		firstName = nickName;
		middleName.Empty();
		lastName.Empty();
	}
	if (lastName.IsEmpty())
		middleName.Empty();

	if (eMail.IsEmpty())
		eMail = nickName;

	m_pWab->ReleaseUser( pUser);

	CString	name = firstName;
	if (!middleName.IsEmpty()) {
		name += ' ';
		name += middleName;
	}
	if (!lastName.IsEmpty()) {
		name += ' ';
		name += lastName;
	}
	return( WriteGroupMember( name, eMail));
}

BOOL CWabIterateProcess::WriteGroupMember( const char *pName, const char *pEmail)
{
	CString		middle;
	CString		line;
	BOOL		result;

	// Check for the header first
	if (!m_listHeaderDone) {
		if (m_recordsDone)
			result = m_out.WriteEol();
		else
			result = TRUE;
		line.LoadString( IDS_LDIF_DN_START);
		line += m_listName;
		line += TR_OUTPUT_EOL;
		middle.LoadString( IDS_FIELD_LDIF_FULLNAME);
		line += middle;
		line += m_listName;
		line += TR_OUTPUT_EOL;
		if (!result || !m_out.WriteStr( line) || !m_out.WriteStr( kLDIFGroup)) {
			UDialogs::ErrMessage0( IDS_ADDRESS_SAVE_ERROR);
			return( FALSE);
		}
		m_listHeaderDone = TRUE;
	}

	
	line.LoadString( IDS_FIELD_LDIF_MEMBER_START);
	line += pName;
	middle.LoadString( IDS_LDIF_DN_MIDDLE);
	line += middle;
	line += pEmail;
	line += TR_OUTPUT_EOL;
	if (!m_out.WriteStr( line)) {
		UDialogs::ErrMessage0( IDS_ADDRESS_SAVE_ERROR);
		return( FALSE);
	}

	if (m_listDone < kValuePerUser) {
		m_listDone++;
		m_totalDone++;
	}

	return( TRUE);
}
*/

