/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* The nsIInterfaceInfo xpcom public declaration. */

#ifndef nsIInterfaceInfo_h___
#define nsIInterfaceInfo_h___

// forward declaration of non-XPCOM types
class nsXPTMethodInfo;
class nsXPTConstant;

// {215DBE04-94A7-11d2-BA58-00805F8A5DD7}
#define NS_IINTERFACEINFO_IID   \
{ 0x215dbe04, 0x94a7, 0x11d2,   \
  { 0xba, 0x58, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIInterfaceInfo : public nsISupports
{
public:

    NS_IMETHOD GetName(char** name) = 0; // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIID(nsIID** iid) = 0;  // returns IAllocatator alloc'd copy

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent) = 0;

    // these include counts of parents
    NS_IMETHOD GetMethodCount(uint16* count) = 0;
    NS_IMETHOD GetConstantCount(uint16* count) = 0;

    // These include methods and constants of parents.
    // There do *not* make copies ***explicit bending of XPCOM rules***
    NS_IMETHOD GetMethodInfo(uint16 index, const nsXPTMethodInfo** info) = 0;
    NS_IMETHOD GetConstant(uint16 index, const nsXPTConstant** constant) = 0;
};

/***************************************************************************/
/***************************************************************************/
// XXX this is here just for testing - pulled from XPConnect...
// it can go away after nsIInterfaceInfos are being built from typelibs

// {159E36D0-991E-11d2-AC3F-00C09300144B}
#define NS_ITESTXPC_FOO_IID       \
{ 0x159e36d0, 0x991e, 0x11d2,   \
  { 0xac, 0x3f, 0x0, 0xc0, 0x93, 0x0, 0x14, 0x4b } }

class nsITestXPCFoo : public nsISupports
{
public:
    NS_IMETHOD Test(int p1, int p2, int* retval) = 0;
    NS_IMETHOD Test2() = 0;
};

// {5F9D20C0-9B6B-11d2-9FFE-000064657374}
#define NS_ITESTXPC_FOO2_IID       \
{ 0x5f9d20c0, 0x9b6b, 0x11d2,     \
  { 0x9f, 0xfe, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsITestXPCFoo2 : public nsITestXPCFoo
{
public:
};

/***************************************************************************/
/***************************************************************************/

#endif /* nsIInterfaceInfo_h___ */
