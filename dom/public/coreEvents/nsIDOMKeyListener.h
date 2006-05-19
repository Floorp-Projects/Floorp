/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIDOMKeyListener_h__
#define nsIDOMKeyListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

/*
 * Key pressed / released / typed listener interface.
 */
#define NS_IDOMKEYLISTENER_IID \
{ /* 35f0d080-da38-11d1-bd85-00805f8ae3f4 */ \
0x35f0d080, 0xda38, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMKeyListener : public nsIDOMEventListener {

public:

     NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMKEYLISTENER_IID)
    
    /**
     * Processes a key pressed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key release event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent) = 0;

    /**
     * Processes a key typed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     *
     */
    NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMKeyListener, NS_IDOMKEYLISTENER_IID)

#endif // nsIDOMKeyListener_h__
