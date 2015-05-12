/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// The base editor controller is used for both text widgets, and all other text
// and html editing
class nsBaseCommandController
  : public nsIController
  , public nsIControllerContext
  , public nsIInterfaceRequestor
  , public nsICommandController
{
public:
  nsBaseCommandController();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLER
  NS_DECL_NSICOMMANDCONTROLLER
  NS_DECL_NSICONTROLLERCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

protected:
  virtual ~nsBaseCommandController();

private:
  nsWeakPtr mCommandContextWeakPtr;
  nsISupports* mCommandContextRawPtr;

  // Our reference to the command manager
  nsCOMPtr<nsIControllerCommandTable> mCommandTable;
};

#endif /* nsBaseCommandController_h_ */
