/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
                                          nsII18nCompatibility::GetIID(), getter_AddRefs(I18nCompatibility));
  if (NS_SUCCEEDED(rv)) {
    PRUint16 csid = 0;
    PRUnichar *charsetUni = NULL;

    rv = I18nCompatibility->CSIDtoCharsetName(csid, &charsetUni);
    if (NS_SUCCEEDED(rv) && NULL != charsetUni) {
      nsString tempStr(charsetUni);
      char *tempCstr = tempStr.ToNewCString();

      nsAllocator::Free(charsetUni);

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
