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

///////////////////////////////////////////////////////////////////////////////////////
//
//	A simple lightweight implementation of a single column, multiple cell size, single
//	selection, no drag-and-drop, broadcaster table. Should be used for simple editable 
//	lists in dialog boxes.
//	 
///////////////////////////////////////////////////////////////////////////////////////
//
//	Who			When		What
//	---			----		----
//	
//	piro		12/1/97		Finished first implementation
//
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <LTableView.h>
#include <LBroadcaster.h>

const MessageT msg_SingleTextColumnDoubleClick	= '2Clk';

class	CSingleTextColumn : 
	public LTableView, 
	public LBroadcaster 

{
public:
	enum				{ class_ID = 'cSTC' };
	
						CSingleTextColumn(LStream *inStream);
	virtual				~CSingleTextColumn();

	Boolean 			IsLatentVisible(void);
	
	virtual	void 		ClickCell( const STableCell& inCell, const SMouseDownEvent& inMouseDown );
	virtual void		DrawCell( const STableCell &inCell, const Rect &inLocalRect);
	virtual void		DrawSelf();

protected:
	
	virtual void		ClickSelf( const SMouseDownEvent&	inMouseDown );
	
private:
	
	void				InitSingleTextColumn();		

	ResIDT				mTxtrID;
};

///////////////////////////////////////////////////////////////////////////////////////
// inline implementations

inline
Boolean
CSingleTextColumn::IsLatentVisible(void)
{
	return ((mVisible == triState_Latent) || (mVisible == triState_On));
}





