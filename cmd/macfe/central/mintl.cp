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

#include "uprefd.h"
// macfe
#include "macutil.h"	// ConstrainTo
#include "resgui.h"		// FENC_RESTYPE, CSIDLIST_RESTYPE, cmd2csid_tbl_ResID
// Netscape
#ifndef _XP_H_
#include "xp_mcom.h"
#endif
#include "libi18n.h"	// INTL_SetUnicodeCSIDList
#include "xpassert.h"
/* Testing Code */
#include "csid.h"		// CS_MAC_ROMAN, CS_AUTO, CS_DEFAULT
#include "intlpriv.h"	// prototypes for FE_GetSingleByteTable and FE_FreeSingleByteTable
#include "PascalString.h"
#include "prefapi.h"

/******************************************************************************************
 * INTERNATIONAL STUFF
 * Better off in another file?
 ******************************************************************************************/

static const char* Pref_PropFont = ".prop_font";
static const char* Pref_PropSize = ".prop_size";
static const char* Pref_FixedFont = ".fixed_font";
static const char* Pref_FixedSize = ".fixed_size";

LArray*			CPrefs::fCharSetFonts = NULL;
CCharSet		CPrefs::fDefaultFont;

static int FontChanged(const char * prefname, void * stuff);
static void InitCheckScriptFont(Handle hndl);

static void ReadCharacterEncodingsHandle(Handle hndl, Boolean usePrevious)
{
	unsigned short		numberEncodings;
	ThrowIfResError_();
	::DetachResource( hndl );
	ThrowIfResError_();
	
	LHandleStream	stream( hndl );
				
	stream.ReadData( &numberEncodings, sizeof( unsigned short) );
	XP_ASSERT(numberEncodings>0);
	for(int i=0;i<numberEncodings;i++)
	{
		CStr31			EncodingName;
		CStr31			PropFont;
		CStr31			FixedFont; 
		unsigned short	PropFontSize;
		unsigned short	FixedFontSize;
		unsigned short	CSID;
		unsigned short  FallbackFontScriptID;
		unsigned short	TxtrButtonResID;
		unsigned short	TxtrTextFieldResID;	
		
		stream.ReadData( &EncodingName,	sizeof( CStr31) );
		stream.ReadData( &PropFont, 		sizeof( CStr31) );
		stream.ReadData( &FixedFont, 		sizeof( CStr31) );
		stream.ReadData( &PropFontSize, 	sizeof( unsigned short) );
		stream.ReadData( &FixedFontSize, 	sizeof( unsigned short) );
		stream.ReadData( &CSID, 			sizeof( unsigned short) );
		stream.ReadData( &FallbackFontScriptID, sizeof( unsigned short) );
		stream.ReadData( &TxtrButtonResID, sizeof( unsigned short) );
		stream.ReadData( &TxtrTextFieldResID, sizeof( unsigned short) );
		
		if ( CPrefs::SetFont( EncodingName, PropFont, FixedFont, PropFontSize, FixedFontSize,
						 CSID , FallbackFontScriptID, TxtrButtonResID, TxtrTextFieldResID ) )
		{
			// Convert CCharSet to 4.0 xp preferences
			CCharSet csFont;
			CPrefs::GetFont(CSID, &csFont);
			char key[256];
			XP_SPRINTF(key, "intl.font%d", CSID);
			
			if (!usePrevious) {
				PREF_SetDefaultCharPref(CPrefs::Concat(key, Pref_PropFont), csFont.fPropFont);
				PREF_SetDefaultCharPref(CPrefs::Concat(key, Pref_FixedFont), csFont.fFixedFont);
				PREF_SetDefaultIntPref(CPrefs::Concat(key, Pref_PropSize), csFont.fPropFontSize);
				PREF_SetDefaultIntPref(CPrefs::Concat(key, Pref_FixedSize), csFont.fFixedFontSize);
			}
			else {
				PREF_SetCharPref(CPrefs::Concat(key, Pref_PropFont), csFont.fPropFont);
				PREF_SetCharPref(CPrefs::Concat(key, Pref_FixedFont), csFont.fFixedFont);
				PREF_SetIntPref(CPrefs::Concat(key, Pref_PropSize), csFont.fPropFontSize);
				PREF_SetIntPref(CPrefs::Concat(key, Pref_FixedSize), csFont.fFixedFontSize);
			}
		}
	}
}

