/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is JavaScript shell workers.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
        void terminateAll(JSRuntime *rt, ThreadPool *tp);

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
