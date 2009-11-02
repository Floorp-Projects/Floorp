/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
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
 * The Original Code is Value-Profiling Utility.
 *
 * The Initial Developer of the Original Code is
 * Intel Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mohammad R. Haghighat [mohammad.r.haghighat@intel.com] 
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

#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "vprof.h"

static void cProbe (void* vprofID)
{
    if (_VAL == _IVAR1) _I64VAR1 ++;
    _IVAR1 = _IVAR0;

    if (_VAL == _IVAR0) _I64VAR0 ++;
    _IVAR0 = (int) _VAL;

    _DVAR0 = ((double)_I64VAR0) / _COUNT;
    _DVAR1 = ((double)_I64VAR1) / _COUNT;
}

//__declspec (thread) boolean cv;
//#define if(c) cv = (c); _vprof (cv); if (cv)
//#define if(c) cv = (c); _vprof (cv, cProbe); if (cv)

#define THREADS 1
#define COUNT 100000
#define SLEEPTIME 0

static int64_t evens = 0;
static int64_t odds = 0;

void sub(int val)
{
    int i;
    //_vprof (1);
    for (i = 0; i < COUNT; i++) {
        //_nvprof ("Iteration", 1);
        //_nvprof ("Iteration", 1);
        _vprof (i);
        //_vprof (i);
        //_hprof(i, 3, (int64_t) 1000, (int64_t)2000, (int64_t)3000);
        //_hprof(i, 3, 10000, 10001, 3000000);
        //_nhprof("Event", i, 3, 10000, 10001, 3000000);
        //_nhprof("Event", i, 3, 10000, 10001, 3000000);
        //Sleep(SLEEPTIME);
        if (i % 2 == 0) {
            //_vprof (i);
            ////_hprof(i, 3, 10000, 10001, 3000000);
            //_nvprof ("Iteration", i);
            evens ++;
        } else {
            //_vprof (1);
            _vprof (i, cProbe);
            odds ++;
        }
        //_nvprof ("Iterate", 1);
    }
    //printf("sub %d done.\n", val);
}

HANDLE array[THREADS];

static int run (void)
{
    int i;
    
    time_t start_time = time(0);
    
    for (i = 0; i < THREADS; i++) {
        array[i] = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)sub, (LPVOID)i, 0, 0);
    }
    
    for (i = 0; i < THREADS; i++) {
        WaitForSingleObject(array[i], INFINITE);
    }

    return 0;
}

int main ()
{
    DWORD start, end;

    start = GetTickCount ();
    run ();
    end = GetTickCount ();

    printf ("\nRun took %d msecs\n\n", end-start);
}
