/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _edttypep_h_
#define _edttypep_h_

#include "xp_mem.h"

#define EDITOR_TYPES    1
#define ED_Element      CEditElement
#define ED_Buffer       CEditBuffer
#define ED_TagCursor    CEditTagCursor
#define ED_BitArray     CBitArray

#define EDT_PersistentInsertPoint     CPersistentEditInsertPoint

class CEditElement;
class CEditBuffer;
class CEditTagCursor;
class CPersistentEditInsertPoint;

#endif
