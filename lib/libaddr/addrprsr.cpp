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

#include "xp.h"

#include "addrprsr.h"
#include "prefapi.h"

/*****************************************************************************************

  Creating and Destroying methods....

******************************************************************************************/

AB_AddressParser::AB_AddressParser(MSG_Master * master)
{
	m_master = master;  // in case we need to get at prefs....
	m_defaultDomain = NULL; // lazy fetch for this....get it when we need it...
}

AB_AddressParser::~AB_AddressParser()
{
	XP_ASSERT(m_defaultDomain == NULL);
}

/* static */ int AB_AddressParser::Create(MSG_Master * master, AB_AddressParser ** parser /* caller passes in ptr to parser */)
{
	int status = AB_FAILURE;
	// right now just create one...we might want to make it a static object later....
	if (parser)
	{
		*parser = new AB_AddressParser(master);
		if (*parser)
			status = AB_SUCCESS;
		else
			status = AB_FAILURE;
	}
	else
		status = ABError_NullPointer;

	return status;
}

int AB_AddressParser::Close()
{
	if (m_defaultDomain)
	{
		XP_FREE(m_defaultDomain);
		m_defaultDomain = NULL;
	}

	return AB_SUCCESS;
}
/*****************************************************************************************

  Our movers and shakers.....this is the stuff that actually parses and assembles email
  addresses.

******************************************************************************************/

// take a naked address, parse it as an RFC822 address and put it in address. Caller must XP_FREE addresss parameter
int AB_AddressParser::FormatNakedAddress(char ** addresses, const char * nakedAddress, XP_Bool ensureDomainIsPresent)
{
	int status = AB_SUCCESS;
	if (nakedAddress && addresses)
	{
		*addresses = NULL;
		char * name = NULL;
		char * address = NULL;
		
		int num = MSG_ParseRFC822Addresses(nakedAddress, &name, &address);
		
		char * curname = name;
		char * curaddress = address;

		if (num)
		{
			for (int i = 0; i < num; i++)
			{
				FormatAndAddToAddressString(addresses, curname, curaddress, ensureDomainIsPresent);
				// increment to the next name in the list
				// of incoming addresses
				curname += XP_STRLEN(curname) + 1;
				curaddress += XP_STRLEN(curaddress) + 1;
			}
		}

		XP_FREEIF(name);
		XP_FREEIF(address);
	} // if naked address
	else
		status = ABError_NullPointer;

	return status;
}

int AB_AddressParser::FormatAndAddToAddressString(char ** addresses, const char * name, const char * address, XP_Bool ensureDomainIsPresent)
// this was grabbed pretty much intact from code for the 4.0x address book in ABook::AddToAddressString.
{
	XP_ASSERT(addresses && address);
	if (!addresses|| !address) 
		return ABError_NullPointer;
	
	char* muckedaddr = NULL;
	char * strippedAddr = NULL;
	char * escAddr = NULL;

	if (address)
		strippedAddr = XP_STRDUP (address);
	
	MSG_UnquotePhraseOrAddr (strippedAddr, &escAddr);

	if (!strippedAddr || !escAddr) 
	{
		XP_FREEIF(strippedAddr);
		XP_FREEIF(escAddr);
		return AB_FAILURE;
	}
	
	if (ensureDomainIsPresent) 
	{
		XP_Bool hasMachinePart =
			(XP_STRCHR(escAddr, '@') != NULL ||
			 XP_STRCHR(escAddr, '!') != NULL ||
			 XP_STRCHR(escAddr, '%') != NULL);
		if (!hasMachinePart) 
		{
			// This address has no domain part.  It doesn't even have a funky
			// extremely-old-fashioned UUCP "!"-style address.  Go muck it to
			// have a domain.
			if (!m_defaultDomain) 
				DetermineDefaultDomain();
			if (m_defaultDomain)
				muckedaddr = PR_smprintf("%s@%s", escAddr, m_defaultDomain);
		}
	}

	if (*addresses)   // if the string exists already then we have at least one name already in the list so add a comma b4 we add our name
		NET_SACat(addresses, ", ");

	// strip out the double quote if they exist
	// this will happen if an name contains
	// spaces or other funny characters
	// so we can reqoute the name 
	char * strippedName = XP_STRDUP (name);
	char * escName = NULL;
	MSG_UnquotePhraseOrAddr (strippedName, &escName);
	if (!strippedName || !escName) 
	{
		XP_FREEIF(strippedAddr);
		XP_FREEIF(escAddr);
		XP_FREEIF(muckedaddr);
		XP_FREEIF(strippedName);
		XP_FREEIF(escName);
		return ABError_NullPointer;
	}

	char* full = MSG_MakeFullAddress(escName, muckedaddr ? muckedaddr : escAddr);
	XP_FREEIF(strippedAddr);
	XP_FREEIF(escAddr);
	XP_FREEIF(strippedName);
	XP_FREEIF(escName);
	XP_FREEIF(muckedaddr);
	if (full) 
	{
		NET_SACat(addresses, full);
		XP_FREEIF (full);
	}

	return AB_SUCCESS;
}

void AB_AddressParser::DetermineDefaultDomain()
// this was grabbed pretty much intact from code for the 4.0x address book in Abook::DetermineDefaultDomain...
{
#ifdef MOZADDRSTANDALONE
	XP_ASSERT (FALSE);
	return (NULL);
#else
	if (m_defaultDomain)
		return;

	m_defaultDomain = NULL;

	/* Ignore all pref call backs until I've figured more things out! */
/* 

#ifdef XP_OS2_FIX
	PREF_RegisterCallback("mail.identity",	ABook::TossDefaultDomain_s, this);
#else
	PREF_RegisterCallback("mail.identity",	(PrefChangedFunc)ABook::TossDefaultDomain_s, this);
#endif 

*/
	// ### Need to unregister this callback when we get destroyed.
	// Unfortunately, prefapi doesn't yet provide a way to do that.

	PREF_CopyCharPref("mail.identity.defaultdomain", &m_defaultDomain);
	if (m_defaultDomain) 
	{
		// Get rid of extra whitespace.  If that means we end up with nothing,
		// then free all the allocated string and set m_defaultDomain back to
		// NULL.  Also, let's not let people put at-signs here and stuff.
		char* ptr = XP_StripLine(m_defaultDomain);

		while (*ptr == '@') ptr++;

		if (*ptr == '\0' || XP_STRCHR(ptr, '@')) 
		{
			XP_FREEIF (m_defaultDomain);
			m_defaultDomain = NULL;
		}
		else 
			if (ptr != m_defaultDomain) 
			{
				char* tmp = XP_STRDUP(ptr);
				XP_FREEIF (m_defaultDomain);
				m_defaultDomain = tmp;
			}
	}

	if (!m_defaultDomain) 
	{
		// Doesn't seem to be an explicit domain
		// specified.  Use the one in the user's
		// e-mail address.
		const char* useraddr = FE_UsersMailAddress();
		if (useraddr) 
		{
			char* ptr = XP_STRCHR(useraddr, '@');
			if (ptr) 
			{
				ptr++;
				if (*ptr) 
					m_defaultDomain = XP_STRDUP(XP_StripLine(ptr));
			}
		}
	}

#endif
}
