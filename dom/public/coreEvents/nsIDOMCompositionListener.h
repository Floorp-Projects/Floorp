/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
