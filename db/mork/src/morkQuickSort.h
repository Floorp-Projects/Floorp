/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MORKQUICKSORT_
#define _MORKQUICKSORT_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

extern void
morkQuickSort(mork_u1* ioVec, mork_u4 inCount, mork_u4 inSize,
  mdbAny_Order inOrder, void* ioClosure);
 
//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKQUICKSORT_ */
