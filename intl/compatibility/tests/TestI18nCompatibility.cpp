/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include <iostream.h>
#include "nsIComponentManager.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
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
      char *tempCstr = tempStr.ToNewCString();

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
