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

#include "CTSMEditField.h"


Boolean						CTSMEditField::sInitialized 	= false;
Boolean 					CTSMEditField::sHasTSM 				= false;
TSMTEPreUpdateUPP	CTSMEditField::sPreUpdateUPP 	= NewTSMTEPreUpdateProc( CTSMEditField::PreUpdate );
TSMTEPostUpdateUPP	CTSMEditField::sPostUpdateUPP 	= NewTSMTEPostUpdateProc( CTSMEditField::PostUpdate );


//	Default constructor

CTSMEditField::CTSMEditField ()
{
}	//	CTSMEditField::CTSMEditField

CTSMEditField::CTSMEditField( LStream* inStream )

	:	LEditField( inStream )

	{

		if ( !sInitialized )
			Initialize();
			
		OSErr		result					= noErr;
		OSType	theServiceTypes = kTSMTEInterfaceType;
		
		mTSMDocID = 0;
		mTSMTEHandle = NULL;
		

		Try_ 
			{
			
	  		if ( sHasTSM )
	  			{
	  			
						result = ::NewTSMDocument( 1, &theServiceTypes, &mTSMDocID, static_cast<long>(&mTSMTEHandle) );
						ThrowIfOSErr_( result );

						if ( !mTSMTEHandle && mTSMDocID )
							{
							
								::DeleteTSMDocument( mTSMDocID );
								mTSMDocID = 0;
								
								Throw_( paramErr );
							
							}
						
						(*mTSMTEHandle)->textH = mTextEditH;
						(*mTSMTEHandle)->preUpdateProc = sPreUpdateUPP;
						(*mTSMTEHandle)->postUpdateProc = sPostUpdateUPP;
						(*mTSMTEHandle)->updateFlag = kTSMTEAutoScroll;
						(*mTSMTEHandle)->refCon = (Int32)this;
								
					}
					
			} 
				
		Catch_( inErr ) 
			{
			
// Failure just means that this edit field won't support TSMTE
	
			} 
			
		EndCatch_;
				
	}

//
//	Parameterized constructor

CTSMEditField::CTSMEditField (	const SPaneInfo&	inPaneInfo,
										Str255				inString,
										ResIDT				inTextTraitsID,
										Int16					inMaxChars,
										Uint8					inAttributes,
										TEKeyFilterFunc		inKeyFilter,
										LCommander*			inSuper)
					: LEditField ( 	inPaneInfo, 
									inString, 
									inTextTraitsID,
									inMaxChars, 
									inAttributes,
									inKeyFilter, 
									inSuper )

{
}	//	CTSMEditField::CTSMEditField


CTSMEditField::~CTSMEditField()
	{
	
		OSErr	result = noErr;
		
		try
			{
			
				if ( mTSMDocID != 0 )
					{
					
						::FixTSMDocument( mTSMDocID );
						::DeactivateTSMDocument( mTSMDocID ); //	for a bug in TSM.  See TE27
						
						result = ::DeleteTSMDocument( mTSMDocID );
						Assert_( result == noErr );
					
						mTSMDocID = 0;
						
					}
					
			}
			
		catch ( ... )
			{
			
			
			}
			
	}


pascal void 
CTSMEditField::PreUpdate( TEHandle inTEHandle, Int32 inRefCon )
	{
	
		CTSMEditField	*theOwnerEditField = NULL;

		if ( inRefCon != NULL )
			{
					
				theOwnerEditField = reinterpret_cast<CTSMEditField *>( inRefCon );
			
				theOwnerEditField->FocusDraw();
				
			}
	
	}


pascal void
CTSMEditField::PostUpdate( 
	TEHandle inTEHandle, 
	Int32 fixLen, 
	Int32 inputAreaStart, 
	Int32 inputAreaEnd, 
	Int32 pinStart, 
	Int32 pinEnd, 
	Int32 inRefCon )
	{
	
		CTSMEditField	*theOwnerEditField = NULL;

		if ( inRefCon != NULL && fixLen > 0 )
			{
					
				theOwnerEditField = reinterpret_cast<CTSMEditField *>( inRefCon );
			
				// Undo of TSM input is currently not supported.
				//
				if (theOwnerEditField->mTypingAction != NULL)
					theOwnerEditField->mTypingAction->Reset();
				
			}
	
	}

																			
void CTSMEditField::BeTarget( void )
	{

		OSErr	result = noErr;
		short	oldScript = ::GetScriptManagerVariable(smKeyScript);
		
		#ifdef Debug_Signal

			OSErr	err;

			//	check to see if a bug in TSM will be encountered
			ProcessSerialNumber	psn,
								csn;
			err = GetCurrentProcess(&psn);
			err = GetFrontProcess(&csn);
			Assert_((psn.highLongOfPSN == csn.highLongOfPSN) && (psn.lowLongOfPSN == csn.lowLongOfPSN));
		#endif

		FocusDraw();
		
		LEditField::BeTarget();
		
		if ( mTSMDocID != NULL )	
			{
			
				result = ::ActivateTSMDocument( mTSMDocID );
				Assert_( result == noErr );
				
			}
			

	if (oldScript != ::GetScriptManagerVariable(smKeyScript))
		::KeyScript(oldScript);
	}


void 
CTSMEditField::DontBeTarget( void )
	{
	
		OSErr	result = noErr;
		
		FocusDraw();
		
		if ( mTSMDocID != NULL )
			{
			
				::FixTSMDocument( mTSMDocID );
				
				result = ::DeactivateTSMDocument( mTSMDocID );
				Assert_( result == noErr );
				
			}
			
		LEditField::DontBeTarget();
	
	}
	

void
CTSMEditField::Initialize()
	{
	
		OSErr		result						= noErr;
		SInt32	gestaltResponse		= 0;
		
		Assert_( sInitialized == false );
		
		if ( sInitialized == false )
			{
			
				sInitialized = true;
				
		 		result = ::Gestalt( gestaltTSMgrVersion, &gestaltResponse );
		 	 	
				if ( (result == noErr) && (gestaltResponse >= 1) ) 
					{
						
						result = ::Gestalt( gestaltTSMTEAttr, &gestaltResponse );
				
						if ( (result == noErr) && ((gestaltResponse >> gestaltTSMTEPresent) & 1) )	
							{
							
								sHasTSM = true;
								
							}
							
					}
					
			}
	
	}
	
