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
#include "nsIWordBreakerFactory.h"
#include "nsIWordBreaker.h"
#include "nsIBreakState.h"
#include "nsLWBrkCIID.h"

#define WORK_AROUND_SERVICE_MANAGER_ASSERT

IMPL_NS_IBREAKSTATE( nsBreakState )

NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);
NS_DEFINE_IID(kILineBreakerFactory, NS_ILINEBREAKERFACTORY_IID);
NS_DEFINE_IID(kIWordBreakerFactory, NS_IWORDBREAKERFACTORY_IID);


static char teng1[] = 
//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
 "This is a test to test(reasonable) line    break. This 0.01123 = 45 x 48.";

static PRUint32 exp1[] = {
  4,5,7,8,9,10,14,15,17,18,22,34,35,39,40,41,42,43,49,50,54,55,62,63,64,65,
  67,68,69,70
};

static PRUint32 wexp1[] = {

  4,5,7,8,9,10,14,15,17,18,22,23,33,34,35,39,43,48,49,50,54,55,56,57,62,63,
  64,65,67,68,69,70,72
};
//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
static char teng2[] = 
 "()((reasonab(l)e) line  break. .01123=45x48.";

static PRUint32 exp2[] = {
  2,12,15,17,18,22,23,24,30,31,37,38,
};
static PRUint32 wexp2[] = {
  4,12,13,14,15,16,17,18,22,24,29,30,31,32,37,38,43
};

//          1         2         3         4         5         6         7
//01234567890123456789012345678901234567890123456789012345678901234567890123456789
static char teng3[] = 
 "It's a test to test(ronae ) line break....";
static PRUint32 exp3[] = {
  4, 5, 6,7,11,12,14,15,19,25,27,28,32,33
};
static PRUint32 wexp3[] = {
  4,5,6,7,11,12,14,15,19,20,25,26,27,28,32,33,38
};

static char ruler1[] =
"          1         2         3         4         5         6         7  ";
static char ruler2[] =
"0123456789012345678901234567890123456789012345678901234567890123456789012";


PRBool TestASCIILB(nsILineBreaker *lb,
                 const char* in, const PRUint32 len, 
                 const PRUint32* out, PRUint32 outlen)
{
         nsAutoString eng1(in);
         nsBreakState bk(eng1.GetUnicode(), eng1.Length());
         PRUint32 i,j;
         PRUint32 res[256];
         PRBool ok = PR_TRUE;
         for(i = 0, lb->FirstForwardBreak(&bk);
                    (! bk.IsDone()) && (i < 256);
                    lb->NextForwardBreak(&bk), i++)
         {
            res [i] = bk.Current();
         }
         if (i != outlen)
         {
            ok = PR_FALSE;
            cout << "WARNING!!! return size wrong, expect " << outlen <<
                    " bet got " << i << "\n";
         }
         cout << "string  = \n" << in << "\n";
         cout << ruler1 << "\n";
         cout << ruler2 << "\n";
         cout << "Expect = \n";
         for(j=0;j<outlen;j++)
         {
            cout << out[j] << ",";
         }
         cout << "\nResult = \n";
         for(j=0;j<i;j++)
         {
            cout << res[j] << ",";
         }
         cout << "\n";
         for(j=0;j<i;j++)
         {
            if(j < outlen)
            {
                if (res[j] != out[j])
                {
                   ok = PR_FALSE;
                   cout << "[" << j << "] expect " << out[j] << " but got " <<
                      res[j] << "\n";
                }
            } else {
                   ok = PR_FALSE;
                   cout << "[" << j << "] additional " <<
                      res[j] << "\n";
   
            }
         }
         return ok;
}

