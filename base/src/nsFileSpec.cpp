/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
#include "nsFileSpec.h"

#include "prtypes.h"

#if DEBUG
#include <ostream>
#endif

#include <strstream>

//========================================================================================
namespace nsFileSpecHelpers
//========================================================================================
{
	enum
	{	kMaxFilenameLength = 31				// should work on Macintosh, Unix, and Win32.
	,	kMaxAltDigitLength	= 5
	,	kMaxAltNameLength	= (kMaxFilenameLength - (kMaxAltDigitLength + 1))
	};
	void LeafReplace(
		std::string& ioPath,
		char inSeparator,
		const std::string& inLeafName);
	std::string GetLeaf(const std::string& inPath, char inSeparator);
}

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::LeafReplace(
	std::string& ioPath,
	char inSeparator,
	const std::string& inLeafName)
//----------------------------------------------------------------------------------------
{
	// Find the existing leaf name
	std::string::size_type lastSeparator = ioPath.rfind(inSeparator);
	std::string::size_type myLength = ioPath.length();
	if (lastSeparator < myLength)
		ioPath = ioPath.substr(0, lastSeparator + 1) + inLeafName;
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
std::string nsFileSpecHelpers::GetLeaf(const std::string& inPath, char inSeparator)
//----------------------------------------------------------------------------------------
{
	std::string::size_type lastSeparator = inPath.rfind(inSeparator);
	std::string::size_type myLength = inPath.length();
	if (lastSeparator < myLength)
		return inPath.substr(1 + lastSeparator, myLength - lastSeparator - 1);
	return inPath;
} // nsNativeFileSpec::GetLeafName


#ifdef XP_MAC
#include "nsFileSpecMac.cpp" // Macintosh-specific implementations
#elif defined(XP_WIN)
#include "nsFileSpecWin.cpp" // Windows-specific implementations
#elif defined(XP_UNIX)
#include "nsFileSpecUnix.cpp" // Unix-specific implementations
#endif


//========================================================================================
//								nsFileURL implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const std::string& inString)
//----------------------------------------------------------------------------------------
:	mURL(inString)
#ifdef XP_MAC
,	mNativeFileSpec(inString.substr(kFileURLPrefixLength, inString.length() - kFileURLPrefixLength))
#endif
{
	NS_ASSERTION(mURL.substr(0, kFileURLPrefixLength) == kFileURLPrefix, "Not a URL!");
}

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const std::string& inString)
//----------------------------------------------------------------------------------------
{
	mURL = inString;
	NS_ASSERTION(mURL.substr(0, kFileURLPrefixLength) == kFileURLPrefix, "Not a URL!");
#ifdef XP_MAC
	mNativeFileSpec = 
		inString.substr(kFileURLPrefixLength, inString.length() - kFileURLPrefixLength);
#endif
}

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:	mURL(inOther.mURL)
#ifdef XP_MAC
,	mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
}

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = inOther.mURL;
#ifdef XP_MAC
	mNativeFileSpec = inOther.GetNativeSpec();
#endif
}

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsUnixFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + ((string&)inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther.GetNativeSpec();
#endif
}
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsUnixFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + ((string&)inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther.GetNativeSpec();
#endif
}

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + (std::string&)nsUnixFilePath(inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther;
#endif
}
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix +  (std::string&)nsUnixFilePath(inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther;
#endif
}

//========================================================================================
//								nsUnixFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsUnixFilePath::nsUnixFilePath(const std::string& inString)
//----------------------------------------------------------------------------------------
:	mPath(inString)
#ifdef XP_MAC
,	mNativeFileSpec(inString)
#endif
{
	NS_ASSERTION(mPath.substr(0, kFileURLPrefixLength) != kFileURLPrefix, "URL passed as path");
}

//----------------------------------------------------------------------------------------
nsUnixFilePath::nsUnixFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:	mPath(((string&)inOther).substr(
			kFileURLPrefixLength, ((string&)inOther).length() - kFileURLPrefixLength))
#ifdef XP_MAC
,	mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
}

//----------------------------------------------------------------------------------------
void nsUnixFilePath::operator = (const std::string& inString)
//----------------------------------------------------------------------------------------
{
	mPath = inString;
#ifdef XP_MAC
	mNativeFileSpec = inString;
#endif
	NS_ASSERTION(mPath.substr(0, kFileURLPrefixLength) != kFileURLPrefix, "URL passed as path");
}

//----------------------------------------------------------------------------------------
void nsUnixFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	mPath = ((string&)inOther).substr(
				kFileURLPrefixLength, ((string&)inOther).length() - kFileURLPrefixLength);
#ifdef XP_MAC
	mNativeFileSpec = inOther.GetNativeSpec();
#endif
}

//========================================================================================
//								nsNativeFileSpec implementation
//========================================================================================

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec()
//----------------------------------------------------------------------------------------
{
}
#endif

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeUnique(const std::string& inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
	if (inSuggestedLeafName.length() > 0)
		SetLeafName(inSuggestedLeafName);

	MakeUnique();
} // nsNativeFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeUnique()
//----------------------------------------------------------------------------------------
{
	if (!Exists())
		return;

	short index = 0;
	std::string altName = GetLeafName();
	std::string::size_type lastDot = altName.rfind('.');
	std::string suffix;
	if (lastDot < altName.length())
	{
		suffix = altName.substr(lastDot, altName.length() - lastDot); // include '.'
		altName = altName.substr(0, lastDot);
	}
	const std::string::size_type kMaxRootLength
		= nsFileSpecHelpers::kMaxAltNameLength - suffix.length() - 1;
	if (altName.length() > kMaxRootLength)
		altName = altName.substr(0, kMaxRootLength);
	while (Exists())
	{
		// start with "Picture-2.jpg" after "Picture.jpg" exists
		if ( ++index > 999 )		// something's very wrong
			return;
		char buf[nsFileSpecHelpers::kMaxFilenameLength + 1];
		ostrstream newName(buf, nsFileSpecHelpers::kMaxFilenameLength);
		newName << altName.c_str() << "-" << index << suffix.c_str() << ends;
		SetLeafName(newName.str()); // or: SetLeafName(buf)
	}
} // nsNativeFileSpec::MakeUnique
