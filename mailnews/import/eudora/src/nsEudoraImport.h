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

#ifndef nsEudoraImport_h___
#define nsEudoraImport_h___

#include "nsIImportModule.h"
#include "nsCOMPtr.h"


#define NS_EUDORAIMPORT_CID					\
{ /* c8448da0-8f83-11d3-a206-00a0cc26da63 */      \
	0xc8448da0, 0x8f83, 0x11d3,						\
	{0xa2, 0x6, 0x0, 0xa0, 0xcc, 0x26, 0xda, 0x63 }}




#define kEudoraSupportsString NS_IMPORT_MAIL_STR "," NS_IMPORT_ADDRESS_STR "," NS_IMPORT_SETTINGS_STR

class nsEudoraImport : public nsIImportModule
{
public:

	nsEudoraImport();
	virtual ~nsEudoraImport();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIImportModule interface 
	////////////////////////////////////////////////////////////////////////////////////////

	NS_DECL_NSIIMPORTMODULE

		
protected:		
};


#endif /* nsEudoraImport_h___ */
