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

#ifndef nsTextImport_h___
#define nsTextImport_h___

#include "nsIImportModule.h"
#include "nsCOMPtr.h"


#define NS_TEXTIMPORT_CID					\
{ /* A5991D01-ADA7-11d3-A9C2-00A0CC26DA63 */      \
	0xa5991d01, 0xada7, 0x11d3,						\
	{0xa9, 0xc2, 0x0, 0xa0, 0xcc, 0x26, 0xda, 0x63 }}



#define kTextSupportsString NS_IMPORT_ADDRESS_STR

class nsTextImport : public nsIImportModule
{
public:

	nsTextImport();
	virtual ~nsTextImport();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIImportModule interface 
	////////////////////////////////////////////////////////////////////////////////////////

	NS_DECL_NSIIMPORTMODULE

		
protected:
};



#endif /* nsTextImport_h___ */
