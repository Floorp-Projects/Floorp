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

//
// CProgressCaption
//
// Manages a progress bar and a text area to show loading status messages.
// Rewritten to use standard toolbox controls instead of drawing our own.
//

#pragma once


#include <LProgressBar.h>
#include <LCaption.h>


class CProgressCaption : public LView
{
	public:
		enum { class_ID = 'PgCp', kProgressBar = 'prog', kStatusText = 'capt' };
		enum { eBarHidden = -2, eIndefinite = -1 };

								CProgressCaption(LStream* inStream);
		virtual					~CProgressCaption();

		virtual void			SetDescriptor(ConstStringPtr inDescriptor);
		virtual void			SetDescriptor(const char* inCDescriptor);
		virtual StringPtr		GetDescriptor(Str255 outDescriptor) const;

		virtual	void			SetValue(Int32 inValue);
		virtual	Int32			GetValue() const;

	protected:

		virtual void			FinishCreateSelf ( ) ;
		
		LProgressBar*			mBar;
		LCaption*				mStatusText;
};

