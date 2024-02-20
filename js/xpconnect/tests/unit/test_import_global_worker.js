/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testSharedInWorker() {
  // Loading into shared global isn't allowed in worker.

  let worker = new ChromeWorker("resource://test/import_shared_in_worker.js");
  let { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage("");

  const result = await promise;

  Assert.equal(result.caught1, true);
  Assert.equal(result.caught2, true);
  Assert.equal(result.caught3, true);
});
