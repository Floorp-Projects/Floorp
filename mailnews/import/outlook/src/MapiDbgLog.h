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

#ifndef MapiDbgLog_h___
#define MapiDbgLog_h___

#ifdef NS_DEBUG
#define MAPI_DEBUG	1
#endif

#ifdef MAPI_DEBUG
#include "stdio.h"

#define MAPI_DUMP_STRING( x)		printf( "%s", (const char *)x)
#define MAPI_TRACE0( x)				printf( x)
#define MAPI_TRACE1( x, y)			printf( x, y)
#define MAPI_TRACE2( x, y, z)		printf( x, y, z)
#define MAPI_TRACE3( x, y, z, a)	printf( x, y, z, a)
#define MAPI_TRACE4( x, y, z, a, b) printf( x, y, z, a, b)


#else

#define MAPI_DUMP_STRING( x)
#define	MAPI_TRACE0( x)
#define	MAPI_TRACE1( x, y)
#define	MAPI_TRACE2( x, y, z)
#define MAPI_TRACE3( x, y, z, a)
#define MAPI_TRACE4( x, y, z, a, b)

#endif



#endif /* MapiDbgLog_h___ */

