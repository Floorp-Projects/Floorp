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

#ifndef nsIEditorObserver_h__
#define nsIEditorObserver_h__
#include "nsISupports.h"
#include "nscore.h"

/*
Editor Observer interface to outside world
*/

#define NS_IEDITOROBSERVER_IID \
{/* a6cf9125-15b3-11d2-932e-00805f8add32 */ \
  0xa6cf9125, 0x15b3, 0x11d2, \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * A generic editor observer interface. 
 * <P>
 * nsIEditorObserver is the interface used by applications wishing to be notified
 * when the editor has completed a user action. 
 *
 */
class nsIEditorObserver : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITOROBSERVER_IID; return iid; }

  /** 
   * Called after the editor completes a user action.
   */
  NS_IMETHOD EditAction()=0;
};


#endif //nsIEditorObserver_h__

