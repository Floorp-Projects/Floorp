/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPIWindowRoot_h__
#define nsPIWindowRoot_h__

#include "nsISupports.h"
#include "nsIDOMEventTarget.h"

class nsPIDOMWindow;
class nsIControllers;
class nsIController;
struct JSContext;

// 426C1B56-E38A-435E-B291-BE1557F2A0A2
#define NS_IWINDOWROOT_IID \
{ 0xc89780f2, 0x8905, 0x417f, \
  { 0xa6, 0x62, 0xf6, 0xc, 0xa6, 0xd7, 0xc, 0x91 } }

class nsPIWindowRoot : public nsIDOMEventTarget
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWINDOWROOT_IID)

  virtual nsPIDOMWindow* GetWindow()=0;

  // get and set the node that is the context of a popup menu
  virtual nsIDOMNode* GetPopupNode() = 0;
  virtual void SetPopupNode(nsIDOMNode* aNode) = 0;

  virtual nsresult GetControllerForCommand(const char *aCommand,
                                           nsIController** aResult) = 0;
  virtual nsresult GetControllers(nsIControllers** aResult) = 0;

  virtual void SetParentTarget(nsIDOMEventTarget* aTarget) = 0;
  virtual nsIDOMEventTarget* GetParentTarget() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIWindowRoot, NS_IWINDOWROOT_IID)

#endif // nsPIWindowRoot_h__