void CPrefs::ReadCharacterEncodings()
{
	Handle hndl;
	if (UseApplicationResFile())	// It will first try to read from application. Put them into our memory preference
	{
		hndl = ::Get1Resource( FENC_RESTYPE,FNEC_RESID );
		if (hndl)
		{
			if (!*hndl)
				::LoadResource(hndl);
			
			if (*hndl)
			{
				::HNoPurge(hndl);
				ReadCharacterEncodingsHandle(hndl, false);
				::HPurge(hndl);
			}
		}
	}
	if (sPrefFileVersion == 3 && UsePreferencesResFile())	// It then try to read from application. overwrite the memory preference we build above
	{
		hndl = ::Get1Resource( FENC_RESTYPE,FNEC_RESID );
		if (hndl)
		{
			if (!*hndl)
				::LoadResource(hndl);
			
			if (*hndl)
			{
				::HNoPurge(hndl);
				ReadCharacterEncodingsHandle(hndl, true);
				::HPurge(hndl);
			}
		}
	}
	// Convert 4.0 style preferences to CCharSet structure
	else if (sPrefFileVersion == 4)
	{
		CCharSet csFont;
		LArrayIterator iter( *fCharSetFonts );
		while ( iter.Next( &csFont ) ) {
			ReadXPFont(csFont.fCSID, &csFont);
		}
	}
	
	// Currently it is not customerizable, so we don't check PreferencesFile
	if (UseApplicationResFile())	
	{
		hndl = ::Get1Resource( CSIDLIST_RESTYPE, CSIDLIST_RESID );
		if (hndl)
		{
			if (!*hndl)
				::LoadResource(hndl);
			
			if (*hndl)
			{
				::HNoPurge(hndl);
				InitCheckScriptFont(hndl);
				::HPurge(hndl);
			}
		}
	}
	
	PREF_RegisterCallback( "intl.font", FontChanged, nil );
}

// 4.0: Font changes register the following callback which
// reflects xp pref changes into the CCharSet structure
int FontChanged(const char * prefname, void * /* stuff */)
{
	int csid;
	if (sscanf(prefname, "intl.font%d", &csid)) {
		CCharSet csFont;
		if (CPrefs::GetFont(csid, &csFont)) {
			CPrefs::ReadXPFont(csid, &csFont);
		}
	}
	return 0;
}

// Convert CCharSet to xp font pref
void CPrefs::ReadXPFont(int16 csid, CCharSet* csFont)
{
	char key[256];
	int len = 31;
	char propFont[31];
	char fixedFont[31];
	int32 propSize, fixedSize;
	
	XP_SPRINTF(key, "intl.font%d", csid);
	PREF_GetCharPref(Concat(key, Pref_PropFont), propFont, &len);
	PREF_GetCharPref(Concat(key, Pref_FixedFont), fixedFont, &len);
	PREF_GetIntPref(Concat(key, Pref_PropSize), &propSize);
	PREF_GetIntPref(Concat(key, Pref_FixedSize), &fixedSize);
	
	CPrefs::SetFont( csFont->fEncodingName, propFont, fixedFont, propSize, fixedSize,
				 csFont->fCSID , csFont->fFallbackFontScriptID, csFont->fTxtrButtonResID,
				 csFont->fTxtrTextFieldResID );
}

extern int16		cntxt_lastSize;

