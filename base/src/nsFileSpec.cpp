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
#include <ostream.h>
#endif

#include <strstream.h>

//========================================================================================
NS_NAMESPACE nsFileSpecHelpers
//========================================================================================
{
	enum
	{	kMaxFilenameLength = 31				// should work on Macintosh, Unix, and Win32.
	,	kMaxAltDigitLength	= 5
	,	kMaxAltNameLength	= (kMaxFilenameLength - (kMaxAltDigitLength + 1))
	};
	NS_NAMESPACE_PROTOTYPE void LeafReplace(
		string& ioPath,
		char inSeparator,
		const string& inLeafName);
	NS_NAMESPACE_PROTOTYPE string GetLeaf(const string& inPath, char inSeparator);
} NS_NAMESPACE_END

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::LeafReplace(
	string& ioPath,
	char inSeparator,
	const string& inLeafName)
//----------------------------------------------------------------------------------------
{
	// Find the existing leaf name
	string::size_type lastSeparator = ioPath.rfind(inSeparator);
	string::size_type myLength = ioPath.length();
	if (lastSeparator < myLength)
		ioPath = ioPath.substr(0, lastSeparator + 1) + inLeafName;
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
string nsFileSpecHelpers::GetLeaf(const string& inPath, char inSeparator)
//----------------------------------------------------------------------------------------
{
	string::size_type lastSeparator = inPath.rfind(inSeparator);
	string::size_type myLength = inPath.length();
	if (lastSeparator < myLength)
		return inPath.substr(1 + lastSeparator, myLength - lastSeparator - 1);
	return inPath;
} // nsNativeFileSpec::GetLeafName


#ifdef XP_MAC
#include "nsFileSpecMac.cpp" // Macintosh-specific implementations
#elif defined(XP_PC)
#include "nsFileSpecWin.cpp" // Windows-specific implementations
#elif defined(XP_UNIX)
#include "nsFileSpecUnix.cpp" // Unix-specific implementations
#endif

//========================================================================================
//								nsFileURL implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const string& inString)
//----------------------------------------------------------------------------------------
:	mURL(inString)
#ifdef XP_MAC
,	mNativeFileSpec(inString.substr(kFileURLPrefixLength, inString.length() - kFileURLPrefixLength))
#endif
{
	NS_ASSERTION(mURL.substr(0, kFileURLPrefixLength) == kFileURLPrefix, "Not a URL!");
}

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const string& inString)
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
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + ((std::string&)inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther.GetNativeSpec();
#endif
}
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + ((std::string&)inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther.GetNativeSpec();
#endif
}

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix + (string&)nsFilePath(inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther;
#endif
}
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mURL = kFileURLPrefix +  (string&)nsFilePath(inOther);
#ifdef XP_MAC
	mNativeFileSpec  = inOther;
#endif
}

//========================================================================================
//								nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const string& inString)
//----------------------------------------------------------------------------------------
:	mPath(inString)
#ifdef XP_MAC
,	mNativeFileSpec(inString)
#endif
{
	NS_ASSERTION(mPath.substr(0, kFileURLPrefixLength) != kFileURLPrefix, "URL passed as path");
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:	mPath(((std::string&)inOther).substr(
			kFileURLPrefixLength, ((std::string&)inOther).length() - kFileURLPrefixLength))
#ifdef XP_MAC
,	mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const string& inString)
//----------------------------------------------------------------------------------------
{
	mPath = inString;
#ifdef XP_MAC
	mNativeFileSpec = inString;
#endif
	NS_ASSERTION(mPath.substr(0, kFileURLPrefixLength) != kFileURLPrefix, "URL passed as path");
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	mPath = ((std::string&)inOther).substr(
				kFileURLPrefixLength, ((std::string&)inOther).length() - kFileURLPrefixLength);
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
void nsNativeFileSpec::MakeUnique(const string& inSuggestedLeafName)
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
	string altName = GetLeafName();
	string::size_type lastDot = altName.rfind('.');
	string suffix;
	if (lastDot < altName.length())
	{
		suffix = altName.substr(lastDot, altName.length() - lastDot); // include '.'
		altName = altName.substr(0, lastDot);
	}
	const string::size_type kMaxRootLength
		= nsFileSpecHelpers::kMaxAltNameLength - suffix.length() - 1;
	if (altName.length() > kMaxRootLength)
		altName = altName.substr(0, kMaxRootLength);
	while (Exists())
	{
		// start with "Picture-2.jpg" after "Picture.jpg" exists
		if ( ++index > 999 )		// something's very wrong
			return;
		char buf[nsFileSpecHelpers::kMaxFilenameLength + 1];
		std::ostrstream newName(buf, nsFileSpecHelpers::kMaxFilenameLength);
		newName << altName.c_str() << "-" << index << suffix.c_str() << std::ends;
		SetLeafName(newName.str()); // or: SetLeafName(buf)
	}
} // nsNativeFileSpec::MakeUnique
