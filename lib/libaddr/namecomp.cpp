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

#include "namecomp.h"
#include "addrprsr.h"
#include "abcinfo.h"

AB_NameCompletionCookie::AB_NameCompletionCookie(MSG_Pane * srcPane, AB_ContainerInfo * srcCtr, ABID entryID)
{
	InitializeStrings();
	m_entryID = entryID;
	m_entryType = AB_Person; // give us some value to initialize by..
	if (srcCtr)
	{
		m_container = srcCtr;
		m_container->Acquire();

		// fetch the entry type
		AB_AttributeValue * value = NULL;
		AB_AttribID attrib = AB_attribEntryType;
		uint16 numItems = 1;
		if (m_entryID != AB_ABIDUNKNOWN)
			m_container->GetEntryAttributes(srcPane, entryID, &attrib, &value, &numItems);
		if (value && numItems)
			m_entryType = value->u.entryType;
		AB_FreeEntryAttributeValue(value);
	}

	LoadStrings(srcPane, srcCtr, entryID);
}

AB_NameCompletionCookie::AB_NameCompletionCookie (const char * nakedAddress) // you can also create one given a naked address
{
	m_container = NULL;
	m_entryID = AB_ABIDUNKNOWN;
	m_entryType = AB_NakedAddress;
	InitializeStrings();
	LoadStrings(nakedAddress);
}

void AB_NameCompletionCookie::InitializeStrings()
{
	m_displayString = NULL;
	m_headerString = NULL;
	m_expandedHdrString = NULL;
	m_nakedAddress = NULL;
}


AB_NameCompletionCookie::~AB_NameCompletionCookie()
{
	XP_ASSERT(!m_container);
	XP_ASSERT(!m_displayString);
	XP_ASSERT(!m_headerString);
	XP_ASSERT(!m_expandedHdrString);
	XP_ASSERT(!m_nakedAddress);
}

void AB_NameCompletionCookie::CloseSelf()
{
	if (m_container)
	{
		m_container->Release();
		m_container = NULL;
	}

	if (m_displayString)
		XP_FREE(m_displayString);
	if (m_headerString)
		XP_FREE(m_headerString);
	if (m_expandedHdrString)
		XP_FREE(m_expandedHdrString);
	if (m_nakedAddress)
		XP_FREE(m_nakedAddress);

	m_displayString = NULL;
	m_headerString = NULL;
	m_expandedHdrString = NULL;
	m_nakedAddress = NULL;
}

/* static */ int AB_NameCompletionCookie::Close(AB_NameCompletionCookie * cookie)
{
	if (cookie)
	{
		cookie->CloseSelf();
		delete cookie;
	}

	return AB_SUCCESS;
}

void AB_NameCompletionCookie::LoadStrings(const char * nakedAddress)
{
	if (m_nakedAddress)
		XP_FREE(m_nakedAddress);
	
	if (m_headerString)
		XP_FREE(m_headerString);
	if (m_displayString)
		XP_FREE(m_displayString);
	if (m_expandedHdrString)
		XP_FREE(m_expandedHdrString);

	if (nakedAddress)
	{
		// hack...i already wrote the expanded header string to be parsed and use the default domain...
		// now folks want the header string to show it too...so I'm going to create the expanded hdr string
		// for the naked address then use it for the header string.
		LoadExpandedHeaderString(nakedAddress);
		if (m_expandedHdrString)
			m_headerString = XP_STRDUP(m_expandedHdrString);
		
		m_nakedAddress		= XP_STRDUP(nakedAddress);
		m_displayString     = XP_STRDUP(nakedAddress);  // leave display string without the default name...

	}
}

void AB_NameCompletionCookie::LoadExpandedHeaderString(const char * nakedAddress)
{
	AB_AddressParser * parser = NULL;
	AB_AddressParser::Create (NULL, &parser);
	if (parser)
	{
		parser->FormatNakedAddress(&m_expandedHdrString, nakedAddress,TRUE);
		parser->Release();
		parser = NULL;
	}
}

void AB_NameCompletionCookie::LoadStrings(MSG_Pane * srcPane, AB_ContainerInfo * srcContainer, ABID entryID)
{
	// for right now..nice and simple...simply get the full address for the entry by
	// calling the database...

	if (m_headerString)
		XP_FREE(m_headerString);
	if (m_displayString)
		XP_FREE(m_displayString);
	if (m_expandedHdrString)
		XP_FREE(m_expandedHdrString);
	
	if (srcContainer)
	{
		m_displayString = srcContainer->GetFullAddress(srcPane, entryID);  // gets RFC822 compliant address
		
		// for now, header string will be the same thing!
		if (m_displayString)
			m_headerString = XP_STRDUP(m_displayString);

		AB_ContainerInfo::GetExpandedHeaderForEntry(&m_expandedHdrString, srcPane, srcContainer, entryID);
	}
}

char * AB_NameCompletionCookie::GetNameCompletionDisplayString()
{
	if (m_displayString)
		return XP_STRDUP(m_displayString);
	else
		return NULL;
}

char * AB_NameCompletionCookie::GetHeaderString()
{
	if (m_headerString)
		return XP_STRDUP(m_headerString);
	else
		return NULL;
}

char * AB_NameCompletionCookie::GetExpandedHeaderString()
{
	if (m_expandedHdrString)
		return XP_STRDUP(m_expandedHdrString);
	else
		return NULL;
}


AB_ContainerInfo * AB_NameCompletionCookie::GetEntryContainer()
{
	return m_container;
}
	
ABID AB_NameCompletionCookie::GetEntryABID()
{
	return m_entryID;
}

const char * AB_NameCompletionCookie::GetNakedAddress()
{
	return m_nakedAddress;
}
	
