/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsUCvKODll_h___
#define nsUCvKODll_h___

#include "prtypes.h"

extern "C" PRUint16 g_utKSC5601Mapping[];
extern "C" PRUint16 g_ufKSC5601Mapping[];
#define g_AsciiMapping ucvko_g_AsciiMapping
extern "C" PRUint16 g_AsciiMapping[];
extern "C" PRUint16 g_HangulNullMapping[];

#endif /* nsUCvKODll_h___ */