Boolean CPrefs::SetFont(	const CStr255& EncodingName,
							const CStr255& PropFont,
							const CStr255& FixedFont,
							unsigned short PropFontSize, 
							unsigned short FixedFontSize, 
							unsigned short CSID, 
							unsigned short FallbackFontScriptID, 
							unsigned short TxtrButtonResID, 
							unsigned short TxtrTextFieldResID )
{
	CCharSet			csFont;
	Boolean				changed = FALSE;
	long				realFixedSize;
	long				realPropSize;
	short				index;
	CStr255				systemFont;
	
	::GetFontName( 0, systemFont );
	if ( CPrefs::GetFont( CSID, &csFont ) )
		index = fCharSetFonts->FetchIndexOf( &csFont );
	else
	{
		csFont.fEncodingName = EncodingName;
		csFont.fCSID = CSID;
		csFont.fFallbackFontScriptID = FallbackFontScriptID;
		csFont.fTxtrButtonResID = TxtrButtonResID;
		csFont.fTxtrTextFieldResID = TxtrTextFieldResID;
			
		if ( CSID == CS_MAC_ROMAN )
			fCharSetFonts->InsertItemsAt( 1, LArray::index_First, &csFont );
		else
			fCharSetFonts->InsertItemsAt( 1, LArray::index_Last, &csFont );
		index = fCharSetFonts->FetchIndexOf( &csFont );
		changed = TRUE;
	}
	if ( index != LArray::index_Bad )
	{
		realFixedSize = FixedFontSize;
		realPropSize = PropFontSize;
		
		ConstrainTo( FONT_SIZE_MIN, FONT_SIZE_MAX, realFixedSize );
		ConstrainTo( FONT_SIZE_MIN, FONT_SIZE_MAX, realPropSize );
		
		if ( 	changed || 
				PropFont != csFont.fPropFont || realPropSize != csFont.fPropFontSize ||
				FixedFont != csFont.fFixedFont || realFixedSize != csFont.fFixedFontSize )
		{
		
			csFont.fPropFont = PropFont;
			csFont.fPropFontSize = realPropSize;
			csFont.fFixedFont = FixedFont;
			csFont.fFixedFontSize = realFixedSize;
				
			::GetFNum( PropFont, (short*)&csFont.fPropFontNum );
			if(csFont.fPropFontNum == 0 && !EqualString( PropFont, systemFont, FALSE, FALSE ) )
			{	
				
				// Do not have that font. Try to us smScriptAppFondSize from Script Manager
				long AppFondSize;
				unsigned short AppFond;
				CStr255 tryFontName;
				
				AppFondSize = ::GetScriptVariable(FallbackFontScriptID, smScriptAppFondSize);
				AppFond = (AppFondSize >> 16);
				::GetFontName(AppFond, tryFontName);
				if(tryFontName[0] != 0)	// The AppFond exist
				{
					csFont.fPropFontNum = AppFond;
					csFont.fPropFont = tryFontName;
				}
			}
			::GetFNum( FixedFont, (short*)&csFont.fFixedFontNum );
			if(csFont.fFixedFontNum == 0 && !EqualString( FixedFont, systemFont, FALSE, FALSE ) )
			{	
				// Do not have that font. Try to us smScriptAppFondSize from Script Manager
				long SmallFondSize;
				unsigned short SmallFond;
				CStr255 tryFontName;
				SmallFondSize = ::GetScriptVariable(FallbackFontScriptID,smScriptSmallFondSize);
				SmallFond = (SmallFondSize >> 16);
				::GetFontName(SmallFond, tryFontName);
				if(tryFontName[0] != 0)	// The SmallFond exist.
				{
					csFont.fFixedFontNum = SmallFond;
					csFont.fFixedFont = tryFontName;
				}
			}

			fCharSetFonts->AssignItemsAt( 1, index, &csFont );

			changed = TRUE;
			if((csFont.fTxtrButtonResID!=0) || (csFont.fTxtrTextFieldResID!=0))
			{
			// Mapping of char set to textTraits resources
			// Txtr resources need to be locked and held in memory
				Int16 buttonFont, textField;
				buttonFont = csFont.fTxtrButtonResID;
				textField = csFont.fTxtrTextFieldResID;
				
				UseApplicationResFile();
				
				TextTraitsH	buttonH = (TextTraitsH) ::GetResource( 'Txtr', buttonFont );
				TextTraitsH	textH = (TextTraitsH) ::GetResource( 'Txtr', textField );
				if (buttonH && textH )
				{
					if (!*buttonH)
						::LoadResource((Handle)buttonH);
					
					if (*buttonH)
						::HNoPurge( (Handle)buttonH );
					
					if (!*textH)
						::LoadResource((Handle)textH);
					
					if (*textH)
						::HNoPurge( (Handle)textH );
					
					if (*buttonH && *textH )
					{
						short size = ::GetHandleSize((Handle)buttonH);
						::SetHandleSize((Handle)textH, sizeof(TextTraitsRecord));	// Because resources  are compressed
						(**textH).fontNumber = -1;//fontNumber_Unknown in UTextTraits
						StHandleLocker lock((Handle)textH);
						LString::CopyPStr(csFont.fFixedFont, (StringPtr)&(**textH).fontName, 255);
						(**textH).size = realFixedSize;
						(**buttonH).fontNumber = -1;	// fontNumber_Unknown
						::SetHandleSize((Handle)buttonH, sizeof(TextTraitsRecord));	// Because resources  are compressed
						StHandleLocker lock2((Handle)buttonH);
						LString::CopyPStr(csFont.fPropFont, (StringPtr)&(**buttonH).fontName, 255);
					}
				}
	#ifdef DEBUG
//				else
//					XP_ASSERT(FALSE);
	#endif
			}
		}
	}
	
	if (changed)
		SetModified();
	return changed;
}




