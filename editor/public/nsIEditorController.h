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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef NS_IEDITOR_CONTROLLER_H
#define NS_IEDITOR_CONTROLLER_H

#define NS_IEDITORCONTROLLER_IID_STR "075F6CB1-B26D-11d3-9933-00108301233C"

#define NS_IEDITORCONTROLLER_IID \
  { 0x75f6cb1, 0xb26d, 0x11d3, \
    { 0x99, 0x33, 0x0, 0x10, 0x83, 0x1, 0x23, 0x3c }}


class nsIEditor;
class nsCString;

class nsIEditorController : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEDITORCONTROLLER_IID)

  /** init the controller
   *  aCommandRefCon = a cookie that is passed to commands
   */
  NS_IMETHOD Init(nsISupports *aCommandRefCon) = 0;

  /** Set the cookie that is passed to commands
   */
  NS_IMETHOD SetCommandRefCon(nsISupports *aCommandRefCon) = 0;

};


#endif //NS_IEDITOR_CONTROLLER_H


