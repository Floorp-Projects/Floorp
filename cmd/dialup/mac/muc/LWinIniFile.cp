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

#include "LWinIniFile.h"
#include "ctype.h"

LWinIniFile::LWinIniFile( FSSpec& inFileSpec ):
	LTextFile( inFileSpec )
{
	mSectionPosition = 0;
}

LWinIniFile::LWinIniFile(): LTextFile()
{
	mSectionPosition = 0;
}

static void RemovePreceedingWhiteSpace( LStr255& string )
{
	while ( string.Length() && isspace( string[ 1 ] ) )
		string.Remove( 1, 1 );
}	
	
static void RemoveBracketDelimiters( LStr255& string )
{
	// ¥ get rid of '['
	if ( string[ 1 ] == '[' )
		string.Remove( 1, 1 );
	
	// ¥Êget rid of ']'
	if ( string.EndsWith( ']' ) )
		string.Remove( string.Length(), 1 );
}

ExceptionCode LWinIniFile::FindSection( const LStr255& inSectionName )
{
	if ( mDataForkRefNum == refNum_Undefined )
		this->OpenDataFork( fsRdPerm );
	
	this->SetMarker( 0, streamFrom_Start );
	
	LStr255		buffer;
	LStr255		sectionName = inSectionName;
	
	while ( !this->AtEnd() )
	{
		Int32			bytesToRead = 255;
		ExceptionCode	err;
				
		err = this->ReadLine( &buffer[ 1 ], bytesToRead );
		
		if ( err != noErr && err != eofErr )
			return err;
		
		buffer[ 0 ] = bytesToRead - 1;
	
		if ( !bytesToRead || buffer[ 1 ] != '[' )
			continue;
		
		RemoveBracketDelimiters( buffer );
		RemoveBracketDelimiters( sectionName );
		if ( buffer.CompareTo( sectionName ) == 0 )
		{
			mSectionPosition = this->GetMarker();
			return noErr;
		}
	}

	buffer[ 0 ] = 0;
	mSectionPosition = 0;
	return err_NotFound;
}


ExceptionCode LWinIniFile::GetNextNameValuePair( LStr255& pName, LStr255& pValue )
{
	pValue[ 0 ] = 0;
	
	LStr255			pStr;
	ExceptionCode	err;
	Int32			byteCount = 255;
	
	err = this->ReadLine( &pStr[ 1 ], byteCount );

	if ( err != noErr )
		return err;

	pStr[ 0 ] = byteCount - 1;
	
	RemovePreceedingWhiteSpace( pStr );
	if ( pStr[ 0 ] == 0  )
		return err_NotFound;
		
	if ( pStr[ 1 ] == '[' )
		return err_NotFound;
				
	if ( pStr.Length() )
	{
		long index = 1;

		while ( 	( pStr[ index ] != '=' ) &&
					( index <= pStr.Length() ) )
			index++;
		
		if ( index > pStr.Length() )
			return err_NotFound;
		
		while (		( pStr[ index ] == '=' ) || 
					isspace( pStr[ index ] ) &&
					( index > 1 ) )
			index--;
			
		long	nameLen	= index;
		pName.Assign( pStr, 1, nameLen );

		index++;

		while ( 	( index <= pStr.Length() ) &&
					( isspace( pStr[ index ] ) ||
					( pStr[ index ] == '=' ) ) )
			index++;
				
		long	valuePos = index;
		long	valueLen = pStr.Length() - valuePos + 1;

		while ( valueLen && isspace( pStr[ valuePos + valueLen - 1 ] ) )
			valueLen--;
				
		if ( valueLen <= 0 )
			return err_NoValue;
				
		pValue.Assign( pStr, valuePos, valueLen );
	}	
	return noErr;
}

ExceptionCode LWinIniFile::GetValueForName( const LStr255& pName, LStr255& pValue )
{
	LStr255			nextName;
	LStr255			nextValue;
	ExceptionCode	err = noErr;

	pValue[ 0 ] = 0;
	
	if ( pName.Length() == 0 )
		return err_NotFound;
			
	this->SetMarker( mSectionPosition, streamFrom_Start );
	while ( err != eofErr )
	{
	  	err = this->GetNextNameValuePair( nextName, nextValue );
		if ( err == noErr )
		{
		  	if ( nextName == pName )
			{
	  			if ( nextValue.Length() )
	  			{
	  				unsigned short		pos;
	  				const LStr255		semiColon( ";" );
	  				pos = nextValue.Find( semiColon );
	  				if ( pos )
	  					nextValue.Remove( pos, nextValue.Length() - pos );
	  				pValue = nextValue;
				}
				return noErr;
			}
		}
	}
	return err_NotFound;
}

Int16 LWinIniFile::OpenDataFork( Int16 inPermissions )
{
	mSectionPosition = 0;
	return LTextFile::OpenDataFork( inPermissions );
}

void LWinIniFile::CloseDataFork()
{
	mSectionPosition = 0;
	LTextFile::CloseDataFork();
}
