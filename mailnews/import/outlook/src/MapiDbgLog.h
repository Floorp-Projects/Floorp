/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MapiDbgLog_h___
#define MapiDbgLog_h___

/*
#ifdef NS_DEBUG
#define MAPI_DEBUG	1
#endif
*/

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

