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

// InternetConfig.cp
//
// Created by Tim Craycroft, 2/9/96
//

#include "InternetConfig.h"

#include <ICAPI.h>
#include <ICKeys.h>

#include <LListener.h>
#include <LPeriodical.h>

#include "xp_core.h"
#include "xp_str.h"

#include "uprefd.h"
#include "prefapi.h"		// ns/modules/libpref
#include "resgui.h"

//
// InternetConfig class.
//
// Checks the IC seed on resume events
//
class CInternetConfig
{
	public:

		static void	Connect();
		static void	Disconnect();
		
		static	ICError	GetInternetConfigString(ConstStr255Param	icKey,
												Str255				s,
												long				*port = nil);

		static	ICError	GetInternetConfigFileMapping(	OSType fileType, 
														OSType creator, 
														ConstStr255Param fileName,
														ICMapEntry *ent);

		static	ICError	LaunchInternetConfigApplication(ConstStr255Param key);

#ifndef MOZ_MAIL_NEWS
		static	ICError	SendInternetConfigURL(char *address);
#endif

		static int		UseICPrefCallback(const char *prefString, void *);
		static void		ResumeEvent();

		static	Boolean	CurrentlyUsingIC();
		
		static	Boolean	HaveICInstance();

	private:
		
		CInternetConfig();
		~CInternetConfig();

		void	SynchIf();	// iff the seed has changed
		void	SynchFromIC();
		ICError	SynchStringFromIC(	ConstStr255Param	icKey,
									CPrefs::PrefEnum	netscapePref,
									Boolean				stripPort = false,
									const char			*xpPortPrefName = nil,
									int32				defaultPort = 0);
									
		ICError	SynchSplitStringFromIC(	ConstStr255Param	icKey,
										char 				divider,
										CPrefs::PrefEnum	firstString,
										CPrefs::PrefEnum	secondString,
										Boolean				stripPort = false);

		ICError	GetICString(	ConstStr255Param	icKey,
								Str255				s,
								int32				*port = nil);

		ICError GetICFileMapping(	OSType fileType, 
									OSType creator, 
									ConstStr255Param fileName,
									ICMapEntry *ent);

		ICError	LaunchICApplication(ConstStr255Param key);
									
#ifndef MOZ_MAIL_NEWS
		ICError SendICURL(char *address);
#endif
		ICInstance	fInstance;
		SInt32		fSeed;

		static 	CInternetConfig*	sInternetConfigConnection;
};


void
CInternetConfigInterface::ResumeEvent()
{
	CInternetConfig::ResumeEvent();
}

//
// ConnectToInternetConfig
//
// Only public entrypoint to this InternetConfig module.
//
// gets the folder from CPrefs::GetFilePrototype(prefSpec(, MainFolder)
// 
void
CInternetConfigInterface::ConnectToInternetConfig()
{
	// I assume that this is only called once, at startup.
	const char	*useICPrefName = "browser.mac.use_internet_config";
	PREF_RegisterCallback(useICPrefName, CInternetConfig::UseICPrefCallback, nil);
	try
	{
		if (CInternetConfig::CurrentlyUsingIC())
		{
			CInternetConfig::Connect();
		}
	}
	catch(ICError err)
	{
		// do something ?	a dialog perhaps ?
		// only if there is a real problem, not if 
		// IC just isn't installed.
	}
}	

void	
CInternetConfigInterface::DisconnectFromInternetConfig()
{
	CInternetConfig::Disconnect();
}

Boolean
CInternetConfigInterface::CurrentlyUsingIC(void)
{
	Boolean returnValue = TRUE;
	returnValue = returnValue && CInternetConfig::CurrentlyUsingIC();
	returnValue = returnValue && CInternetConfig::HaveICInstance();
	
	return returnValue;
}

void
CInternetConfigInterface::GetInternetConfigString(	ConstStr255Param icKey,
													Str255 s,
													long *port)
{
	if (CInternetConfig::GetInternetConfigString(icKey, s, port))
	{
		s[0] = 0;
	}
}

ICError
CInternetConfigInterface::GetInternetConfigFileMapping(	OSType fileType, 
														OSType creator, 
														ConstStr255Param fileName,
														ICMapEntry *ent)
{
	return CInternetConfig::GetInternetConfigFileMapping(fileType, creator,
															fileName, ent);
}

#ifndef MOZ_MAIL_NEWS
ICError
CInternetConfigInterface::SendInternetConfigURL(char *address)
{
	return CInternetConfig::SendInternetConfigURL(address);
}
#endif

ICError
CInternetConfigInterface::LaunchInternetConfigApplication(ConstStr255Param key)
{
	return CInternetConfig::LaunchInternetConfigApplication(key);
}


CInternetConfig*
CInternetConfig::sInternetConfigConnection = nil;

Boolean
CInternetConfig::CurrentlyUsingIC()
{
	
	XP_Bool	result;
	const char	*useICPrefName = "browser.mac.use_internet_config";
	if (PREF_NOERROR != PREF_GetBoolPref(useICPrefName, &result))
	{
		result = false;
	}
	return (Boolean)result;
}


