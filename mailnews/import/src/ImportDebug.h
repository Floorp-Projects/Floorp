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

#ifndef ImportDebug_h___
#define ImportDebug_h___

#ifdef NS_DEBUG
#define IMPORT_DEBUG	1
#endif

#include "nslog.h"

NS_DECL_LOG(ImportDebugLog)
#define PRINTF NS_LOG_PRINTF(ImportDebugLog)
#define FLUSH  NS_LOG_FLUSH(ImportDebugLog)

#ifdef IMPORT_DEBUG
#include <stdio.h>

#define	IMPORT_LOG0( x)	PRINTF( x)
#define	IMPORT_LOG1( x, y)	PRINTF( x, y)
#define	IMPORT_LOG2( x, y, z)	PRINTF( x, y, z)
#define	IMPORT_LOG3( a, b, c, d)	PRINTF( a, b, c, d)

#else

#define	IMPORT_LOG0( x)
#define	IMPORT_LOG1( x, y)
#define	IMPORT_LOG2( x, y, z)
#define	IMPORT_LOG3( a, b, c, d)

#endif

#endif
