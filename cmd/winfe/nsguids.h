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

/* NSGUIDS.H contains the interface ID's for internal objects.  To add
 * new interfaces, define an APIID with the appropriate interface identifier
 * along with a number.  IDs are based off of a guid which was created
 * on my ethernet card (JRE). To define a guid, add:
 *     APIID(<your interface id>,<number>)
 * where number is the next consecutive number in the list.	
 */

#ifndef __NSGUIDS_H
#define __NSGUIDS_H

#ifndef __APIAPI
	#include "apiapi.h"
#endif

APIID(IID_IPageSetup,       1);		/* Page setup api id */
APIID(IID_IChrome,          2);		/* Chrome object */
APIID(IID_ITabControl,      3);		/* Tab control */
APIID(IID_IPage,            4);		/* Page for the pager */
APIID(IID_IPager,           5);		/* Pager (Tabbed view that swaps pages) */
APIID(IID_IImageMap,        6);     /* Image Map */
APIID(IID_IMsgCompose,		7);		/* Interface for FE_* calls defined by libmsg */
APIID(IID_IMsgList,			8);		/*    "  */
APIID(IID_IMailNewsPage,	9);		/* Mail/News page specific */
APIID(IID_IMWContext,		10);	/* MWContext Interface */
APIID(IID_IOutlinerParent,	11);    /* Outliner parent - column controller */
APIID(IID_IOutliner,		12);    /* Outliner */
APIID(IID_INSToolBar,       13);    /* ToolBar */
APIID(IID_INSAnimation,		14);	/* "Starfield" animation */
APIID(IID_IMailFrame,		15);	/* Mail Frame Hack */
APIID(IID_INSStatusBar,     16);    /* Status Bar */
APIID(IID_IAddressControl,	17);    /* Address control */

#endif
