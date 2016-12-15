/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org/#promises
 */

// TODO We use object instead Function.  There is an open issue on WebIDL to
// have different types for "platform-provided function" and "user-provided
// function"; for now, we just use "object".
callback PromiseInit = void (object resolve, object reject);

callback PromiseJobCallback = void();

[TreatNonCallableAsNull]
callback AnyCallback = any (any value);

// Promises are implemented in SpiderMonkey; just define a tiny interface to make
// codegen of Promise arguments and return values work.
[NoInterfaceObject,
 Exposed=(Window,Worker,WorkerDebugger,System)]
// Need to escape "Promise" so it's treated as an identifier.
interface _Promise {
};

// Hack to allow us to have JS owning and properly tracing/CCing/etc a
// PromiseNativeHandler.
[NoInterfaceObject,
 Exposed=(Window,Worker,System)]
interface PromiseNativeHandler {
};
