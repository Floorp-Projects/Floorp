/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsOEImport_h___
#define nsOEImport_h___

#include "nsIImportModule.h"
#include "nsCOMPtr.h"

#define NS_OEIMPORT_CID						  \
{ /* be0bc880-1742-11d3-a206-00a0cc26da63 */      \
 	0xbe0bc880, 0x1742, 0x11d3,                   \
 	{0xa2, 0x06, 0x0, 0xa0, 0xcc, 0x26, 0xda, 0x63}}



#define kOESupportsString NS_IMPORT_MAIL_STR "," NS_IMPORT_ADDRESS_STR "," NS_IMPORT_SETTINGS_STR

class nsOEImport : public nsIImportModule
{
public:

	nsOEImport();
	virtual ~nsOEImport();
	
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIImportModule interface 
	////////////////////////////////////////////////////////////////////////////////////////


	/* readonly attribute wstring name; */
	NS_IMETHOD GetName(PRUnichar * *aName);
	
	/* readonly attribute wstring description; */
	NS_IMETHOD GetDescription(PRUnichar * *aDescription);
	
	/* readonly attribute string supports; */
	NS_IMETHOD GetSupports(char * *aSupports);
	
	/* nsISupports GetImportInterface (in string importType); */
	NS_IMETHOD GetImportInterface(const char *importType, nsISupports **_retval);
		
protected:		
};

extern nsresult NS_NewOEImport(nsIImportModule** aImport);


#endif /* nsOEImport_h___ */
