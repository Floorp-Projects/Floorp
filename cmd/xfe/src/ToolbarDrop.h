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
/*---------------------------------------*/
/*																		*/
/* Name:		ToolbarDrop.h											*/
/* Description:	Classes to support drop stuff on toolbars.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _TOOLBAR_DROP_H_
#define _TOOLBAR_DROP_H_

#include "DragDrop.h"

class XFE_PersonalToolbar;
class XFE_BookmarkMenu;

//
// XFE_PersonalDrop class
//

class XFE_ToolbarDrop : public XFE_DropNetscape
{
public:

    XFE_ToolbarDrop			(Widget);
    ~XFE_ToolbarDrop		();

protected:

    void			targets			();
    void			operations		();
    int				processTargets	(Atom*,const char**,int);

    virtual void	addEntry		(const char * address,const char * title);
	Boolean			isFromSameShell	();

private:
};

//
// XFE_PersonalDrop class
//

class XFE_PersonalDrop : public XFE_ToolbarDrop
{
public:

    XFE_PersonalDrop		(Widget					dropWidget,
							 XFE_PersonalToolbar *	toolbar);

    ~XFE_PersonalDrop		();

protected:

    virtual void	addEntry		(const char * address,const char * title);
    virtual void	dragIn			();
	virtual void	dragOut			();
    virtual void	dragMotion		();
    virtual void	dropComplete	();

protected:

    XFE_PersonalToolbar *	_personalToolbar;
	Widget					_dropWidget;
};

//
// XFE_PersonalTabDrop class
//

class XFE_PersonalTabDrop : public XFE_PersonalDrop
{
public:

    XFE_PersonalTabDrop		(Widget					dropWidget,
							 XFE_PersonalToolbar *	toolbar);

    ~XFE_PersonalTabDrop	();

protected:

	virtual void	addEntry		(const char * address,const char * title);
    virtual void	dragIn			();
	virtual void	dragOut			();
    virtual void	dragMotion		();
    virtual void	dropComplete	();
};

//
// XFE_QuickfileDrop class
//

class XFE_QuickfileDrop : public XFE_ToolbarDrop
{
public:																  
																	  
    XFE_QuickfileDrop			(Widget					dropWidget,
							 XFE_BookmarkMenu *		quickfileMenu);


    ~XFE_QuickfileDrop		();

protected:

    virtual void	addEntry		(const char * address,const char * title);
    virtual void	dragIn			();
	virtual void	dragOut			();
    virtual void	dropComplete	();

private:

	XFE_BookmarkMenu *		_quickfileMenu;
};



#endif // _TOOLBAR_DROP_H_
