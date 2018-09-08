/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseCommandController_h__
#define nsBaseCommandController_h__

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

  static already_AddRefed<nsIController> CreateWindowController();
  static already_AddRefed<nsIController> CreateEditorController();
  static already_AddRefed<nsIController> CreateEditingController();
  static already_AddRefed<nsIController> CreateHTMLEditorController();

protected:
  virtual ~nsBaseCommandController();

private:
  nsWeakPtr mCommandContextWeakPtr;
  nsISupports* mCommandContextRawPtr;

  // Our reference to the command manager
  nsCOMPtr<nsIControllerCommandTable> mCommandTable;
};

#endif /* nsBaseCommandController_h_ */
