/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _ADDRPARSER_H_
#define _ADDRPARSER_H_

#include "abcom.h"
#include "abcinfo.h"  // pick up the ab_referenced object class
#include "msgmast.h"
#include "ptrarray.h"


/**************************************************************************************************************

  The address parser as the name implies is repsonsible for parsing potential addresses, making sure they 
  are formatted correctly (quoting special characters, etc.) in conjunction with making sure email addresses
  have domain names assigned to them or at least the default domain.

  There is only one address parser. So it is created through a static call to create and is cached. The address parser
  is the only element in the cache. 

  It is ref counted. When you get one through ::Create, you are acquiring the object. Release when you are done with
  it. 

******************************************************************************************************************/

class AB_AddressParser : public AB_ReferencedObject
{
protected:
	AB_AddressParser(MSG_Master * master);
	virtual ~AB_AddressParser();

public:
	static int Create(MSG_Master * master, AB_AddressParser ** parser);
	virtual int Close(); // closes and deletes

	// parsing addresses....
	// If you are trying to build an address string, use this function to format the name and address, convert to RFC822 and 
	// then tack it on to the comma delimited string: addresses. We do not check for duplicates...that is your responsibility.
	int FormatAndAddToAddressString (char ** addresses, const char * name, const char * address, XP_Bool ensureDomainIsPresent);
	
	// if you are trying to build an address string from a naked address, use this function. The returned address string
	// is an RFC822 string and may contain multiple addresses if the naked address contained characters like a space. 
	int FormatNakedAddress(char ** address, const char * nakedAddress, XP_Bool ensureDomainIsPresent);

protected:
	void DetermineDefaultDomain();

	char * m_defaultDomain;
	MSG_Master * m_master;
};

#endif /* _ADDRPARSER_H_ */
