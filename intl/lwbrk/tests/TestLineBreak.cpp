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
#include "nsRepository.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsILineBreakerFactory.h"
#include "nsILineBreaker.h"
#include "nsIBreakState.h"
#include "nsLWBrkCIID.h"


IMPL_NS_IBREAKSTATE( nsBreakState )

NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);
NS_DEFINE_IID(kILineBreakerFactory, NS_ILINEBREAKERFACTORY_IID);


static char teng1[] = 
//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
 "This is a test to test(reasonable) line    break. This 0.01123 = 45 x 48.";

static PRUint32 exp1[] = {
  4,5,7,8,9,10,14,15,17,18,22,34,35,39,40,41,42,43,49,50,54,55,62,63,64,65,
  67,68,69
};

//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
static char teng2[] = 
 "()((reasonab(l)e) line  break. .01123=45x48.";

static PRUint32 exp2[] = {
  2,12,15,17,18,22,23,24,30,31,37,38,40,41
};

//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
static char teng3[] = 
 "It's a test to test(ronae ) line break....";
static PRUint32 exp3[] = {
  4, 5, 6,7,11,12,14,15,25,26,27,28,32,33
};

static char ruler1[] =
"          1         2         3         4         5         6         7  ";
static char ruler2[] =
"0123456789012345678901234567890123456789012345678901234567890123456789012";
void TestLineBreaker()
{
   cout << "==================================\n";
   cout << "Finish nsILineBreakerFactory Test \n";
   cout << "==================================\n";
   nsILineBreakerFactory *t = NULL;
   nsresult res;
   res = nsServiceManager::GetService(kLWBrkCID,
                                kILineBreakerFactory,
                                (nsISupports**) &t);
           
   cout << "Test 1 - GetService():\n";
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t1st GetService failed\n";
   } else {
     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
   }

   res = nsServiceManager::GetService(kLWBrkCID,
                                kILineBreakerFactory,
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t2nd GetService failed\n";
   } else {

     cout << "Test 3 - GetLineBreaker():\n";
     nsILineBreaker *lb;

     nsAutoString lb_arg("");
     res = t->GetBreaker(lb_arg, &lb);
     if(NS_FAILED(res) || (lb == NULL)) {
         cout << "GetBreaker(nsILineBreaker*) failed\n";
     } else {
         
         cout << "Test 4 - {First,Next}ForwardBreak():\n";
         PRUint32 expect = sizeof(exp1)/sizeof(char);

         nsAutoString eng1(teng1);
         nsBreakState bk(eng1.GetUnicode(), eng1.Length());
         PRUint32 i,j;
         PRUint32 res[256];
         for(i = 0, lb->FirstForwardBreak(&bk);
                    (! bk.IsDone()) && (i < 256);
                    lb->NextForwardBreak(&bk), i++)
         {
            res [i] = bk.Current();
         }
         if (i != expect)
         {
            cout << "WARNING!!! return size wrong, expect " << expect <<
                    " bet got " << i << "\n";
         }
         cout << "string  = \n" << teng1 << "\n";
         cout << ruler1 << "\n";
         cout << ruler2 << "\n";
         for(j=0;j<i;j++)
         {
            cout << res[j] << ",";
         }
         for(j=0;j<i;j++)
         {
            if(j < expect)
            {
                if (res[j] != exp1[j])
                {
                   cout << "[" << j << "] expect " << exp1[j] << " but got " <<
                      res[j] << "\n";
                }
            } else {
                   cout << "[" << j << "] additional " <<
                      res[j] << "\n";
   
            }
         }
     
         NS_IF_RELEASE(lb);
     }

     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
   }
   cout << "==================================\n";
   cout << "Finish nsILineBreakerFactory Test \n";
   cout << "==================================\n";

}


#ifdef XP_PC
#define LWBRK_DLL "lwbrk.dll"
#else
#ifdef XP_MAC
#define LWBRK_DLL "LWBRK_DLL"
#else
#endif
#define LWBRK_DLL "liblwbrk.so"
#endif

extern "C" void NS_SetupRegistry()
{
   nsRepository::RegisterFactory(kLWBrkCID,  LWBRK_DLL, PR_FALSE, PR_FALSE);
}
 
int main(int argc, char** argv) {
   
   NS_SetupRegistry();

   // --------------------------------------------

   TestLineBreaker();

   // --------------------------------------------
   cout << "Finish All The Test Cases\n";
   nsresult res = NS_OK;
   res = nsRepository::FreeLibraries();

   if(NS_FAILED(res))
      cout << "nsRepository failed\n";
   else
      cout << "nsRepository FreeLibraries Done\n";
   return 0;
}
