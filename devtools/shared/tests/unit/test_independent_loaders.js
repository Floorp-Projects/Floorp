/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that each instance of the Dev Tools loader contains its own loader
 * instance, and also returns unique objects.  This ensures there is no sharing
 * in place between loaders.
 */
function run_test() {
  const loader1 = new DevToolsLoader();
  const loader2 = new DevToolsLoader();

  const indent1 = loader1.require("devtools/shared/indentation");
  const indent2 = loader2.require("devtools/shared/indentation");

  Assert.ok(indent1 !== indent2);

  Assert.ok(loader1._provider !== loader2._provider);
  Assert.ok(loader1._provider.loader !== loader2._provider.loader);
}
