/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIDOMKeyListener_h__
#define nsIDOMKeyListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/*
 * Key pressed / released / typed listener interface.
 */
#define NS_IDOMKEYLISTENER_IID \
{ /* 35f0d080-da38-11d1-bd85-00805f8ae3f4 */ \
0x35f0d080, 0xda38, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMKeyListener : public nsIDOMEventListener {

public:

    /**
     * Processes a key pressed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key release event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key typed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     *
     */
    virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent) = 0;

};
#endif // nsIDOMKeyListener_h__
