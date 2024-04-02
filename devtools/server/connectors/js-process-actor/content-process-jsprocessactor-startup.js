/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
  We want this to only startup the DevToolsProcess JS Actor on process start
  and not when we only register the JS Process Actor when watching the first target type.
  The Watcher Actor will query each individual JS Process Actor and fine control
  the ordering of requests. It is especially important to spawn the top level target first.
*/
const isContentProcessStartup = !Services.ww
  .getWindowEnumerator()
  .hasMoreElements();
if (isContentProcessStartup) {
  /*
     We can't spawn the JSProcessActor right away and have to spin the event loop.
     Otherwise it isn't registered yet and isn't listening to observer service.
     Could it be the reason why JSProcessActor aren't spawn via process actor option's child.observers notifications ??
  */
  Services.tm.dispatchToMainThread(() => {
    /*
      This notification is registered in DevToolsServiceWorker JS process actor's options's `observers` attribute
      and will force the JS Process actor to be instantiated in all processes.
    */
    Services.obs.notifyObservers(null, "init-devtools-content-process-actor");
    /*
      Instead of using observer service, we could also manually call some method of the actor:
      ChromeUtils.domProcessChild.getActor("DevToolsProcess").observe(null, "init-devtools-content-process-actor");
    */
  });
}
