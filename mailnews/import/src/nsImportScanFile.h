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

#ifndef nsImportScanFile_h__
#define nsImportScanFile_h__

#include "nsIFileSpec.h"

class nsImportScanFile {
public:
	nsImportScanFile();
	nsImportScanFile( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz);
	virtual ~nsImportScanFile();

	void	InitScan( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz);

	PRBool	OpenScan( nsIFileSpec *pSpec, PRUint32 bufSz = 4096);
	void	CleanUpScan( void);

	virtual	PRBool	Scan( PRBool *pDone);

protected:
	void			ShiftBuffer( void);
	PRBool			FillBufferFromFile( void);
	virtual PRBool	ScanBuffer( PRBool *pDone);

protected:
	nsIFileSpec *	m_pFile;
	PRUint8 *		m_pBuf;
	PRUint32		m_bufSz;
	PRUint32		m_bytesInBuf;
	PRUint32		m_pos;
	PRBool			m_eof;
	PRBool			m_allocated;
};

class nsImportScanFileLines : public nsImportScanFile {
public:
	nsImportScanFileLines() {m_needEol = PR_FALSE;}

	void	ResetLineScan( void) { m_needEol = PR_FALSE;}

	virtual PRBool ProcessLine( PRUint8 * /* pLine */, PRUint32 /* len */, PRBool * /* pDone */ ) {return( PR_TRUE);}

protected:
	virtual PRBool	ScanBuffer( PRBool *pDone);

	PRBool	m_needEol;

};


#endif /* nsImportScanFile_h__ */