Boolean CPrefs::GetFont( UInt16 win_csid, CCharSet* font )
{
	CCharSet			csFont;
	LArrayIterator		iter( *fCharSetFonts );
	win_csid &= ~CS_AUTO;
	while ( iter.Next( &csFont ) )
	{
		if ( (csFont.fCSID & ~CS_AUTO) == win_csid )
		{
			*font = csFont;
			return TRUE;
		}
	}
	font = &fDefaultFont;
	return FALSE;
}

Boolean CPrefs::GetFontAtIndex( unsigned long index, CCharSet* font )
{
	XP_ASSERT( index <= fCharSetFonts->GetCount() );
	
	return ( fCharSetFonts->FetchItemAt( index, font ) );
}
Int16 CPrefs::GetButtonFontTextResIDs(unsigned short csid)
{
	CCharSet csFont;
	GetFont(csid, &csFont );
	return csFont.fTxtrButtonResID;
}
Int16 CPrefs::GetTextFieldTextResIDs(unsigned short csid)
{
	CCharSet csFont;
	GetFont(csid, &csFont );
	return csFont.fTxtrTextFieldResID;
}

short CPrefs::GetProportionalFont(unsigned short csid)
{
	CCharSet csFont;
	GetFont(csid, &csFont );
	return csFont.fPropFontNum;
}
short CPrefs::GetFixFont(unsigned short csid)
{
	CCharSet csFont;
	GetFont(csid, &csFont );
	return csFont.fFixedFontNum;
}


// pkc 1/23/97
// PowerPC alignment is on by default, so we need
// to set 68k alignment for these structs
#pragma options align=mac68k

struct CsidPair {
	int16 			win_csid;
	int16 			doc_csid;
	int32 			cmdNum;
};

struct CsidTable
{
	int16 		itemcount;
	CsidPair 	pair[1];
};

#pragma options align=reset
//

int16 CPrefs::CmdNumToWinCsid( int32 cmdNum)
{
	// Get resource from application
	Handle hndl = ::GetResource( 'Csid', cmd2csid_tbl_ResID);
		// Note: ::GetResource calls _LoadResource internally.  You don't
		// have to call LoadResource again, it's a waste of space and time.
	ThrowIfResError_();
	if (hndl != NULL)
	{
		// lock in on the stack.
		StHandleLocker lock((Handle) hndl );
		// cast it into our internal data struct
		CsidTable** cthndl = (CsidTable**)hndl;
		// search it.
		int16 itemcount = (**cthndl).itemcount;
		CsidPair* item = &(**cthndl).pair[0];
		for (int16 i = 0; i < itemcount; i++, item++)
		{
			if (cmdNum == item->cmdNum)
				return item->win_csid;
		}
	}
	return CS_DEFAULT;
}

