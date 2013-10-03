/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// XXX Remove me soon!
[PrefControlled]
interface WorkerMessagePort : EventTarget {
    [Throws]
    void postMessage(any message, optional sequence<any> transferable);

    void start();

    void close();

    [SetterThrows=Workers, GetterThrows=Workers]
    attribute EventHandler onmessage;
};
