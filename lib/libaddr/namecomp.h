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

/***************************************************************************************************************
  The name completion cookie is an object which stores a name completion result. There are APIs which allow the 
  FEs to ask the name completion cookie for the different types of strings: NCString (the blurr string), the display
  string (in the address field of the compose pane) and the header string (expands mailing lists, makes sure name
  is RFC822) for mailing the message.


  Typically, this object is created by the back end in response to a name completion call by the front end. This 
  object is then returned to the FE who is responsible for using an API for freeing the cookie when they are done 
  with it. 

  A name completion cookie is generated with a container and entryID. At this time, the container is not ref-counted
  because we do not keep it around! We perform all name resolution at the time of creation! People may not like
  this as it means we expand mailing lists at the time of the name completion and not at the time of sending the 
  message. So this may change. But for now...it isn't ref counted.

  CORRECTION: In order to support name completion for the mailing list pane, the cookie will have a ref counted copy 
  of the ctr and ABID. closing the cookie will cause the ctr to be released.

*****************************************************************************************************************/


#ifndef _NAMECOMP_H_
#define _NAMECOMP_H_

#include "abcom.h"
#include "abcinfo.h"

class AB_NameCompletionCookie
{
public:
	AB_NameCompletionCookie (MSG_Pane * srcPane, AB_ContainerInfo * srcContainer, ABID entryID);
	AB_NameCompletionCookie (const char * nakedAddress); // you can also create one given a naked address

	void CloseSelf();
	static int Close(AB_NameCompletionCookie * cookie);

	// string getters
	char * GetNameCompletionDisplayString();  // caller must free return value
	char * GetHeaderString();
	char * GetExpandedHeaderString();
	AB_EntryType GetEntryType() { return m_entryType;}

	// source getters
	AB_ContainerInfo * GetEntryContainer();
	ABID GetEntryABID();

	const char * GetNakedAddress(); 
	
protected:
	virtual ~AB_NameCompletionCookie ();

	AB_ContainerInfo * m_container;
	ABID m_entryID; 
	AB_EntryType m_entryType;
	char * m_nakedAddress;


	void InitializeStrings();
	void LoadStrings(MSG_Pane * srcPane, AB_ContainerInfo * ctr, ABID entryID);
	void LoadStrings(const char * nakedAddredd);
	void LoadExpandedHeaderString(const char * nakedAddress);

	char * m_displayString;      // the blurr string which appears in UI-less name completion. could include extra attributes
							     // set via a pref. i.e. "John Doe <johndoe@netscape.com> Client Engineering"
	char * m_headerString;		 // i.e. "John Doe <johndoe@netscape.com>" or "JohnsMailingList". 
	char * m_expandedHdrString;  // mailing lists are expanded into component names. all RFC822 addresses
};

#endif
