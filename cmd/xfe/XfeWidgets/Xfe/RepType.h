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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/RepType.h>											*/
/* Description:	Xfe widgets representation types public header.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeRepType_h_							/* start RepType.h		*/
#define _XfeRepType_h_

#include <Xm/Xm.h>								/* Motif public defs	*/

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Representation types													*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Boolean
XfeRepTypeCheck				(Widget				w,
							 String				rep_type,
							 unsigned char *	address,
							 unsigned char		fallback);
/*----------------------------------------------------------------------*/
extern void			XfeRegisterAccentType				(void);
extern void			XfeRegisterArrowType				(void);
extern void			XfeRegisterBmSelectionType			(void);
extern void			XfeRegisterBoxType					(void);
extern void			XfeRegisterBufferType				(void);
extern void			XfeRegisterButtonLayout				(void);
extern void			XfeRegisterButtonTrigger			(void);
extern void			XfeRegisterButtonType				(void);
extern void			XfeRegisterChromeChildType			(void);
extern void			XfeRegisterComboBoxType				(void);
extern void			XfeRegisterLocationType				(void);
extern void			XfeRegisterPaneChildAttachment		(void);
extern void			XfeRegisterPaneChildType			(void);
extern void			XfeRegisterPaneDragModeType			(void);
extern void			XfeRegisterPaneSashType				(void);
extern void			XfeRegisterRulesType				(void);
extern void			XfeRegisterToolBarIndicatorLocation	(void);
extern void			XfeRegisterToolBarSelectionPolicy	(void);
extern void			XfeRegisterToolScrollArrowPlacement	(void);
/*----------------------------------------------------------------------*/

extern void			XfeRegisterRepresentationTypes		(void);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end RepType.h		*/
