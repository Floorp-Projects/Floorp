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
 
//	This file is included by nsFileSpec.cp, and includes the Windows-specific
//	implementations.

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath;
}

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	// Convert '/' to '\'
	std::string& str = (std::string&)inPath;
	for (std::string::size_type i = 0; i < str.length(); i++)
	{
		char c = str[i];
		if (c == '/')
			c = '\\';
		mPath.append(&c, 1);
	}
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::SetLeafName(const std::string& inLeafName)
//----------------------------------------------------------------------------------------
{
	nsFileHelpers::LeafReplace(mPath, '\\', inLeafName);
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
std::string nsNativeFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
	return nsFileHelpers::GetLeaf(mPath, '\\');
} // nsNativeFileSpec::GetLeafName

