/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testInWorker() {
  const worker = new ChromeWorker("resource://test/contextual_worker.js");
  const { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage("");

  const result = await promise;

  Assert.ok(result.equal1);
  Assert.ok(result.equal2);
});
