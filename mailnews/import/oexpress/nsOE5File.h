/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsOE5File_h___
#define nsOE5File_h___

#include "nsString.h"
#include "nsIFileSpec.h"

class nsOE5File
{
public:
		/* pFile must already be open for reading. */
	static PRBool	VerifyLocalMailFile( nsIFileSpec *pFile);
		/* pFile must NOT be open for reading	 */
	static PRBool	IsLocalMailFile( nsIFileSpec *pFile);

	static PRBool	ReadIndex( nsIFileSpec *pFile, PRUint32 **ppIndex, PRUint32 *pSize);

	static nsresult	ImportMailbox( PRBool *pAbort, nsString& name, nsIFileSpec *inFile, nsIFileSpec *pDestination);

private:
	typedef struct {
		PRUint32 *	pIndex;
		PRUint32	count;
		PRUint32	alloc;
	} PRUint32Array;
	
	static const char *m_pFromLineSep;

	static PRBool	ReadBytes( nsIFileSpec *stream, void *pBuffer, PRUint32 offset, PRUint32 bytes);
	static PRUint32 ReadMsgIndex( nsIFileSpec *file, PRUint32 offset, PRUint32Array *pArray);
	static void		ConvertIndex( nsIFileSpec *pFile, char *pBuffer, PRUint32 *pIndex, PRUint32 size);
	static nsresult WriteMessageBuffer( nsIFileSpec *stream, char *pBuffer, PRUint32 size, char *last2);
	static PRBool	IsFromLine( char *pLine, PRUint32 len);


};



#endif /* nsOE5File_h___ */
