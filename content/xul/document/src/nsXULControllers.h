/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  The XUL "controllers" object.

*/

#ifndef nsXULControllers_h__
#define nsXULControllers_h__

#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsIControllers.h"
#include "nsISupportsArray.h"

class nsIDOMXULCommandDispatcher;

class nsXULControllers : public nsIControllers
{
public:
    friend NS_IMETHODIMP
    NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTROLLERS
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
  
protected:
    nsXULControllers();
    virtual ~nsXULControllers(void);

    nsCOMPtr<nsISupportsArray> mControllers;
    nsWeakPtr mCommandDispatcher;
};




#endif // nsXULControllers_h__