int16 CPrefs::CmdNumToDocCsid(int32 cmdNum)
{
	// Get resource from application
	Handle hndl = ::GetResource( 'Csid', cmd2csid_tbl_ResID);
		// Note: ::GetResource calls _LoadResource internally.  You don't
		// have to call LoadResource again, it's a waste of space and time.
	ThrowIfResError_();
	if (hndl != NULL)
	{
		// lock in on the stack.
		StHandleLocker lock((Handle) hndl );
		// cast it into our internal data struct
		CsidTable** cthndl = (CsidTable**)hndl;
		// search it.
		int16 itemcount = (**cthndl).itemcount;
		CsidPair* item = &(**cthndl).pair[0];
		for (int16 i = 0; i < itemcount; i++, item++)
		{
			if (cmdNum == item->cmdNum)
				return item->doc_csid;
		}
	}
	return CS_DEFAULT;
}

int32  CPrefs::WinCsidToCmdNum( int16 csid)
{
	// Get resource from application
	Handle hndl = ::GetResource( 'Csid', cmd2csid_tbl_ResID);
		// Note: ::GetResource calls _LoadResource internally.  You don't
		// have to call LoadResource again, it's a waste of space and time.
	ThrowIfResError_();
	if(hndl != NULL)
	{
		// lock in on the stack
		StHandleLocker lock((Handle) hndl );
		// cast it into our internal data struct
		CsidTable** cthndl = (CsidTable**)hndl;
		// search it.
		int16 itemcount = (**cthndl).itemcount;
		CsidPair* item = &(**cthndl).pair[0];
		for (int16 i = 0; i < itemcount; i++, item++)
		{
			if (csid == item->win_csid)
				return item->cmdNum;
		}
	}
	return cmd_Nothing;
}

enum {
	kScriptIsNotEnabled = 0,
	kScriptIsEnabled,
	kScriptHasFont
};

static Boolean CheckNamedFont(ConstStr255Param name)
{
	short family;
	::GetFNum(name, &family);
	return family != 0;
}
ScriptCode  CPrefs::CsidToScript(int16 csid)
{
	switch(csid)
	{
		case CS_DINGBATS:
			XP_ASSERT(TRUE);
			return -1;
		case CS_SYMBOL:
			XP_ASSERT(TRUE);
			return -1;
		default:
			CCharSet font;
			if(CPrefs::GetFont(csid,&font ))
				return font.fFallbackFontScriptID;
			else
			{
				XP_ASSERT(TRUE);
				return -1;
			}
	}
}
static void InitCheckScriptFont(Handle hndl)
{
	int scriptstatus[64];
	ScriptCode script;
	for(script = 0 ; script < 64 ; script++)
	{
		if((::GetScriptVariable(script, smScriptEnabled) & 0x00FF) == 0)
			scriptstatus[script] = kScriptIsNotEnabled;
		else
			scriptstatus[script] = kScriptIsEnabled;
	}

	int numOfFond = CountResources('FOND');
	ThrowIfResError_();
	for(short i=0; i < numOfFond; i++)
	{
		ScriptCode fontscript;
		short resid;
		ResType type;
		Str255 name;
		Handle handle = GetIndResource('FOND', i+1);
		GetResInfo(handle, &resid, &type, name);
		ThrowIfResError_();
		fontscript = FontToScript(resid);
		if(scriptstatus[fontscript] == kScriptIsEnabled)
			scriptstatus[fontscript] = kScriptHasFont;
	}
	unsigned short		numOfCsid;
	::DetachResource( hndl );
	ThrowIfResError_();
	
	LHandleStream	stream( hndl );
				
	stream.ReadData( &numOfCsid, sizeof( unsigned short) );
	XP_ASSERT(numOfCsid > 0);
	uint16 realNum = 0;
	int16 *reallist = new int16[numOfCsid];
	ThrowIfNil_( reallist );
	for(int i=0;i<numOfCsid;i++)
	{
		int16 thiscsid;
		stream.ReadData( &thiscsid,	sizeof( int16) );
		Boolean csidenabled;
		switch(thiscsid)
		{
			case CS_DINGBATS:
				csidenabled = CheckNamedFont("\pZapf Dingbats");
				break;
			case CS_SYMBOL:
				csidenabled = CheckNamedFont("\pSymbol");
				break;
			case CS_MAC_GREEK:
				csidenabled = (::GetScriptManagerVariable(smRegionCode) == verGreece);
				break;			
			default:
				ScriptCode csidScript = CPrefs::CsidToScript(thiscsid);
				csidenabled = (scriptstatus[ csidScript ] == kScriptHasFont);
				break;
		}
		if(csidenabled)
			reallist[ realNum++ ] = thiscsid;
	}
	INTL_SetUnicodeCSIDList(realNum, reallist);
	delete[] reallist;
}




