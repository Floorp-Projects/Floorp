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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// This file contains implementations of factories for various
// downloading-related interfaces.

#import "CHDownloadProgressDisplay.h"
#import "ProgressDlgController.h"

#include "nsCOMPtr.h"
#include "nsIFactory.h"

#include "nsDownloadListener.h"
#include "CHDownloadFactories.h"

// factory for nsIDownload objects
// XXX replace with generic factory stuff
class DownloadListenerFactory : public nsIFactory
{
public:
              DownloadListenerFactory();
  virtual     ~DownloadListenerFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

protected:

  DownloadControllerFactory*  mControllerFactory;  // factory which creates the Cocoa window controller
};


DownloadListenerFactory::DownloadListenerFactory()
: mControllerFactory(nil)
{
  mControllerFactory = [[ChimeraDownloadControllerFactory alloc] init];
}

DownloadListenerFactory::~DownloadListenerFactory()
{
  [mControllerFactory release];
}

NS_IMPL_ISUPPORTS1(DownloadListenerFactory, nsIFactory)

/* void createInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); */
NS_IMETHODIMP
DownloadListenerFactory::CreateInstance(nsISupports *aOuter, const nsIID& aIID, void* *aResult)
{
  nsresult rv;
  
  if (aIID.Equals(NS_GET_IID(nsIDownload)) || 
      aIID.Equals(NS_GET_IID(nsITransfer)) ||
      aIID.Equals(NS_GET_IID(nsISupports)))
  {
    nsDownloadListener* downloadListener = new nsDownloadListener(mControllerFactory);
    if (!downloadListener) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(downloadListener);
    rv = downloadListener->QueryInterface(aIID, aResult);
    NS_RELEASE(downloadListener);
    return rv;
  }

  return NS_ERROR_NO_INTERFACE;
}

/* void lockFactory (in PRBool lock); */
NS_IMETHODIMP
DownloadListenerFactory::LockFactory(PRBool lock)
{
  return NS_OK;
}

#pragma mark -

nsresult NewDownloadListenerFactory(nsIFactory* *outFactory)
{
  DownloadListenerFactory* newFactory = new DownloadListenerFactory();
  if (!newFactory) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(newFactory);
  nsresult rv = newFactory->QueryInterface(NS_GET_IID(nsIFactory), (void **)outFactory);
  NS_RELEASE(newFactory);
  return rv;
}

