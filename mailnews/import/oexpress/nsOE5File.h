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

	static nsresult	ImportMailbox( PRUint32 *pBytesDone, PRBool *pAbort, nsString& name, nsIFileSpec *inFile, nsIFileSpec *pDestination, PRUint32 *pCount);

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
