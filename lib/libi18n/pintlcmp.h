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
#ifndef _PINTLCMP_H_
#define _PINTLCMP_H_
#include "intlpriv.h"

typedef struct {
	unsigned char src_b1;
	unsigned char src_b2_start;
	unsigned char src_b2_end;
	unsigned char dest_b1;
	unsigned char dest_b2_start;
} DoubleByteToLowerMap;

/*	Prototype for private function */
MODULE_PRIVATE DoubleByteToLowerMap *INTL_GetDoubleByteToLowerMap(int16 csid);
MODULE_PRIVATE unsigned char *INTL_GetSingleByteToLowerMap(int16 csid);

#endif /* _PINTLCMP_H_ */


