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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsImportEncodeScan_h___
#define nsImportEncodeScan_h___

#include "nsIFileSpec.h"
#include "nsImportScanFile.h"
#include "nsString.h"

class nsImportEncodeScan : public nsImportScanFile {
public:
	nsImportEncodeScan();
	~nsImportEncodeScan();

	PRBool	InitEncodeScan( PRBool appleSingleEncode, nsIFileSpec *pSpec, const char *pName, PRUint8 * pBuf, PRUint32 sz);
	void	CleanUpEncodeScan( void);

	virtual PRBool	Scan( PRBool *pDone);

protected:
	void 	FillInEntries( int numEntries);
	PRBool 	AddEntries( void);

protected:
	PRBool			m_isAppleSingle;
	nsIFileSpec *	m_pInputFile;
	int				m_encodeScanState;
	long			m_resourceForkSize;
	long			m_dataForkSize;
	nsCString		m_useFileName;
};

#endif /* nsImportEncodeScan_h__ */

