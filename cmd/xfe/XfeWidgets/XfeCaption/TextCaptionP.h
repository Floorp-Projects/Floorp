/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/TextCaptionP.h>									*/
/* Description:	XfeTextCaption widget private header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeTextCaptionP_h_						/* start TextCaptionP.h	*/
#define _XfeTextCaptionP_h_

#include <Xfe/CaptionP.h>
#include <Xfe/TextCaption.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION
	
/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaptionClassPart												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct
{
	XtPointer			extension;				/* extension			*/ 
} XfeTextCaptionClassPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaptionClassRec												*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTextCaptionClassRec
{
    CoreClassPart				core_class;
    CompositeClassPart			composite_class;
    ConstraintClassPart			constraint_class;
    XmManagerClassPart			manager_class;
    XfeManagerClassPart			xfe_manager_class;
    XfeCaptionClassPart			xfe_caption_class;
	XfeTextCaptionClassPart		xfe_text_caption_class;
} XfeTextCaptionClassRec;

externalref XfeTextCaptionClassRec xfeTextCaptionClassRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaptionPart													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTextCaptionPart
{
	/* Text resources */
	XmString			text_string;			/* Text string			*/
	XmFontList			text_font_list;			/* Text font list		*/

    /* Private data -- Dont even look past this comment -- */
} XfeTextCaptionPart;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaptionRec													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef struct _XfeTextCaptionRec
{
    CorePart				core;
    CompositePart			composite;
    ConstraintPart			constraint;
    XmManagerPart			manager;
    XfeManagerPart			xfe_manager;
    XfeCaptionPart			xfe_caption;
    XfeTextCaptionPart		xfe_text_caption;
} XfeTextCaptionRec;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeTextCaptionPart Access Macro										*/
/*																		*/
/*----------------------------------------------------------------------*/
#define _XfeTextCaptionPart(w) \
&(((XfeTextCaptionWidget) w) -> xfe_text_caption)

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end TextCaptionP.h	*/

