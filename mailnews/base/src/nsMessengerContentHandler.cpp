/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMessengerContentHandler.h"
#include "nsIChannel.h"
#include "nsCRT.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsIWindowWatcher.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

nsMessengerContentHandler::nsMessengerContentHandler()
{
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS1(nsMessengerContentHandler, nsIContentHandler)

nsMessengerContentHandler::~nsMessengerContentHandler()
{
}

NS_IMETHODIMP nsMessengerContentHandler::HandleContent(const char * aContentType,
                                                nsIInterfaceRequestor* aWindowContext, nsIRequest *request)
{
  nsresult rv = NS_OK;
  if (!request)
    return NS_ERROR_NULL_POINTER;

  // First of all, get the content type and make sure it is a content type we know how to handle!
  if (nsCRT::strcasecmp(aContentType, "application/x-message-display") == 0) {
    nsCOMPtr<nsIURI> aUri;
    nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
    if (!aChannel) return NS_ERROR_FAILURE;

    rv = aChannel->GetURI(getter_AddRefs(aUri));
    if (aUri)
    {
      rv = request->Cancel(NS_ERROR_ABORT);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIURL> aUrl = do_QueryInterface(aUri);
        if (aUrl)
        {
          nsCAutoString queryPart;
          aUrl->GetQuery(queryPart);
          queryPart.ReplaceSubstring("type=message/rfc822", "type=application/x-message-display");
          aUrl->SetQuery(queryPart);
          rv = OpenWindow(aUri);
        }
      }
    }
  }

  return rv;
}

// Utility function to open a message display window and and load the message in it.
nsresult nsMessengerContentHandler::OpenWindow(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  
  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");
  if (!wwatch)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> newWindow;
  return wwatch->OpenWindow(0, "chrome://messenger/content/messageWindow.xul",
                 "_blank", "all,chrome,dialog=no,status,toolbar", aURI,
                 getter_AddRefs(newWindow));
}
