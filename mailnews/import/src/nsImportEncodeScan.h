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

