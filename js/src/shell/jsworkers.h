/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsworkers_h___
#define jsworkers_h___

#ifdef JS_THREADSAFE

#include "jsapi.h"

/*
 * Workers for the JS shell.
 *
 * Note: The real implementation of DOM Workers is in dom/workers.
 */
namespace js {
    namespace workers {
        class ThreadPool;

        class WorkerHooks {
        public:
            virtual JSObject *newGlobalObject(JSContext *cx) = 0;
            virtual ~WorkerHooks() {}
        };

        /*
         * Initialize workers. This defines the Worker constructor on global.
         * Requires request. rootp must point to a GC root.
         *
         * On success, *rootp receives a pointer to an object, and init returns
         * a non-null value. The caller must keep the object rooted and must
         * pass it to js::workers::finish later.
         */
        ThreadPool *init(JSContext *cx, WorkerHooks *hooks, JSObject *global, JSObject **rootp);

        /* Asynchronously signal for all workers to terminate.
         *
         * Call this before calling finish() to shut down without waiting for
         * all messages to be proceesed.
         */
        void terminateAll(ThreadPool *tp);

	/*
	 * Finish running any workers, shut down the thread pool, and free all
	 * resources associated with workers. The application must call this
	 * before shutting down the runtime, and not during GC.
	 *
	 * Requires request.
	 */
	void finish(JSContext *cx, ThreadPool *tp);
    }
}

#endif /* JS_THREADSAFE */

#endif /* jsworkers_h___ */
