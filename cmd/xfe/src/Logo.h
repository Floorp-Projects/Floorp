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
/* Name:		Logo.h													*/
/* Description:	XFE_Logo component header file.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _xfe_logo_h_
#define _xfe_logo_h_

#include "Component.h"
#include "Frame.h"
#include "AnimationType.h"

//
// Animation Size
//
typedef enum
{
	XFE_ANIMATION_LARGE,
    XFE_ANIMATION_SMALL
} EAnimationSize;

class XFE_Logo : public XFE_Component
{
public:  
	
	XFE_Logo		(XFE_Frame *		frame,
					 Widget				parent,
					 const char *		name,
					 int				interval = 100);

	virtual ~XFE_Logo();

	// Accessors
	Cardinal			getCurrentFrame		();
	EAnimationSize		getSize				();
	EAnimationType		getType				();
	XP_Bool				isRunning			();

	// Mutators
	void				setCurrentFrame		(Cardinal index);
	void				setInterval			(Cardinal interval);
	void				setSize				(EAnimationSize size);
	void				setType				(EAnimationType type);

	// Control methods
	void				reset				();
	void				start				();
	void				stop				();

#ifdef NETSCAPE_PRIV
	// Hee hee
	void				easterEgg			(URL_Struct * url);
#endif /* NETSCAPE_PRIV */

	XP_Bool		processTraversal		(XmTraversalDirection direction);

private:

	XFE_Frame *			_frame;
	EAnimationType		_type;
	EAnimationSize		_size;
#ifdef NETSCAPE_PRIV
	XP_Bool             _mozillaEasterEgg;  // Have we visted about:mozilla ?
#endif /* NETSCAPE_PRIV */
	

	static Pixmap **		_small_animation_pixmaps;
	static fe_icon_data **	_small_animation_data;
	static Pixmap **		_large_animation_pixmaps;
	static fe_icon_data **	_large_animation_data;
	static Cardinal		 	_num_animations;
	static Cardinal *		_animation_frame_counts;
	static void		tipStringCallback	(Widget,XtPointer,XtPointer);

	void			updatePixmaps		();

	static void		initAnimationData	();
	static XP_Bool	animationsAreValid	();

	static Pixmap *	createAnimation		(XFE_Frame *		frame,
										 Widget				logo,
										 fe_icon_data *		animation_data,
										 Cardinal			num_frames);

	static Pixmap 	createPixmap		(XFE_Frame *		frame,
										 Widget				logo,
										 fe_icon_data *		icon_data);


	static void		logoCallback		(Widget,XtPointer,XtPointer);

};

#endif /* _xfe_logo_h_ */
