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
