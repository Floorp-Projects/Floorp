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



// nc_exception.h

#pragma once

//////////////////////////////////////////////////////////////////////////
//
//	This file contains several classes for exceptions support in Communicator. 
//	The classes can be used as they are or they can be further subclassed.
//
//	When using the supplied classes as they are, the user should check if
//	the current codes and strings, stored in "exception_codes.h" and 
//	"exception_str.r", suffice his/her needs. If they don't he/she should 
//	add the proper codes and strings to the two aforementioned files.
//
//	When the user needs to subclass the supplied classes, usually to supply
//	additional granularity, he/she should provide a constructor, a specific getter
//	method for the error code and override the GetStringResID, method in order 
//	to return the proper resource ID for the exception strings. He/she should
//	also add a new enum and string resource.
//
//	Only code declared in the proper enums in the file "exception_codes.h" should 
//	be used for exceptions. This will ensure compatibility and proper documentation.
//
//	By appropiatelly subclassing and keeping the policy to declare and maintain
//	suclass enum as error code we can be assured of consistent errors and 
//	proper documentation.
//
//////////////////////////////////////////////////////////////////////////
//
//	USAGE:
//
//	try{
//			foo();
//		}
//	catch( nc_exception_subclass& error )
//		{
//			// do something
//		}	
//	catch( nc_exception& error )
//		{
//			// do something
//		}	
//	catch( ... )
//		{
//			// do something
//		}	
//	...
//
//	foo() {
//			throw nc_exception_subclass( eSampleException );
//		  }
//
//////////////////////////////////////////////////////////////////////////
//
//	When		What
//	-----		-----
//
//	12/3/97		Genesis
//	1/6/97		Changed to reflect the usage of string resources for internationalization.
//
//////////////////////////////////////////////////////////////////////////

#pragma exceptions on			// make sure that exceptions are on

#include "exception_codes.h"	// file containing the exception codes
#include "reserr.h"

#pragma mark class nc_exception
//////////////////////////////////////////////////////////////////////////
//
// nc_exception
//
// Base class for all the exceptions.
//
// Methods:
//
// 	nc_exception( const int errorCode ) - constructor used to set the proper 
//		exception code
//
//	GetErrorCode() - getter that returns the error code
//
//	DisplaySimpleAlert() - methods that displays the default alert using the 
//		string from the default resource.
//
//////////////////////////////////////////////////////////////////////////

class nc_exception
{	
	public:	
								nc_exception( const GenericCode errorCode );
		virtual					~nc_exception();

		virtual void			DisplaySimpleAlert() const;
		
	protected:	

		const int 				GetErrorCode() const;
		void 					SetErrorCode( const GenericCode errorCode );
		void 					SetErrorCode( const int errorCode );
		
		virtual short			GetStringResID() const;

	private:		
								nc_exception();	// private - empty constructor is not allowed
								
		int						mErrorCode;

};

//////////////////////////////////////////////////////////////////////////
// nc_exception inlines

inline
nc_exception::nc_exception()
{
}

inline
nc_exception::~nc_exception()
{
}

inline
nc_exception::nc_exception( const GenericCode errorCode ) : 
	mErrorCode( (int) errorCode ) 
{
}

inline
short			
nc_exception::GetStringResID() const
{
	return genericExStringsID;
}

inline
void
nc_exception::DisplaySimpleAlert() const
{
	Str255		errorString, codeString;
	short 		stringResID = GetStringResID();
	
	::GetIndString( errorString, stringResID, mErrorCode );
	::NumToString( mErrorCode, codeString );
	::ParamText( errorString, codeString, nil, nil );
	::CautionAlert( ALRT_ErrorOccurred, nil );
}

inline
void
nc_exception::SetErrorCode( const int errorCode )
{
	mErrorCode = errorCode;
}

inline
const int
nc_exception::GetErrorCode() const
{
	return mErrorCode;
}

#pragma mark -
#pragma mark class mail_exception
//////////////////////////////////////////////////////////////////////////
// mail_exception
//
// subclass for the mail/news errors.
//
//////////////////////////////////////////////////////////////////////////

class mail_exception : public nc_exception 
{
		
public:

					mail_exception( const MailCode errorCode );
	const MailCode 	GetMailErrorCode() const;
	
protected:

		virtual short			GetStringResID() const;
private:

	mail_exception(); 					// private - empty constructor is not allowed

};

//////////////////////////////////////////////////////////////////////////
// mail_exception inlines

inline
mail_exception::mail_exception()
{
}

inline
mail_exception::mail_exception( const MailCode errorCode ) 
{
	SetErrorCode( (int) errorCode );
}

inline
const MailCode
mail_exception::GetMailErrorCode() const
{
	return ( ( MailCode ) GetErrorCode() );
}

inline
short
mail_exception::GetStringResID() const
{
	return mailExStringsID;
}


#pragma mark -
#pragma mark class browser_exception
//////////////////////////////////////////////////////////////////////////
// browser_exception
//
// subclass for the browser errors.
//
//////////////////////////////////////////////////////////////////////////

class browser_exception : public nc_exception 
{
		
public:

						browser_exception( const BrowserCode errorCode );
	const BrowserCode 	GetBrowserErrorCode() const;
	
protected:

		virtual short			GetStringResID() const;
private:
	
	browser_exception(); 					// private - empty constructor is not allowed

};

//////////////////////////////////////////////////////////////////////////
// browser_exception inlines

inline
browser_exception::browser_exception()
{
}

inline
browser_exception::browser_exception( const BrowserCode errorCode )
{
	SetErrorCode( (int) errorCode );
}

inline
const BrowserCode
browser_exception::GetBrowserErrorCode() const
{
	return ( ( BrowserCode ) GetErrorCode() );
}

inline
short
browser_exception::GetStringResID() const
{
	return browserExStringsID;
}