PRBool TestASCIIWB(nsIWordBreaker *lb,
                 const char* in, const PRUint32 len, 
                 const PRUint32* out, PRUint32 outlen)
{
         nsAutoString eng1(in);
         nsBreakState bk(eng1.GetUnicode(), eng1.Length());
         PRUint32 i,j;
         PRUint32 res[256];
         PRBool ok = PR_TRUE;
         for(i = 0, lb->FirstForwardBreak(&bk);
                    (! bk.IsDone()) && (i < 256);
                    lb->NextForwardBreak(&bk), i++)
         {
            res [i] = bk.Current();
         }
         if (i != outlen)
         {
            ok = PR_FALSE;
            cout << "WARNING!!! return size wrong, expect " << outlen <<
                    " bet got " << i << "\n";
         }
         cout << "string  = \n" << in << "\n";
         cout << ruler1 << "\n";
         cout << ruler2 << "\n";
         cout << "Expect = \n";
         for(j=0;j<outlen;j++)
         {
            cout << out[j] << ",";
         }
         cout << "\nResult = \n";
         for(j=0;j<i;j++)
         {
            cout << res[j] << ",";
         }
         cout << "\n";
         for(j=0;j<i;j++)
         {
            if(j < outlen)
            {
                if (res[j] != out[j])
                {
                   ok = PR_FALSE;
                   cout << "[" << j << "] expect " << out[j] << " but got " <<
                      res[j] << "\n";
                }
            } else {
                   ok = PR_FALSE;
                   cout << "[" << j << "] additional " <<
                      res[j] << "\n";
   
            }
         }
         return ok;
}
     
     
PRBool TestLineBreaker()
{
   cout << "==================================\n";
   cout << "Finish nsILineBreakerFactory Test \n";
   cout << "==================================\n";
   nsILineBreakerFactory *t = NULL;
   nsresult res;
   PRBool ok = PR_TRUE;
   res = nsServiceManager::GetService(kLWBrkCID,
                                kILineBreakerFactory,
                                (nsISupports**) &t);
           
   cout << "Test 1 - GetService():\n";
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t1st GetService failed\n";
     ok = PR_FALSE;
   } else {
#ifdef WORD_AROUND_SERVICE_MANAGER_ASSERT
     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
#endif
   }

   res = nsServiceManager::GetService(kLWBrkCID,
                                kILineBreakerFactory,
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t2nd GetService failed\n";
     ok = PR_FALSE;
   } else {

     cout << "Test 3 - GetLineBreaker():\n";
     nsILineBreaker *lb;

     nsAutoString lb_arg("");
     res = t->GetBreaker(lb_arg, &lb);
     if(NS_FAILED(res) || (lb == NULL)) {
         cout << "GetBreaker(nsILineBreaker*) failed\n";
         ok = PR_FALSE;
     } else {
         
         cout << "Test 4 - {First,Next}ForwardBreak():\n";
         if( TestASCIILB(lb, teng1, sizeof(teng1)/sizeof(char), 
                   exp1, sizeof(exp1)/sizeof(PRUint32)) )
         {
           cout << "Test 4 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 4 Failed\n\n";
         }

         cout << "Test 5 - {First,Next}ForwardBreak():\n";
         if(TestASCIILB(lb, teng2, sizeof(teng2)/sizeof(char), 
                   exp2, sizeof(exp2)/sizeof(PRUint32)) )
         {
           cout << "Test 5 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 5 Failed\n\n";
         }

         cout << "Test 6 - {First,Next}ForwardBreak():\n";
         if(TestASCIILB(lb, teng3, sizeof(teng3)/sizeof(char), 
                   exp3, sizeof(exp3)/sizeof(PRUint32)) )
         {
           cout << "Test 6 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 6 Failed\n\n";
         }


         NS_IF_RELEASE(lb);
     }

#ifdef WORD_AROUND_SERVICE_MANAGER_ASSERT
     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
#endif
   }
   cout << "==================================\n";
   cout << "Finish nsILineBreakerFactory Test \n";
   cout << "==================================\n";

   return ok;
}

PRBool TestWordBreaker()
{
   cout << "==================================\n";
   cout << "Finish nsIWordBreakerFactory Test \n";
   cout << "==================================\n";
   nsIWordBreakerFactory *t = NULL;
   nsresult res;
   PRBool ok = PR_TRUE;
   res = nsServiceManager::GetService(kLWBrkCID,
                                kIWordBreakerFactory,
                                (nsISupports**) &t);
           
   cout << "Test 1 - GetService():\n";
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t1st GetService failed\n";
     ok = PR_FALSE;
   } else {
     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
   }

   res = nsServiceManager::GetService(kLWBrkCID,
                                kIWordBreakerFactory,
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t2nd GetService failed\n";
     ok = PR_FALSE;
   } else {

     cout << "Test 3 - GetWordBreaker():\n";
     nsIWordBreaker *lb;

     nsAutoString lb_arg("");
     res = t->GetBreaker(lb_arg, &lb);
     if(NS_FAILED(res) || (lb == NULL)) {
         cout << "GetBreaker(nsIWordBreaker*) failed\n";
         ok = PR_FALSE;
     } else {
         
         cout << "Test 4 - {First,Next}ForwardBreak():\n";
         if( TestASCIIWB(lb, teng1, sizeof(teng1)/sizeof(char), 
                   wexp1, sizeof(wexp1)/sizeof(PRUint32)) )
         {
           cout << "Test 4 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 4 Failed\n\n";
         }

         cout << "Test 5 - {First,Next}ForwardBreak():\n";
         if(TestASCIIWB(lb, teng2, sizeof(teng2)/sizeof(char), 
                   wexp2, sizeof(wexp2)/sizeof(PRUint32)) )
         {
           cout << "Test 5 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 5 Failed\n\n";
         }

         cout << "Test 6 - {First,Next}ForwardBreak():\n";
         if(TestASCIIWB(lb, teng3, sizeof(teng3)/sizeof(char), 
                   wexp3, sizeof(wexp3)/sizeof(PRUint32)) )
         {
           cout << "Test 6 Passed\n\n";
         } else {
           ok = PR_FALSE;
           cout << "Test 6 Failed\n\n";
         }


         NS_IF_RELEASE(lb);
     }

     res = nsServiceManager::ReleaseService(kLWBrkCID, t);
   }
   cout << "==================================\n";
   cout << "Finish nsIWordBreakerFactory Test \n";
   cout << "==================================\n";

   return ok;
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
   cout << "Test Line Break\n";

   PRBool ok ; 
   ok =TestWordBreaker();

   cout << "Test Word Break\n";
   ok =TestLineBreaker();

   // --------------------------------------------
   cout << "Finish All The Test Cases\n";
   nsresult res = NS_OK;
   res = nsRepository::FreeLibraries();

   if(NS_FAILED(res))
      cout << "nsRepository failed\n";
   else
      cout << "nsRepository FreeLibraries Done\n";
   if(ok)
      cout <<  "Line Break Test\nOK\n";
   else
      cout <<  "Line Break Test\nFailed\n";
   return 0;
}
