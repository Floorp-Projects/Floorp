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

//================================================================================
// 64bit Arithmetic Support Functions

extern NS_NATIVECALL(int64) x86Mul64Bit(int64 a, int64 b);
extern NS_NATIVECALL(int64) x86Div64Bit(int64 a, int64 b);
extern NS_NATIVECALL(int64) x86Mod64Bit(int64 a, int64 b);
extern NS_NATIVECALL(int64) x86Shl64Bit(int64 a, int b);
extern NS_NATIVECALL(uint64) x86Shr64Bit(uint64 a, int b);
extern NS_NATIVECALL(int64) x86Sar64Bit(int64 a, int b);
extern NS_NATIVECALL(int64) x86ThreeWayCMP_L(int64 a, int64 b);
extern NS_NATIVECALL(int64) x86ThreeWayCMPC_L(int64 a, int64 b);
extern NS_NATIVECALL(int64) x86Extract64Bit(int64 a, int b);
