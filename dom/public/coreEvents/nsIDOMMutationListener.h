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

#ifndef nsIDOMMutationListener_h__
#define nsIDOMMutationListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/*
 * Mutation event listener interface.
 */
// {0666EC94-3C54-4e16-8511-E8CC865F236C}
#define NS_IDOMMUTATIONLISTENER_IID \
{ 0x666ec94, 0x3c54, 0x4e16, { 0x85, 0x11, 0xe8, 0xcc, 0x86, 0x5f, 0x23, 0x6c } }

class nsIDOMMutationListener : public nsIDOMEventListener {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMMUTATIONLISTENER_IID)
    
  NS_IMETHOD SubtreeModified(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD NodeInserted(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD NodeRemoved(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD NodeRemovedFromDocument(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD NodeInsertedIntoDocument(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD AttrModified(nsIDOMEvent* aMutationEvent)=0;
  NS_IMETHOD CharacterDataModified(nsIDOMEvent* aMutationEvent)=0;
};
#endif // nsIDOMMutationListener_h__
