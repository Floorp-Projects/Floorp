/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): 
 */
#ifndef nsImportMimeEncode_h__
#define nsImportMimeEncode_h__

#include "nsImportScanFile.h"
#include "ImportOutFile.h"
#include "nsImportEncodeScan.h"
#include "nsString.h"
#include "nsIImportMimeEncode.h"


// Content-Type: image/gif; name="blah.xyz"
// Content-Transfer-Encoding: base64
// Content-Disposition: attachment; filename="blah.xyz"

class nsImportMimeEncode : public nsImportEncodeScan {
public:
	nsImportMimeEncode();
	~nsImportMimeEncode();
	
	void	EncodeFile( nsIFileSpec *pInFile, ImportOutFile *pOut, const char *pFileName, const char *pMimeType);

	PRBool	DoWork( PRBool *pDone);
	
	long	NumBytesProcessed( void) { long val = m_bytesProcessed; m_bytesProcessed = 0; return( val);}

protected:
	void	CleanUp( void);
	PRBool	SetUpEncode( void);
	PRBool	WriteFileName( nsCString& fName, PRBool wasTrans, const char *pTag);
	PRBool	TranslateFileName( nsCString& inFile, nsCString& outFile);


	virtual PRBool	ScanBuffer( PRBool *pDone);


protected:
	nsCString			m_fileName;
	nsIFileSpec *		m_pMimeFile;
	ImportOutFile *		m_pOut;
	nsCString			m_mimeType;

	int				m_state;
	long			m_bytesProcessed;
	PRUint8 *		m_pInputBuf;
	PRBool			m_appleSingle;
	
	// Actual encoding variables
	int			m_lineLen;
};


class nsIImportMimeEncodeImpl : public nsIImportMimeEncode {
public:
	NS_DECL_ISUPPORTS

	NS_DECL_NSIIMPORTMIMEENCODE

	nsIImportMimeEncodeImpl();
	virtual ~nsIImportMimeEncodeImpl();

 	static NS_METHOD Create( nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
	ImportOutFile *			m_pOut;
	nsImportMimeEncode *	m_pEncode;
};


#endif /* nsImportMimeEncode_h__ */

