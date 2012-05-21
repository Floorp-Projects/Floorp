/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  The XUL "controllers" object.

*/

#ifndef nsXULControllers_h__
#define nsXULControllers_h__

#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsIControllers.h"
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


nsresult NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

class nsXULControllers : public nsIControllers,
                         public nsISecurityCheckedComponent
{
public:
    friend nsresult
    NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULControllers, nsIControllers)
    NS_DECL_NSICONTROLLERS
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
  
protected:
    nsXULControllers();
    virtual ~nsXULControllers(void);

    void        DeleteControllers();

    nsTArray<nsXULControllerData*>   mControllers;
    PRUint32                         mCurControllerID;
};




#endif // nsXULControllers_h__
