/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/********************************************************************************************************
 
   Interface for parsing RFC-822 addresses.
 
*********************************************************************************************************/

#ifndef nsMSGRFCPARSER_h__
#define nsMSGRFCPARSER_h__

#include "msgCore.h"
#include "nsIMsgHeaderParser.h" /* include the interface we are going to support */
#include "comi18n.h"

 /* 
  * RFC-822 parser
  */ 

class nsMsgHeaderParser: public nsIMsgHeaderParser 
{
public: 
  nsMsgHeaderParser();
	virtual ~nsMsgHeaderParser();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

  NS_DECL_NSIMSGHEADERPARSER
	
	MimeCharsetConverterClass *GetUSAsciiToUtf8CharsetConverter();
protected:
	MimeCharsetConverterClass *m_USAsciiToUtf8CharsetConverter;
}; 

#endif /* nsMSGRFCPARSER_h__ */
