/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

/*

  The XUL "controllers" object.

*/

#ifndef nsXULControllers_h__
#define nsXULControllers_h__

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsIControllers.h"
#include "nsISupportsArray.h"
#include "nsISecurityCheckedComponent.h"
#include "nsCycleCollectionParticipant.h"

/* non-XPCOM class for holding controllers and their IDs */
class nsXULControllerData
{
public:
                            nsXULControllerData(PRUint32 inControllerID, nsIController* inController)
                            : mControllerID(inControllerID)
                            , mController(inController)
                            {                            
                            }

                            ~nsXULControllerData() {}

    PRUint32                GetControllerID()   { return mControllerID; }

    nsresult                GetController(nsIController **outController)
                            {
                              NS_IF_ADDREF(*outController = mController);
                              return NS_OK;
                            }
    
    PRUint32                mControllerID;
    nsCOMPtr<nsIController> mController;
};


NS_IMETHODIMP NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

class nsXULControllers : public nsIControllers,
                         public nsISecurityCheckedComponent
{
public:
    friend NS_IMETHODIMP
    NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULControllers, nsIControllers)
    NS_DECL_NSICONTROLLERS
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
  
protected:
    nsXULControllers();
    virtual ~nsXULControllers(void);

    void        DeleteControllers();

    nsVoidArray mControllers;
    PRUint32    mCurControllerID;
};




#endif // nsXULControllers_h__
