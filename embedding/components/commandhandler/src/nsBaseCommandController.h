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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Judge   <mjudge@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsBaseCommandController_h__
#define nsBaseCommandController_h__

#define NS_BASECOMMANDCONTROLLER_CID \
{ 0xbf88b48c, 0xfd8e, 0x40b4, { 0xba, 0x36, 0xc7, 0xc3, 0xad, 0x6d, 0x8a, 0xc9 } }
#define NS_BASECOMMANDCONTROLLER_CONTRACTID \
 "@mozilla.org/embedcomp/base-command-controller;1"


#include "nsIController.h"
#include "nsIControllerContext.h"
#include "nsIControllerCommandTable.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWeakReferenceUtils.h"

// The base editor controller is used for both text widgets, 
//   and all other text and html editing
class nsBaseCommandController :  public nsIController,
                            public nsIControllerContext,
                            public nsIInterfaceRequestor,
                            public nsICommandController
{
public:

          nsBaseCommandController();
  virtual ~nsBaseCommandController();

  // nsISupports
  NS_DECL_ISUPPORTS
    
  // nsIController
  NS_DECL_NSICONTROLLER

  // nsICommandController
  NS_DECL_NSICOMMANDCONTROLLER

  //nsIControllerContext
  NS_DECL_NSICONTROLLERCONTEXT

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR
  
private:

   nsWeakPtr mCommandContextWeakPtr;
   nsISupports* mCommandContextRawPtr;
   
   // Our reference to the command manager
   nsCOMPtr<nsIControllerCommandTable> mCommandTable;     
};

#endif /* nsBaseCommandController_h_ */

