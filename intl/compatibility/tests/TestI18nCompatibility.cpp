/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <iostream.h>
#include "nsIComponentManager.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsII18nCompatibility.h"


static NS_DEFINE_CID(kI18nCompatibilityCID, NS_I18NCOMPATIBILITY_CID);

#ifdef XP_PC
#define DLL_NAME "intlcmpt.dll"
#else
#ifdef XP_MAC
#define DLL_NAME "INTLCMPT_DLL"
#else
#endif
#define DLL_NAME "intlcmpt"MOZ_DLL_SUFFIX
#endif

extern "C" void NS_SetupRegistry()
{
  nsComponentManager::RegisterComponent(kI18nCompatibilityCID,  NULL, NULL, DLL_NAME, PR_FALSE, PR_FALSE);
}

#undef DLL_NAME


int main(int argc, char** argv) {
  nsresult rv;

  NS_SetupRegistry();

  nsCOMPtr <nsII18nCompatibility> I18nCompatibility;
  rv = nsComponentManager::CreateInstance(kI18nCompatibilityCID, NULL, 
                                          NS_GET_IID(nsII18nCompatibility), getter_AddRefs(I18nCompatibility));
  if (NS_SUCCEEDED(rv)) {
    PRUint16 csid = 0;
    PRUnichar *charsetUni = NULL;

    rv = I18nCompatibility->CSIDtoCharsetName(csid, &charsetUni);
    if (NS_SUCCEEDED(rv) && NULL != charsetUni) {
      nsString tempStr(charsetUni);
      char *tempCstr = ToNewCString(tempStr);

      nsMemory::Free(charsetUni);

      if (NULL != tempCstr) {
        cout << csid << " " << tempCstr << "\n";
        delete [] tempCstr;
      }
    }
  }
  else {
    cout << "error: CreateInstance\n";
  }

  cout << "done\n";

  return 0;
}
