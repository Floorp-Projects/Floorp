/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCommandHandler.h"
#include "nsWebBrowser.h"
#include "nsDocShellTreeOwner.h"

#include "nsIAllocator.h"
#include "nsPIDOMWindow.h"

nsCommandHandler::nsCommandHandler() :
    mWindow(nullptr)
{
}

nsCommandHandler::~nsCommandHandler()
{
}

nsresult nsCommandHandler::GetCommandHandler(nsICommandHandler **aCommandHandler)
{
    NS_ENSURE_ARG_POINTER(aCommandHandler);

    *aCommandHandler = nullptr;
    if (mWindow == nullptr)
    {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mWindow));
    if (!window)
    {
        return NS_ERROR_FAILURE;
    }

    // Get the document tree owner

    nsCOMPtr<nsIDocShellTreeItem> docShellAsTreeItem =
      do_QueryInterface(window->GetDocShell());
    nsIDocShellTreeOwner *treeOwner = nullptr;
    docShellAsTreeItem->GetTreeOwner(&treeOwner);

    // Make sure the tree owner is an an nsDocShellTreeOwner object
    // by QI'ing for a hidden interface. If it doesn't have the interface
    // then it's not safe to do the casting.

    nsCOMPtr<nsICDocShellTreeOwner> realTreeOwner(do_QueryInterface(treeOwner));
    if (realTreeOwner)
    {
        nsDocShellTreeOwner *tree = static_cast<nsDocShellTreeOwner *>(treeOwner);
        if (tree->mTreeOwner)
        {
            nsresult rv;
            rv = tree->mTreeOwner->QueryInterface(NS_GET_IID(nsICommandHandler), (void **)aCommandHandler);
            NS_RELEASE(treeOwner);
            return rv;
        }
     
        NS_RELEASE(treeOwner);
     }

    *aCommandHandler = nullptr;

    return NS_OK;
}


NS_IMPL_ADDREF(nsCommandHandler)
NS_IMPL_RELEASE(nsCommandHandler)

NS_INTERFACE_MAP_BEGIN(nsCommandHandler)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICommandHandler)
    NS_INTERFACE_MAP_ENTRY(nsICommandHandlerInit)
    NS_INTERFACE_MAP_ENTRY(nsICommandHandler)
NS_INTERFACE_MAP_END

///////////////////////////////////////////////////////////////////////////////
// nsICommandHandlerInit implementation

/* attribute nsIDocShell docShell; */
NS_IMETHODIMP nsCommandHandler::GetWindow(nsIDOMWindow * *aWindow)
{
    *aWindow = nullptr;
    return NS_OK;
}

NS_IMETHODIMP nsCommandHandler::SetWindow(nsIDOMWindow * aWindow)
{
    if (aWindow == nullptr)
    {
       return NS_ERROR_FAILURE;
    }
    mWindow = aWindow;
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsICommandHandler implementation

/* string exec (in string aCommand, in string aStatus); */
NS_IMETHODIMP nsCommandHandler::Exec(const char *aCommand, const char *aStatus, char **aResult)
{
    NS_ENSURE_ARG_POINTER(aCommand);
    NS_ENSURE_ARG_POINTER(aResult);

    nsCOMPtr<nsICommandHandler> commandHandler;
    GetCommandHandler(getter_AddRefs(commandHandler));

    // Call the client's command handler to deal with this command
    if (commandHandler)
    {
        *aResult = nullptr;
        return commandHandler->Exec(aCommand, aStatus, aResult);
    }

    // Return an empty string
    const char szEmpty[] = "";
    *aResult = (char *) nsAllocator::Clone(szEmpty, sizeof(szEmpty));

    return NS_OK;
}

/* string query (in string aCommand, in string aStatus); */
NS_IMETHODIMP nsCommandHandler::Query(const char *aCommand, const char *aStatus, char **aResult)
{
    NS_ENSURE_ARG_POINTER(aCommand);
    NS_ENSURE_ARG_POINTER(aResult);

    nsCOMPtr<nsICommandHandler> commandHandler;
    GetCommandHandler(getter_AddRefs(commandHandler));

    // Call the client's command handler to deal with this command
    if (commandHandler)
    {
        *aResult = nullptr;
        return commandHandler->Query(aCommand, aStatus, aResult);
    }

    // Return an empty string
    const char szEmpty[] = "";
    *aResult = (char *) nsAllocator::Clone(szEmpty, sizeof(szEmpty));

    return NS_OK;
}
