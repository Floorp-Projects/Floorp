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

#ifndef nsIDOMEventCapturer_h__
#define nsIDOMEventCapturer_h__

#include "nsIDOMEventReceiver.h"
class nsIDOMEventListener;
class nsIDOMMouseListener;
class nsIDOMMouseMotionListener;
class nsIDOMKeyListener;
class nsIDOMFocusListener;
class nsIDOMLoadListener;
class nsIDOMDragListener;
class nsIEventListenerManager;

/*
 * DOM event source class.  Object that allow event registration and distribution 
 * from themselves implement this interface.
 */
#define NS_IDOMEVENTCAPTURER_IID \
{ /* a044aec0-df04-11d1-bd85-00805f8ae3f4 */ \
0xa044aec0, 0xdf04, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMEventCapturer : public nsIDOMEventReceiver {

public:

  NS_IMETHOD CaptureEvent(nsIDOMEventListener *aListener) = 0;
  NS_IMETHOD ReleaseEvent(nsIDOMEventListener *aListener) = 0;

};
#endif // nsIDOMEventCapturer_h__
