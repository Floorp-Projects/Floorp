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

#ifndef STDAFX_PCH
#define STDAFX_PCH

#define OEMRESOURCE

#if defined(DEBUG_blythe)
//  Set up a flag specific to WFE developers in the client
#define DEBUG_WFE
#endif

/*	Very windows specific includes.
 */
/*  MFC, KFC, RUN DMC, whatever */
#include <afxwin.h>
#include <afxext.h>
#include <afxpriv.h>
#include <afxole.h>
#include <afxdisp.h>
#include <afxodlgs.h>
#ifdef _WIN32
#include <afxcmn.h>
#endif

/* More XP than anything */
#include "xp.h"
#include "fe_proto.h"
#include "fe_rgn.h"
#include "libi18n.h"
#include "xlate.h"
#include "ntypes.h"
#ifdef EDITOR
#include "edttypes.h"
#endif
#include "xpassert.h"
#include "lo_ele.h"
#include "layers.h"

/*  Standard C includes */
#ifndef _WIN32
#include <dos.h>
#endif
#include <malloc.h>
#include <direct.h>
#include <stdarg.h> 
#include <time.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifdef DEBUG
#include <assert.h>
#endif




/*	Very windows specific includes.
 */
/* WFE needs a layout file */
extern "C"  {
#include "layout.h"
}

/*	Some common defines. */
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

/*  All front end forward declarations needed to compile. */
#include "forward.h"

/*	Front end Casting macros. */
#include "cast.h"

/*	General purpose utilities. */
#include "feutil.h"

/*	afxData/sysInfo */
#include "sysinfo.h"

/*  Some defines we like everywhere. */
#include "resource.h"
#include "defaults.h"

/*  The application include and
 *	Commonly used, rarely changed headers */
#include "ncapiurl.h"
#include "genedit.h"
#include "genframe.h"
#include "genview.h"
#include "gendoc.h"
#include "intlwin.h"
#include "mozilla.h"
#include "cxwin.h"
#include "winproto.h"

#ifdef DEBUG
    #ifdef assert
        #undef assert
    #endif
    #define assert(x)   ASSERT(x)
#endif

#endif /* STDAFX_PCH */
