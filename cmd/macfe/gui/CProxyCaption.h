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

// ===========================================================================
//	CProxyCaption.h
//
// ===========================================================================

#ifndef CProxyCaption_H
#define CProxyCaption_H
#pragma once

#include <LCaption.h>

class	CProxyCaption : public LCaption
{
public:
	enum { class_ID = 'Bpxc' };

	typedef LCaption super;
	
						CProxyCaption(
								LStream*		inStream);
	
	virtual void		SetDescriptor(
								ConstStringPtr	inDescriptor);

protected:
	virtual void		FinishCreateSelf();
	virtual void		DrawSelf();

	static void			DrawText(
								Ptr				inText,
								Int32			inLength,
								const Rect&		inRect);
	
	Rect				mOriginalFrame;
};

#endif
