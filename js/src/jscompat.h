/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996 Netscape Communications Corporation, All Rights Reserved.
 */
#ifndef jscompat_h___
#define jscompat_h___
/*
 * Compatibility glue for various NSPR versions.  We must always define int8,
 * int16, jsword, and so on to minimize differences with js/ref, no matter what
 * the NSPR typedef names may be.
 */
#include "jstypes.h"
#include "jslong.h"

typedef JSIntn intN;
typedef JSUintn uintN;
typedef JSUword jsuword;
typedef JSWord jsword;
typedef float float32;
#define allocPriv allocPool
#endif /* jscompat_h___ */
