/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specifzic language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIRangeList_h___
#define nsIRangeList_h___

#include "nsISupports.h"
#include "nsIDOMRange.h"

// IID for the nsIRangeList interface
#define NS_IRANGELIST      \
{ 0xaeca730, 0x835b, 0x11d2, \
  { 0x8f, 0x30, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }

// IID for the nsIRangeListIterator interface
#define NS_IRANGELISTITERATOR      \
{ 0x5ddb21a1, 0x8362, 0x11d2, \
  { 0x8f, 0x30, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }

// IID for the nsIRangeList Factort interface
#define NS_IRANGELISTFACTORY      \
{ 0xf8052641, 0x8768, 0x11d2, \
  { 0x8f, 0x39, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }

//----------------------------------------------------------------------

// RangeList Interface
class nsIRangeList : public nsISupports {
public:

    /** AddRange will take an IRange and keep track of it
     *  @param aRange is the Range to be added
     */
    virtual nsresult AddRange(nsIDOMRange *aRange)=0;

    /** RemoveRange will take an IRange and look for it
     *  it will remove it if it finds it.
     *  @param aRange is the Range to be removed
     */
    virtual nsresult RemoveRange(nsIDOMRange *aRange)=0;

    /** Clear will clear all ranges from list
     */
    virtual nsresult Clear()=0;
};

class nsIRangeListIterator : public nsISupports {
public:

    /** Next will get the next nsIDOMRange in the list. WILL ADDREF
     *  @param aRange will receive the next irange in the list
     */
    virtual nsresult Next(nsIDOMRange **aRange)=0;

    /** Prev will get the prev nsIDOMRange in the list. WILL ADDREF
     *  @param aRange will receive the prev irange in the list
     */
    virtual nsresult Prev(nsIDOMRange **aRange)=0;

    virtual nsresult Reset()=0;
};

/** Factory methods
 *  
 */
nsresult NS_NewRangeList(nsIRangeList **aList);

#endif /* nsIRangeList_h___ */
