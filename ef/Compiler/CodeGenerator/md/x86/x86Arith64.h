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
