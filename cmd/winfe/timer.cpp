/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"

#include "timer.h"
#include "feselect.h"
#include "cxprint.h"

// structure to keep track of FE_SetTimeOut objects
typedef struct WinTimeStruct {
    TimeoutCallbackFunction   fn;
    void                    * closure;
    DWORD                     dwFireTime;
    struct WinTimeStruct    * pNext;
} WinTime;

// the one and only list of objects waiting for FE_SetTimeOut
WinTime *gTimeOutList = NULL;

//  Process timeouts now.
//  Called once per round of fire.
UINT uTimeoutTimer = 0;
DWORD dwNextFire = (DWORD)-1;
DWORD dwSyncHack = 0;

void CALLBACK EXPORT FireTimeout(HWND hWnd, UINT uMessage, UINT uTimerID, DWORD dwTime)
{
    static BOOL bCanEnter = TRUE;

    //  Don't allow old timer messages in here.
    if(uMessage != WM_TIMER)    {
        ASSERT(0);
        return;
    }
    if(uTimerID != uTimeoutTimer)   {
        return;
    }

    //  Block only one entry into this function, or else.
    if(bCanEnter)   {
        bCanEnter = FALSE;
        // see if we need to fork off any timeout functions
        if(gTimeOutList)    {
            wfe_ProcessTimeouts(dwTime);
        }
        bCanEnter = TRUE;
    }
}

//  Function to correctly have the timer be set.
void SyncTimeoutPeriod(DWORD dwTickCount)
{
    //  May want us to set tick count ourselves.
    if(dwTickCount == 0)    {
        if(dwSyncHack == 0) {
            dwTickCount = GetTickCount();
        }
        else    {
            dwTickCount = dwSyncHack;
        }
    }

    //  If there's no window, we should clear the timer.
    if(NULL == theApp.m_pMainWnd || NULL == theApp.m_pMainWnd->m_hWnd)  {
        uTimeoutTimer = 0;
        dwNextFire = (DWORD)-1;
        theApp.m_bIdleProcessTimeouts = FALSE;
    }
    //  If there's no list, we should clear the timer.
    else if(!gTimeOutList)    {
        if(uTimeoutTimer)   {
            VERIFY(::KillTimer(theApp.m_pMainWnd->m_hWnd, 777));
            uTimeoutTimer = 0;
            dwNextFire = (DWORD)-1;
        }
        theApp.m_bIdleProcessTimeouts = FALSE;
    }
    else if(theApp.m_pMainWnd && theApp.m_pMainWnd->m_hWnd)   {
        //  See if we need to clear the current timer.
        //  Curcumstances are that if the timer will not
        //      fire on time for the next timeout.
        BOOL bSetTimer = FALSE;
        WinTime *pTimeout = gTimeOutList;
        if(uTimeoutTimer)   {
            if(pTimeout->dwFireTime != dwNextFire)   {
                //  There is no need to kill the timer if we are just going to set it again.
                //  Windows will simply respect the new time interval passed in.
                //  VERIFY(::KillTimer(theApp.m_pMainWnd->m_hWnd, 777));
                uTimeoutTimer = 0;
                dwNextFire = (DWORD)-1;

                //  Set the timer.
                bSetTimer = TRUE;
            }
        }
        else    {
            //  No timer set, attempt.
            bSetTimer = TRUE;
        }

        if(bSetTimer)   {
            DWORD dwFireWhen = pTimeout->dwFireTime > dwTickCount ?
                pTimeout->dwFireTime - dwTickCount : 0;
            if(dwFireWhen > UINT_MAX)   {
                dwFireWhen = UINT_MAX;
            }
            UINT uFireWhen = CASTUINT(dwFireWhen);

            ASSERT(uTimeoutTimer == 0);
            uTimeoutTimer = ::SetTimer(
                theApp.m_pMainWnd->m_hWnd, 
                777, 
                uFireWhen, 
                FireTimeout);

            if(uTimeoutTimer)   {
                //  Set the fire time.
                dwNextFire = pTimeout->dwFireTime;
            }
        }

        //  See if the app should attempt to bind idle processing to the
        //      timer code (in the event no timers could be allocated).
        if(uTimeoutTimer)   {
            theApp.m_bIdleProcessTimeouts = FALSE;
        }
        else    {
            ASSERT(0);
            theApp.m_bIdleProcessTimeouts = TRUE;
        }
    }
}

