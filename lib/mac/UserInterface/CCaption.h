/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


// CCaption.h

#pragma once

#include <LCaption.h>
#include <LListener.h>

class LStream;

typedef Int32 LabelNum;

//	CCaption
//
// 	Moved from meditdlg.cp to be used as base class for the Listener/Lcaption derived 
//	class ( CListenerCaption ).
//
// ----
// 
// 	this class is draw a caption in a disabled state if appropriate
//

class CCaption : public LCaption
{
	public:
		enum { class_ID = 'CapD' };

						CCaption( LStream* inStream ) : LCaption ( inStream ) {};
	
		virtual void	DrawSelf();
		virtual void	EnableSelf();
		virtual void	DisableSelf();
};

//	CListenerCaption
//
//	A simple label class that is also a listener. It listens to 
//	messages and changes its content (i.e. text ). 
//
//	*** The Init method needs to be called before this class can be used.
//
// 	*** Needs exceptions support

class CListenerCaption : public CCaption, public LListener
{
	public:
		enum { class_ID = 'CCap' }; 						// class id 
		enum { default_menu_item = 1 };						// initial defualt menu item
										
							CListenerCaption( LStream *inStream );
		virtual				~CListenerCaption();
		
		virtual void		Init( const short strResID, const MessageT& getNew_msg ); 
		
		virtual void 		ChangeText(  const LabelNum& newLabelNum  );
		virtual void		ListenToMessage( MessageT inMessage, void *ioParam );		
		
			LabelNum		GetLabelNum() const;
			void			SetLabelNum(  const LabelNum& newLabelNum  );
			
	protected:
	
	private:
		LabelNum			labelNum;
		short				resourceID;
		MessageT			mMsg_changeText;
};
