/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


/*

  A stack-based lock object that makes using PRMontior a bit more
  convenient. It acquires the monitor when constructed, and releases
  it when it goes out of scope.

  For example,

    class Foo {
    private:
        PRLock* mLock;

    public:
        Foo(void) {
            mLock = PR_NewLock();
        }

        virtual ~Foo(void) {
            PR_DestroyLock(mLock);
        }

        void ThreadSafeMethod(void) {
            // we're don't hold the monitor yet...

            nsAutoLock lock(mLock);
            // ...but now we do.

            // we even can do wacky stuff like return from arbitrary places w/o
            // worrying about forgetting to release the monitor
            if (some_weird_condition)
                return;

            // otherwise do some other stuff
        }

        void ThreadSafeBlockScope(void) {
            // we're not in the monitor here...

            {
                nsAutoLock lock(mLock);
                // but we are now, at least until the block scope closes
            }

            // ...now we're not in the monitor anymore
        }
    };

 */

#ifndef nsAutoLock_h__
#define nsAutoLock_h__

#include "prlock.h"

// If you ever decide that you need to add a non-inline method to this
// class, be sure to change the class declaration to "class NS_BASE
// nsAutoLock".

class nsAutoLock {
private:
    PRLock* mLock;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an nsAutoLock object incorrectly.
    nsAutoLock(void) {}
    nsAutoLock(nsAutoLock& aLock) {}
    nsAutoLock& operator =(nsAutoLock& aLock) {
        return *this;
    }

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an nsAutoLock object on the heap.
    static void* operator new(size_t size) {
        return nsnull;
    }
    static void operator delete(void* memory) {}

public:
    nsAutoLock(PRLock* aLock) : mLock(aLock) {
        PR_ASSERT(mLock);

        // This will assert deep in the bowels of NSPR if you attempt
        // to re-enter the lock.
        PR_Lock(mLock);
    }


    ~nsAutoLock(void) {
        PR_Unlock(mLock);
    }
};


#endif // nsAutoLock_h__