/* this function should register a function that will
 * be called after the specified interval of time has
 * elapsed.  This function should return an id 
 * that can be passed to FE_ClearTimeout to cancel 
 * the Timeout request.
 *
 * A) Timeouts never fail to trigger, and
 * B) Timeouts don't trigger *before* their nominal timestamp expires, and
 * C) Timeouts trigger in the same ordering as their timestamps
 *
 * After the function has been called it is unregistered
 * and will not be called again unless re-registered.
 *
 * func:    The function to be invoked upon expiration of
 *          the Timeout interval 
 * closure: Data to be passed as the only argument to "func"
 * msecs:   The number of milli-seconds in the interval
 */
PUBLIC void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    WinTime * pTime = new WinTime;
    if(!pTime)
        return(NULL);

    // fill it out
    DWORD dwNow = GetTickCount();
    pTime->fn = func;
    pTime->closure = closure;
    pTime->dwFireTime = (DWORD) msecs + dwNow;
    //CLM: Always clear this else the last timer added will have garbage!
    //     (Win16 revealed this bug!)
    pTime->pNext = NULL;
    
    // add it to the list
    if(!gTimeOutList) {        
        // no list add it
        gTimeOutList = pTime;
    } else {

        // is it before everything else on the list?
        if(pTime->dwFireTime < gTimeOutList->dwFireTime) {

            pTime->pNext = gTimeOutList;
            gTimeOutList = pTime;

        } else {

            WinTime * pPrev = gTimeOutList;
            WinTime * pCurrent = gTimeOutList;

            while(pCurrent && (pCurrent->dwFireTime <= pTime->dwFireTime)) {
                pPrev = pCurrent;
                pCurrent = pCurrent->pNext;
            }

            ASSERT(pPrev);

            // insert it after pPrev (this could be at the end of the list)
            pTime->pNext = pPrev->pNext;
            pPrev->pNext = pTime;

        }

    }

    //  Sync the timer fire period.
    SyncTimeoutPeriod(dwNow);

    return(pTime);
}


/* This function cancels a Timeout that has previously been
 * set.  
 * Callers should not pass in NULL or a timer_id that
 * has already expired.
 */
PUBLIC void 
FE_ClearTimeout(void * pStuff)
{
    WinTime * pTimer = (WinTime *) pStuff;

    if(!pTimer) {
        return;
    }

    if(gTimeOutList == pTimer) {

        // first element in the list lossage
        gTimeOutList = pTimer->pNext;

    } else {

        // walk until no next pointer
        for(WinTime * p = gTimeOutList; p && p->pNext && (p->pNext != pTimer); p = p->pNext)
            ;

        // if we found something valid pull it out of the list
        if(p && p->pNext && p->pNext == pTimer) {
            p->pNext = pTimer->pNext;

        } else {
            // get out before we delete something that looks bogus
            return;
        }

    }

    // if we got here it must have been a valid element so trash it
    delete pTimer;

    //  If there's now no be sure to clear the timer.
    SyncTimeoutPeriod(0);
}

//
// Walk down the timeout list and launch anyone appropriate
// Cleaned up logic 04-30-96 GAB
//
void wfe_ProcessTimeouts(DWORD dwNow)
{
    WinTime *p = gTimeOutList;
    if(dwNow == 0)   {
        dwNow = GetTickCount();
    }

    BOOL bCalledSync = FALSE;

#ifndef MOZ_NGLAYOUT
    //  Don't fire timeouts while in the PrintAbortProc, or we will go
    //      reentrant into the GDI code of the print driver if
    //      someone is doing drawing via timeouts.
    if(CPrintCX::m_bGlobalBlockDisplay) {
        //  Be sure to get the next timeout period right, however.
        SyncTimeoutPeriod(dwNow);
        bCalledSync = TRUE;
        
        //  Get Out
        return;
    }
#endif /* MOZ_NGLAYOUT */

    //  Set the hack, such that when FE_ClearTimeout
    //      calls SyncTimeoutPeriod, that GetTickCount()
    //      overhead is not incurred.
    dwSyncHack = dwNow;
    
    // loop over all entries
    while(p) {
        // send it
        if(p->dwFireTime < dwNow) {
            //  Fire it.
            (*p->fn) (p->closure);

            //  Clear the timer.
            //  Period synced.
            FE_ClearTimeout(p);
            bCalledSync = TRUE;

            //  Reset the loop (can't look at p->pNext now, and called
            //      code may have added/cleared timers).
            //  (could do this by going recursive and returning).
            p = gTimeOutList;
        } else {
			//	Make sure we fire an timer.
            //  Also, we need to check to see if things are backing up (they
            //      may be asking to be fired long before we ever get to them,
            //      and we don't want to pass in negative values to the real
            //      timer code, or it takes days to fire....
            if(bCalledSync == FALSE)    {
                SyncTimeoutPeriod(dwNow);
                bCalledSync = TRUE;
            }
            //  Get next timer.
            p = p->pNext;
        }
    }
    dwSyncHack = 0;
}
