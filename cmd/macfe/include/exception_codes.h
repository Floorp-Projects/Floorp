/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//////////////////////////////////////////////////////////////////////////
//
//	exception_codes.h
//
// 
//
//////////////////////////////////////////////////////////////////////////
//
//	This file contains several enums to be used as codes for exceptions.
//	This enums should be always synched with the resource file containing 
//	exception strings (i.e. for every enum there should be a corresponding
//	and appropiate string in the resource file ). The file containing the 
// 	exception string resources is "exception_str.r".
//
//	For easier usage every enum should be followed by a comment.
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// mail/news codes

#define	genericExStringsID  800 // resource containing the generic exception strings

enum GenericCode
{
	eDeafult = 1,				// generic default exception
	eUIResourceError = 2		// this error is thrown when either a UI resource is missing
								// or is being casted to the wrong type. The user should 
								// never see this.



};

//////////////////////////////////////////////////////////////////////////
// browser codes

#define	browserExStringsID 	802 // resource containing the mail exception strings

enum BrowserCode
{
	eBowserDeafult = 1			// default exception for the browser





};

//////////////////////////////////////////////////////////////////////////
// mail/news codes

#define mailExStringsID		801 // resource containing the browser exception strings

enum MailCode
{
	eMailDeafult = 1,			// default exception for news/mail
	eMissingPreference = 2,		// missing preference
	eMoveMessageError = 1		// BE error while moving a message. Currently
								// I am redefining as a default message. This might change




};
