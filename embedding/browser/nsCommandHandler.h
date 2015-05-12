/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCOMMANDHANDLER_H
#define NSCOMMANDHANDLER_H

#include "nsISupports.h"
#include "nsICommandHandler.h"
#include "nsIDOMWindow.h"

class nsCommandHandler
  : public nsICommandHandlerInit
  , public nsICommandHandler
{
public:
  nsCommandHandler();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDHANDLERINIT
  NS_DECL_NSICOMMANDHANDLER

protected:
  virtual ~nsCommandHandler();

private:
  nsresult GetCommandHandler(nsICommandHandler** aCommandHandler);

  nsIDOMWindow* mWindow;
};

#endif