//
// CInternetConfig::Connect
//
// Call once to hook up with IC.  
//
void
CInternetConfig::Connect()
{
	if (!sInternetConfigConnection)
	{
		sInternetConfigConnection = new CInternetConfig();	
	}
}

//
// Bail
//
void
CInternetConfig::Disconnect()
{
	if (sInternetConfigConnection != nil)
	{ 
		delete sInternetConfigConnection;
		sInternetConfigConnection = nil;
	}
}

Boolean
CInternetConfig::HaveICInstance(void)
{
	Boolean returnValue = FALSE;
	
	Connect();
	if (sInternetConfigConnection != nil)
	{
		returnValue = (sInternetConfigConnection->fInstance != NULL);
	}
	
	return returnValue;
}

//
// CInternetConfig::CInternetConfig
//
CInternetConfig::CInternetConfig():
fInstance(NULL)
{	
	ICError	err;
		
	// Detect IC, if present
	StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
	err = ::ICStart(&fInstance, emSignature);
	//ThrowIfOSErr_(err);
	if (!err)
	{
		try
		{	

			ICDirSpec			prefDir[1];
			ICDirSpecArrayPtr	prefDirArray;
			UInt32				dirCount = 0;
			
			// what a wonderful api...
			//
			// Give IC the directory that contains the pref file we're using
			// so it can look there for an IC config file.
			prefDirArray = (ICDirSpecArrayPtr) &prefDir;
			FSSpec	prefSpec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
			prefDir[0].vRefNum 	= prefSpec.vRefNum;
			prefDir[0].dirID	= prefSpec.parID;
			dirCount 			= 1;	
			
			err = ::ICFindConfigFile(fInstance, dirCount, prefDirArray);
			ThrowIfOSErr_(err);

			// Remember initial seed
			err = ::ICGetSeed(fInstance, &fSeed);
			ThrowIfOSErr_(err);
			
			// Read prefs from IC
			if (CurrentlyUsingIC())
			{
				SynchFromIC();
			}
		}
		catch(ICError err)
		{
			// Close IC connection and pass the error along
			::ICStop(fInstance);
			fInstance = NULL;
		//	throw(err);
			// we probably out to delete "this" as well
		}
	}
}

int
CInternetConfig::UseICPrefCallback(const char *, void *)
{
	if (CInternetConfig::CurrentlyUsingIC())
	{
		Connect();
		sInternetConfigConnection->SynchFromIC();
	}
	return 0;	// You don't even want to know my opinion of this!
}


//
// CInternetConfig::~CInternetConfig
//
CInternetConfig::~CInternetConfig()
{
	if (fInstance != NULL)
	{
		::ICStop(fInstance);		// close IC connection
	}
}

//
// CInternetConfig::SynchFromIC
//
// Reads IC settings and converts them to Netscape prefs
//
void
CInternetConfig::SynchFromIC()
{
	ICError	err;
	
	err = ::ICBegin(fInstance, icReadOnlyPerm);
	ThrowIfOSErr_(err);
	
	// Again, this is lame.
	//
	// We should have a table of some sort
	//
	SynchStringFromIC(kICRealName, 		CPrefs::UserName);
	SynchStringFromIC(kICEmail, 		CPrefs::UserEmail);	
	SynchStringFromIC(kICEmail, 		CPrefs::ReplyTo);	// IC has no reply-to	
	SynchSplitStringFromIC(kICMailAccount, '@', CPrefs::PopID, CPrefs::PopHost, true);	
	SynchStringFromIC(kICSMTPHost, 		CPrefs::SMTPHost, true);	
	SynchStringFromIC(kICWWWHomePage,	CPrefs::HomePage);
	SynchStringFromIC(kICOrganization, 	CPrefs::Organization);
	SynchStringFromIC(kICNNTPHost,		CPrefs::NewsHost, true, "news.server_port", 119);
	
	::ICEnd(fInstance);
}

void
CInternetConfig::SynchIf()
{
	SInt32	seed;

	if (::ICGetSeed(fInstance, &seed))
	{
		return;
	}
	if (seed != fSeed)
	{
		try
		{
			SynchFromIC();
		}
		catch(ICError err)
		{
		}
		fSeed = seed;
	}
}

//
// CInternetConfig::ResumeEvent
//
//
void
CInternetConfig::ResumeEvent()
{
	if (CurrentlyUsingIC())
	{
		Connect();
		sInternetConfigConnection->SynchIf();
	}
}

//
// CInternetConfig::SynchStringFromIC
//
// Set a netscape string from an IC string
ICError
CInternetConfig::SynchStringFromIC(	ConstStr255Param	icKey,
									CPrefs::PrefEnum	netscapePref,
									Boolean				stripPort,
									const char			*xpPortPrefName,
									int32				defaultPort)
{								
	char	s[256];
	ICError	err;
	
	int32	*portPtr = stripPort ? &defaultPort : nil;

	err = GetICString(icKey, (unsigned char*) s, portPtr);
	if (err == 0)
	{
		p2cstr((StringPtr)s);
		CPrefs::SetString(s, netscapePref);
		if (xpPortPrefName)
		{
			PREF_SetIntPref(xpPortPrefName, defaultPort);
		}
	}
	return err;
}		

