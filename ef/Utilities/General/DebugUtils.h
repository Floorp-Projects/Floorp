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

#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H

#include "Fundamentals.h"
#include "LogModule.h"

#ifdef DEBUG_LOG

const uint tabWidth = 0;	// Width of tabs in output stream; 0 means no tabs

void printSpaces(LogModuleObject &f, int nSpaces);
void printMargin(LogModuleObject &f, int margin);
int printUInt64(LogModuleObject &f, Uint64 l);
int printInt64(LogModuleObject &f, Int64 l);

#endif
#endif
