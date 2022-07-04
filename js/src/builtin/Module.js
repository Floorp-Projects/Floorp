/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://tc39.es/proposal-top-level-await/#sec-gather-async-parent-completions
function GatherAsyncParentCompletions(module, execList = new_List()) {
  assert(module.status == MODULE_STATUS_EVALUATED, "bad status for async module");

  // Step 5.
  // asyncParentModules is a list, and doesn't have a .length. Might be worth changing
  // later on.
  let i = 0;
  while (module.asyncParentModules[i]) {
    const m = module.asyncParentModules[i];
    if (GetCycleRoot(m).status != MODULE_STATUS_EVALUATED_ERROR &&
        !callFunction(std_Array_includes, execList, m)) {
      assert(!m.evaluationError, "should not have evaluation error");
      assert(m.pendingAsyncDependencies > 0, "should have at least one dependency");
      UnsafeSetReservedSlot(m,
                            MODULE_OBJECT_PENDING_ASYNC_DEPENDENCIES_SLOT,
                            m.pendingAsyncDependencies - 1);
      if (m.pendingAsyncDependencies === 0) {
        callFunction(std_Array_push, execList, m);
        if (!m.async) {
          execList = GatherAsyncParentCompletions(m, execList);
        }
      }
    }
    i++;
  }
  callFunction(ArraySort,
               execList,
               (a, b) => a.asyncEvaluatingPostOrder - b.asyncEvaluatingPostOrder);
  return execList
}
