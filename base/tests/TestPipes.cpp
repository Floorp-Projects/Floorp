/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIThread.h"
#include "nsIByteBufferInputStream.h"
#include "prprf.h"
#include "prinrval.h"
#include "plstr.h"
#include <stdio.h>

#define KEY             0xa7
#define ITERATIONS      333
char kTestPattern[] = "My hovercraft is full of eels.\n";

class nsReceiver : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        char buf[101];
        PRUint32 count;
        PRIntervalTime start = PR_IntervalNow();
        while (PR_TRUE) {
            rv = mIn->Read(buf, 100, &count);
            if (rv == NS_BASE_STREAM_EOF) {
//                printf("EOF count = %d\n", mCount);
                rv = NS_OK;
                break;
            }
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }

            buf[count] = '\0';
//            printf(buf);

            mCount += count;
        }
        PRIntervalTime end = PR_IntervalNow();
        printf("read time = %dms\n", PR_IntervalToMilliseconds(end - start));
        return rv;
    }

    nsReceiver(nsIInputStream* in) : mIn(in), mCount(0) {
        NS_INIT_REFCNT();
        NS_ADDREF(in);
    }

    virtual ~nsReceiver() {
        NS_RELEASE(mIn);
    }

protected:
    nsIInputStream*     mIn;
    PRUint32            mCount;
};

NS_IMPL_ISUPPORTS(nsReceiver, nsIRunnable::GetIID());

int
main()
{
    nsresult rv;
    nsIInputStream* in;
    nsIOutputStream* out;

    rv = NS_NewPipe(&in, &out, PR_TRUE, 40);
    if (NS_FAILED(rv)) return -1;

    nsIThread* receiver;
    rv = NS_NewThread(&receiver, new nsReceiver(in));
    NS_RELEASE(in);

    PRIntervalTime start = PR_IntervalNow();
    for (PRUint32 i = 0; i < ITERATIONS; i++) {
        PRUint32 writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        rv = out->Write(buf, PL_strlen(buf), &writeCount);
        PR_smprintf_free(buf);
        NS_ASSERTION(NS_SUCCEEDED(rv), "write failed");
    }
    rv = out->Close();
    NS_ASSERTION(NS_SUCCEEDED(rv), "close failed");
    PRIntervalTime end = PR_IntervalNow();

    receiver->Join();
    NS_RELEASE(receiver);

    printf("write time = %dms\n", PR_IntervalToMilliseconds(end - start));
    return 0;
}
