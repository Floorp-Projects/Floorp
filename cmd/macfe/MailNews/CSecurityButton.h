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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CSecurityButton.h

#pragma once
#include <LListener.h>
class CMailComposeWindow;
class CMailNewsWindow;
class CMessageView;
class LWindow;
// Modifies the Security button to reflect wether the message is
// signed/encrypted
class CMailSecurityListener : public LListener
{
public:
	CMailSecurityListener() : mMessageView(NULL) {};
	virtual void	ListenToMessage(MessageT inMessage, void* ioParam);
	void SetMessageView( CMessageView* view) {  mMessageView = view;}
private:
	CMessageView *mMessageView;
}; 

class USecurityIconHelpers
{
public:
	enum State { eNone, eSigned, eEncrypted, eSignedEncrypted};
	enum { eBigSecurityButton = 'Bsec',
			eSmallEncryptedButton = 'SBsc',
			eSmallSignedButton = 'SBsg' };
			// Large security buttons must be 'Bsec'
			// Small security buttons must be 'SBsc' (encryption) or 'SBsg' (signed)
	static void UpdateComposeButton( CMailComposeWindow *window );
	static void UpdateMailWindow( CMessageView *messageView);
	static void SetNoMessageLoadedSecurityState( LWindow * window );
	static void AddListenerToSmallButton(LWindow* inWindow, LListener* inListener);
private:
	static void ChangeToSecurityState(
					LWindow* window,
					Boolean inEncrypted,
					Boolean inSigned ); 
};
