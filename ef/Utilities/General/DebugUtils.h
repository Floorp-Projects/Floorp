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
