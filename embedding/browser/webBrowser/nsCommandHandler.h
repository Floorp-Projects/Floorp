/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 */

#ifndef NSCOMMANDHANDLER_H
#define NSCOMMANDHANDLER_H

#include "nsISupports.h"
#include "nsICommandHandler.h"
#include "nsIDOMWindow.h"

class nsCommandHandler :
    public nsICommandHandlerInit,
    public nsICommandHandler
{
    nsIDOMWindow *mWindow;

    nsresult GetCommandHandler(nsICommandHandler **aCommandHandler);

public:
    nsCommandHandler();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMMANDHANDLERINIT
    NS_DECL_NSICOMMANDHANDLER

protected:
    virtual ~nsCommandHandler();
};

#endif

