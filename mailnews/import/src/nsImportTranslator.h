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

#ifndef nsImportTranslator_h___
#define nsImportTranslator_h___

#include "nscore.h"
#include "nsString.h"
#include "nsCRT.h"

class ImportOutFile;

class UMimeEncode {
public:
	static PRUint32	GetBufferSize( PRUint32 inByes);
	static PRUint32	ConvertBuffer( const PRUint8 * pIn, PRUint32 inLen, PRUint8 *pOut, PRUint32 maxLen = 72, PRUint32 firstLineLen = 72, const char * pEolStr = nsnull);
};


class nsImportTranslator {
public:
	virtual ~nsImportTranslator() {}
	virtual PRBool		Supports8bitEncoding( void) { return( PR_FALSE);}
	virtual PRUint32	GetMaxBufferSize( PRUint32 inLen) { return( inLen + 1);}
	virtual void		ConvertBuffer( const PRUint8 * pIn, PRUint32 inLen, PRUint8 * pOut) { nsCRT::memcpy( pOut, pIn, inLen); pOut[inLen] = 0;}
	virtual PRBool		ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed = nsnull);
	virtual PRBool		FinishConvertToFile( ImportOutFile * /* pOutFile */) { return( PR_TRUE);}

	virtual void	GetCharset( nsCString& charSet) { charSet = "us-ascii";}
	virtual void	GetLanguage( nsCString& lang) { lang = "en";}
	virtual void	GetEncoding( nsCString& encoding) { encoding.Truncate();}
};

// Specialized encoder, not a vaild language translator, used for Mime headers.
// rfc2231
class CMHTranslator : public nsImportTranslator {
public:
	virtual PRUint32	GetMaxBufferSize( PRUint32 inLen) { return( (inLen * 3) + 1);}
	virtual void		ConvertBuffer( const PRUint8 * pIn, PRUint32 inLen, PRUint8 * pOut);
	virtual PRBool		ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed = nsnull);
};

// Specialized encoder, not a vaild language translator, used for mail headers
// rfc2047
class C2047Translator : public nsImportTranslator {
public:
	virtual ~C2047Translator() {}

	C2047Translator( const char *pCharset, PRUint32 headerLen) { m_charset = pCharset; m_startLen = headerLen; m_useQuotedPrintable = PR_FALSE;}

	void	SetUseQuotedPrintable( void) { m_useQuotedPrintable = PR_TRUE;}

	virtual PRBool	ConvertToFile( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed = nsnull);
	PRBool	ConvertToFileQ( const PRUint8 * pIn, PRUint32 inLen, ImportOutFile *pOutFile, PRUint32 *pProcessed);

protected:
	PRBool			m_useQuotedPrintable;
	nsCString		m_charset;
	PRUint32		m_startLen;
};

#endif /* nsImportTranslator_h__ */

