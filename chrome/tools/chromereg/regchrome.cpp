/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 * 
 */

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIChromeRegistry.h"

int main(int argc, char **argv)
{
  NS_InitXPCOM2(nsnull, nsnull, nsnull);

  nsCOMPtr <nsIChromeRegistry> chromeReg = 
    do_GetService("@mozilla.org/chrome/chrome-registry;1");
  if (!chromeReg) {
    NS_WARNING("chrome check couldn't get the chrome registry");
    return NS_ERROR_FAILURE;
  }
  chromeReg->CheckForNewChrome();
  // release the chrome registry before we shutdown XPCOM
  chromeReg = 0;
  NS_ShutdownXPCOM(nsnull);
  return 0;
}
