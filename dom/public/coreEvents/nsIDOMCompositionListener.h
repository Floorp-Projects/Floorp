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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIDOMCompositionListener_h__
#define nsIDOMCompositionListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/*
 * Key pressed / released / typed listener interface.
 */
// {F14B6491-E95B-11d2-9E85-0060089FE59B}
#define NS_IDOMCOMPOSITIONLISTENER_IID	\
{ 0xf14b6491, 0xe95b, 0x11d2, \
{ 0x9e, 0x85, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }


class nsIDOMCompositionListener : public nsIDOMEventListener {

public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMCOMPOSITIONLISTENER_IID)

  NS_IMETHOD HandleStartComposition(nsIDOMEvent* aCompositionEvent) = 0;
  NS_IMETHOD HandleEndComposition(nsIDOMEvent* aCompositionEvent) = 0;
  NS_IMETHOD HandleQueryComposition(nsIDOMEvent* aCompositionEvent) = 0;
  NS_IMETHOD HandleQueryReconversion(nsIDOMEvent* aCompositionEvent) = 0;
};
#endif // nsIDOMCompositionListener_h__
