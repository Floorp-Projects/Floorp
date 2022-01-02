/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseCommandController_h__
#define nsBaseCommandController_h__

#include "nsIController.h"
#include "nsIControllerContext.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWeakReferenceUtils.h"
#include "nsControllerCommandTable.h"

// The base editor controller is used for both text widgets, and all other text
// and html editing
class nsBaseCommandController final : public nsIController,
                                      public nsIControllerContext,
                                      public nsIInterfaceRequestor,
                                      public nsICommandController {
 public:
  /**
   * The default constructor initializes the instance with new
   * nsControllerCommandTable.  The other constructor does it with
   * the given aControllerCommandTable.
   */
  explicit nsBaseCommandController(
      nsControllerCommandTable* aControllerCommandTable =
          new nsControllerCommandTable());

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLER
  NS_DECL_NSICOMMANDCONTROLLER
  NS_DECL_NSICONTROLLERCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

  static already_AddRefed<nsBaseCommandController> CreateWindowController();
  static already_AddRefed<nsBaseCommandController> CreateEditorController();
  static already_AddRefed<nsBaseCommandController> CreateEditingController();
  static already_AddRefed<nsBaseCommandController> CreateHTMLEditorController();
  static already_AddRefed<nsBaseCommandController>
  CreateHTMLEditorDocStateController();

 protected:
  virtual ~nsBaseCommandController();

 private:
  nsWeakPtr mCommandContextWeakPtr;
  nsISupports* mCommandContextRawPtr;

  // Our reference to the command manager
  RefPtr<nsControllerCommandTable> mCommandTable;
};

#endif /* nsBaseCommandController_h_ */