//
//	I need to implement the RefCount for this table loader
//
struct RefCount
{
	int32 id;
	int32 count;
	char **cvthdl;
};
LArray* fXlatList =  NULL;
class xlateRefCountList {
	private:
	public:
		static void init();
		static char ** Inc(int32 inid);
		static int32 Dec(char **cvthdl);
		
};
void xlateRefCountList::init()
{
	fXlatList = new LArray(sizeof(RefCount));
}

char ** xlateRefCountList::Inc(int32 inid)
{
	RefCount			refcount;
	LArrayIterator		iter( *fXlatList );
	short	index;
	while ( iter.Next( &refcount ) )
	{
		if( (refcount.id) == inid )
		{
			index = fXlatList->FetchIndexOf( &refcount );
			if (refcount.count == 0)
			{
				// If the refcount is positive, the handle is guaranteed loaded and locked.
				// Otherwise, we must do these things.
				::LoadResource(refcount.cvthdl);
				::HLock(refcount.cvthdl);
			}
			refcount.count++;
			fXlatList->AssignItemsAt( 1, index, &refcount );
			return refcount.cvthdl;
		}
	}
	// we cannot find the refcount for this xlat. So, lets 
	// get the table from resource and insert one.
	Handle tableHandle;
	tableHandle =  ::GetResource('xlat',inid); // also loads!
	ThrowIfResError_();
	ThrowIfNil_(tableHandle);

	::HLock(tableHandle);
	ThrowIfMemError_();

	refcount.id = inid;
	refcount.count = 1;
	refcount.cvthdl = tableHandle;
	fXlatList->InsertItemsAt( 1, LArray::index_First, &refcount );
	return tableHandle;	
}

int32 xlateRefCountList::Dec(char **cvthdl)
{
	RefCount			refcount;
	LArrayIterator		iter( *fXlatList );
	while ( iter.Next( &refcount ) )
	{
		if( (refcount.cvthdl) == cvthdl )
		{
			short	index = fXlatList->FetchIndexOf( &refcount );
			if (refcount.count > 0)
			{
				refcount.count--;
				fXlatList->AssignItemsAt( 1, index, &refcount );

				if (refcount.count == 0)
				{
					::HUnlock((Handle) refcount.cvthdl);
					ThrowIfMemError_();
					// ::HPurge((Handle) refcount.cvthdl); Unnecessary, resources are purgeable.
				}
			}
			return refcount.count;
		}
	}
//	XP_ASSERT(FALSE);
//	Don't assert: we may receive an old copy of sLastTableHandle
	return 0;
}

static char **	sLastTableHandle	= nil;
static int32	sLastTableID		= -1;

char ** 
FE_GetSingleByteTable(int16 /*from_csid*/, int16 /*to_csid*/, int32 resourceid)
{
	char **			tableHandle;

	if(fXlatList == NULL)
		xlateRefCountList::init();

	// if we need the same table, return the previous copy
	if (resourceid == sLastTableID && sLastTableHandle != nil)
	{
		return sLastTableHandle;
	}

	// otherwise load the new table...
	tableHandle = xlateRefCountList::Inc(resourceid);

	// ...and make a copy of it
	if (sLastTableHandle != nil)
		::DisposeHandle(sLastTableHandle);
	sLastTableHandle = tableHandle;
	if (::HandToHand(&sLastTableHandle) == noErr)
		sLastTableID = resourceid;
	else
		sLastTableHandle = nil;

	return tableHandle;
}

void FE_FreeSingleByteTable(char **cvthdl)
{
	if(fXlatList == NULL)
		xlateRefCountList::init();

	if (cvthdl != sLastTableHandle)
		(void)xlateRefCountList::Dec(cvthdl); // safe to Dec more than we Inc'ed
}






