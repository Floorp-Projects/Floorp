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

#include "CValidEditField.h"

//-----------------------------------
CValidEditField::CValidEditField( LStream* s )
//-----------------------------------
:	LGAEditField( s )
,	mValidationFunc( nil )
{
}

//-----------------------------------
Boolean CValidEditField::AllowTargetSwitch( LCommander* /*newTarget*/ )
//-----------------------------------
{
	if ( mValidationFunc )
		return (*mValidationFunc )(this);
	return true;
}

//-----------------------------------
void CValidEditField::SetValidationFunction( ValidationFunc validationFunc )
//-----------------------------------
{
	mValidationFunc = validationFunc;
}

/********************************************************************************
 * Validation Functions
 ********************************************************************************/
#include "uprefd.h"  // for constants used below.
#include "uerrmgr.h"
#include "resgui.h"


Boolean ConstrainEditField( LEditField* whichField, long minValue, long maxValue )
{
	long		value;
	Boolean		allowSwitch = TRUE;
	
	value = whichField->GetValue();	
	if ( value > maxValue || value < minValue )
	{
		allowSwitch = FALSE;
		if ( value > maxValue )
			whichField->SetValue( maxValue );
		else
			whichField->SetValue( minValue );
	}
	return allowSwitch;
}

Boolean ValidateCacheSize( CValidEditField* bufferSize )
{
	Boolean		allowSwitch = TRUE;
	
	allowSwitch = ConstrainEditField( bufferSize, BUFFER_MIN / BUFFER_SCALE,
		 BUFFER_MAX / BUFFER_SCALE );
	if ( !allowSwitch )
	{
		UDesktop::Deactivate();
		::CautionAlert( 1063, NULL );
		UDesktop::Activate();
	}

	return allowSwitch;
}

Boolean ValidateNumberNewsArticles( CValidEditField* articles )
{
	Boolean		allowSwitch = TRUE;
	
	allowSwitch = ConstrainEditField( articles, NEWS_ARTICLES_MIN, NEWS_ARTICLES_MAX );
	if ( !allowSwitch )
	{
		UDesktop::Deactivate();
		::CautionAlert( 1066, NULL );
		UDesktop::Activate();
	}
	return allowSwitch;
}

Boolean ValidateDaysTilExpire( CValidEditField* daysTilExpire )
{
	Boolean		allowSwitch = TRUE;
	
	allowSwitch = ConstrainEditField( daysTilExpire, EXPIRE_MIN, EXPIRE_MAX );
	if ( !allowSwitch )
	{
		UDesktop::Deactivate();
		::CautionAlert( 1064, NULL );
		UDesktop::Activate();
	}		
	
	return allowSwitch;
}

Boolean ValidateNumberConnections( CValidEditField* connections )
{
	Boolean		allowSwitch = TRUE;
	
	allowSwitch = ConstrainEditField( connections, CONNECTIONS_MIN, CONNECTIONS_MAX );
	if ( !allowSwitch )
	{
		UDesktop::Deactivate();
		::CautionAlert( 1062, NULL );
		UDesktop::Activate();
	}		
		
	return allowSwitch;
}

Boolean ValidatePopID( CValidEditField* connections )
{
	CStr255 value;
	connections->GetDescriptor(value);
	if (value.Pos("@"))
	// еее FIX ME l10n
	{
		ErrorManager::PlainAlert(POP_USERNAME_ONLY);
		return FALSE;
	}
	else
		return TRUE;
}

void TargetOnEditField( LEditField* editField, Boolean doTarget )
{
	if ( doTarget )
	{
		editField->Enable();
		editField->Refresh();
		// pkc -- Call SetLatentSub instead of SwitchTarget
		if( editField->GetSuperCommander() )
			(editField->GetSuperCommander())->SetLatentSub(editField);
		editField->SelectAll();
	}
	else
	{
		editField->Disable();
		editField->Refresh();
		// pkc -- Call SetLatentSub instead of SwitchTarget
		if( editField->GetSuperCommander() )
			(editField->GetSuperCommander())->SetLatentSub(editField);
	}
}