//
// CInternetConfig::SynchSplitStringFromIC
//
// Takes a single IC string and splits it into two Netscape strings.
// Useful for machine@host.domain.com, or proxy.address:port type stuff
//
// If the divider can't be found, the entire string is put into the
// first netscape string and the second netscape string is set to '\0'
// 
ICError
CInternetConfig::SynchSplitStringFromIC(	ConstStr255Param	icKey,
											char 				divider,
											CPrefs::PrefEnum	firstString,
											CPrefs::PrefEnum	secondString,
											Boolean				stripPort)
{
	char	buffer[256];
	char	*s;
	char	*split;
	ICError	err;
	
	s 	= buffer;
	err = GetICString(icKey, (unsigned char *) s);
	if (err != 0) return err;
	
	p2cstr((StringPtr)s);
		
	split = strchr(s, divider);
	if (split != NULL)
	{
		*split = '\0';
		if (stripPort)
		{
			char *colon = strchr(split+1, ':');
			if (colon)
			{
				*colon = '\0';
			}
		}
		CPrefs::SetString(split+1, secondString);
	}	
	else
	{
		CPrefs::SetString('\0', secondString);
	}	
	CPrefs::SetString(s, firstString);	
	
	return 0;
}			
					
//
// CInternetConfig::GetICString
//
// Gets an IC string pref
//
ICError
CInternetConfig::GetInternetConfigString(	ConstStr255Param	icKey,
											Str255				s,
											long				*port)
{
	Connect();
	return sInternetConfigConnection->GetICString(icKey, s, port);
}								

//
// CInternetConfig::GetICString
//
// Gets an IC string pref
//
ICError
CInternetConfig::GetICString(	ConstStr255Param	icKey,
								Str255				s,
								int32				*port)
{
	ICAttr attr;	
	long size = 256;
	ICError	result;
	result = ::ICGetPref(fInstance, icKey, &attr, (Ptr)s, &size);
	if (!result)
	{
		if (port)
		{
			char	cString[256];
			BlockMoveData(&s[1], cString, s[0]);
			cString[s[0]] = '\0';
			char *colon = strchr(cString, ':');
			if (colon)
			{
				*colon = '\0';
				s[0] = colon - cString;
				++colon;
				// IC supposedly supports notations like:
				//		news.netscape.com:nntp
				// The protocol services don't seem to work in IC (or I'm to stupid
				// to make them work), so we just check for this one value ("nntp")
				// because that is the only protocol for which we support port numbers.
				if (!XP_STRCASECMP("nntp", colon))
				{
					*port = 119;
				}
				else
				{
					// Add more protocols here if/when we suppor them.
					long	portFromICString;
					int numargs = sscanf(colon, "%ld", &portFromICString);
					if (1 == numargs)
					{
						if (portFromICString >= 0)	// negative port numbers are not valid
						{
							*port = portFromICString;
						}
					}
					// else we just use the default port
				}
			}
		}
	}
	return result;
}								

#ifndef MOZ_MAIL_NEWS
ICError
CInternetConfig::SendInternetConfigURL(char *address)
{
	Connect();
	return sInternetConfigConnection->SendICURL(address);
}

ICError
CInternetConfig::SendICURL(char *address)
{
	if (address == NULL)
		return icNoURLErr;
		
	long selStart = 0;
	long selEnd = strlen(address);

	if( CInternetConfig::HaveICInstance() )
		return ::ICLaunchURL(fInstance, "\p", address, selEnd, &selStart, &selEnd);
	else
		return icPrefNotFoundErr;
}
#endif

ICError
CInternetConfig::GetInternetConfigFileMapping(	OSType fileType,
												OSType creator,
												ConstStr255Param filename,
												ICMapEntry *ent)
{
	Connect();
	return sInternetConfigConnection->GetICFileMapping(fileType, creator, filename, ent);
}

ICError
CInternetConfig::GetICFileMapping(	OSType fileType,
									OSType creator,
									ConstStr255Param filename,
									ICMapEntry *ent)
{
	if( CInternetConfig::HaveICInstance() )
		return ::ICMapTypeCreator(fInstance, fileType, creator, filename, ent);
	else
		return icPrefNotFoundErr;
}

ICError
CInternetConfig::LaunchInternetConfigApplication(ConstStr255Param key)
{
	Connect();
	return sInternetConfigConnection->LaunchICApplication(key);
}

ICError
CInternetConfig::LaunchICApplication(ConstStr255Param key)
{
	if (CInternetConfig::HaveICInstance())
		return ::ICEditPreferences(fInstance, key);
	else
		return icPrefNotFoundErr;
}
