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
/* Name:		<Xfe/XfeAll.h>											*/
/* Description:	Xfe widgets inclusion of all available widgets.			*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeAll_h_								/* start XfeAll.h		*/
#define _XfeAll_h_

#include <Xfe/Arrow.h>
#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>
#include <Xfe/Button.h>
#include <Xfe/Cascade.h>
#include <Xfe/Chrome.h>
#include <Xfe/DashBoard.h>
#include <Xfe/FrameShell.h>
#include <Xfe/Label.h>
#include <Xfe/Logo.h>
#include <Xfe/Oriented.h>
#include <Xfe/Pane.h>
#include <Xfe/ProgressBar.h>
#include <Xfe/Tab.h>
#include <Xfe/TaskBar.h>
#include <Xfe/ToolBar.h>
#include <Xfe/ToolBox.h>
#include <Xfe/ToolItem.h>
#include <Xfe/ToolScroll.h>

#if defined(XFE_WIDGETS_BUILD_DEMO)
#include <Xfe/TempTwo.h>
#endif /* XFE_WIDGETS_BUILD_DEMO */

#if defined(XFE_WIDGETS_BUILD_UNUSED)
#include <Xfe/BypassShell.h>
#include <Xfe/ComboBox.h>
#include <Xfe/FancyBox.h>
#include <Xfe/FontChooser.h>
#endif /* XFE_WIDGETS_BUILD_UNUSED */

#endif											/* end XfeAll.h			*/

