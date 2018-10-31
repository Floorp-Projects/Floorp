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
#include "nsTArray.h"
#include "nsIControllers.h"
#include "nsCycleCollectionParticipant.h"

/* non-XPCOM class for holding controllers and their IDs */
class nsXULControllerData final
{
public:
                            nsXULControllerData(uint32_t inControllerID, nsIController* inController)
                            : mControllerID(inControllerID)
                            , mController(inController)
                            {
                            }

                            ~nsXULControllerData() {}

    uint32_t                GetControllerID()   { return mControllerID; }

    nsresult                GetController(nsIController **outController)
                            {
                              NS_IF_ADDREF(*outController = mController);
                              return NS_OK;
                            }

    uint32_t                mControllerID;
    nsCOMPtr<nsIController> mController;
};

class nsXULControllers final : public nsIControllers
{
public:
    nsXULControllers();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULControllers, nsIControllers)
    NS_DECL_NSICONTROLLERS

protected:
    virtual ~nsXULControllers(void);

    void        DeleteControllers();

    nsTArray<nsXULControllerData*>   mControllers;
    uint32_t                         mCurControllerID;
};




#endif // nsXULControllers_h__
