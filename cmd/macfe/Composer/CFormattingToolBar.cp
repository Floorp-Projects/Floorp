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

#include "CFormattingToolBar.h"
#include "CEditView.h"
#include "resgui.h"
#include "edt.h"

CFormattingToolBar::CFormattingToolBar(LStream * inStream) 
							: CPatternBevelView(inStream)
{
}

CFormattingToolBar::~CFormattingToolBar()
{
	mEditView = NULL;
}

void CFormattingToolBar::FinishCreateSelf()
{
	if ( GetSuperView() )
	{
		// get SuperView (we start with "this" so we're guaranteed non-0)
		LView *superView = (LView *)this;
		while (superView->GetSuperView() != NULL)
			superView = superView->GetSuperView();
		
		mEditView = dynamic_cast<CEditView*>(superView->FindPaneByID( CEditView::pane_ID ));
	}
	else
		mEditView = dynamic_cast<CEditView*>(FindPaneByID( CEditView::pane_ID ));

	// if we have a mailcompose window show insert object popup menu
	// check for presence of insert menu within CFormattingToolBar
	LPane *pane = FindPaneByID('InsO');
	if ( pane )
	{
		CMailEditView *mailview = dynamic_cast<CMailEditView*>( mEditView );
		if ( mailview )
			pane->Show();
		else
			pane->Hide();
	}
	
	UReanimator::LinkListenerToControls(this, this, 11616);
}

void CFormattingToolBar::ListenToMessage( MessageT inMessage, void* ioParam )
{
	PaneIDT paneID = CEditView::pane_ID;
	
	if ( mEditView == NULL )
		return;
	
	if ( inMessage == 'Para' )
	{
		switch  (*(long*)ioParam)
		{
			case 1:		inMessage = cmd_Format_Paragraph_Normal;		break;
			case 2:		inMessage = cmd_Format_Paragraph_Head1;			break;
			case 3:		inMessage = cmd_Format_Paragraph_Head2;			break;
			case 4:		inMessage = cmd_Format_Paragraph_Head3;			break;
			case 5:		inMessage = cmd_Format_Paragraph_Head4;			break;
			case 6:		inMessage = cmd_Format_Paragraph_Head5;			break;
			case 7:		inMessage = cmd_Format_Paragraph_Head6;			break;
			case 8:		inMessage = cmd_Format_Paragraph_Address;		break;
			case 9:		inMessage = cmd_Format_Paragraph_Formatted;		break;
			case 10:	inMessage = cmd_Format_Paragraph_List_Item;		break;
			case 11:	inMessage = cmd_Format_Paragraph_Desc_Title;	break;
			case 12:	inMessage = cmd_Format_Paragraph_Desc_Text;		break;
		}
	}
	else if ( inMessage == 'Size' )
	{
		switch  (*(long*)ioParam)
		{
			case 1:		inMessage = cmd_Format_Font_Size_N2;	break;
			case 2:		inMessage = cmd_Format_Font_Size_N1;	break;
			case 3:		inMessage = cmd_Format_Font_Size_0;		break;
			case 4:		inMessage = cmd_Format_Font_Size_P1;	break;
			case 5:		inMessage = cmd_Format_Font_Size_P2;	break;
			case 6:		inMessage = cmd_Format_Font_Size_P3;	break;
			case 7:		inMessage = cmd_Format_Font_Size_P4;	break;
		}
	}
	else if ( inMessage == 'Algn' )
	{
		switch  (*(long*)ioParam)
		{
			case 1:		inMessage = cmd_JustifyLeft;	break;
			case 2:		inMessage = cmd_JustifyCenter;	break;
			case 3:		inMessage = cmd_JustifyRight;	break;
		}
	}
	else if ( inMessage == 'InsO' )
	{
		switch  (*(long*)ioParam)
		{
			case 1:		inMessage = cmd_Insert_Link;	break;
			case 2:		inMessage = cmd_Insert_Target;	break;
			case 3:		inMessage = cmd_Insert_Image;	break;
			case 4:		inMessage = cmd_Insert_Line;	break;
			case 5:		inMessage = cmd_Insert_Table;	break;
		}
	}

	mEditView->ObeyCommand( inMessage, ioParam );
}

