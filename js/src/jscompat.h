/*
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996-1999 Netscape Communications Corporation, All Rights Reserved.
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
