/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testSyncImportBeforeAsyncImportDependencyInWorker() {
  const worker = new ChromeWorker("resource://test/data/base_uri_worker.js");

  const { promise, resolve } = Promise.withResolvers();
  worker.onmessage = event => {
    resolve(event.data);
  };
  worker.postMessage("");

  const result = await promise;

  Assert.ok(result.scriptToModule.equal1);
  Assert.ok(result.scriptToModule.equal2);
  Assert.ok(result.scriptToModule.equal3);

  Assert.ok(result.moduleToModuleURI.equal1);
  Assert.ok(result.moduleToModuleURI.equal2);
  Assert.ok(result.moduleToModuleURI.equal3);

  Assert.ok(result.moduleToModuleCurrent.equal1);
  Assert.ok(result.moduleToModuleCurrent.equal2);
  Assert.ok(result.moduleToModuleCurrent.equal3);

  Assert.ok(result.moduleToModuleParent.equal1);
  Assert.ok(result.moduleToModuleParent.equal2);
  Assert.ok(result.moduleToModuleParent.equal3);

  Assert.ok(result.moduleToModuleAbsolute.equal1);
  Assert.ok(result.moduleToModuleAbsolute.equal2);
  Assert.ok(result.moduleToModuleAbsolute.equal3);
});
