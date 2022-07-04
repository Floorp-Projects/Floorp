/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function CallModuleResolveHook(module, moduleRequest, expectedMinimumStatus)
{
    let requestedModule = HostResolveImportedModule(module, moduleRequest);
    if (requestedModule.status < expectedMinimumStatus)
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);

    return requestedModule;
}

function ModuleSetStatus(module, newStatus)
{
    assert(newStatus >= MODULE_STATUS_UNLINKED &&
           newStatus <= MODULE_STATUS_EVALUATED_ERROR,
           "Bad new module status in ModuleSetStatus");

    // Note that under OOM conditions we can fail the module instantiation
    // process even after modules have been marked as instantiated.
    assert((module.status <= MODULE_STATUS_LINKED &&
            newStatus === MODULE_STATUS_UNLINKED) ||
           newStatus > module.status,
           "New module status inconsistent with current status");

    UnsafeSetReservedSlot(module, MODULE_OBJECT_STATUS_SLOT, newStatus);
}

function CountArrayValues(array, value)
{
    let count = 0;
    for (let i = 0; i < array.length; i++) {
        if (array[i] === value)
            count++;
    }
    return count;
}

function ArrayContains(array, value)
{
    for (let i = 0; i < array.length; i++) {
        if (array[i] === value)
            return true;
    }
    return false;
}

function HandleModuleInstantiationFailure(module)
{
    // Reset the module to the "unlinked" state. Don't reset the
    // environment slot as the environment object will be required by any
    // possible future instantiation attempt.
    ModuleSetStatus(module, MODULE_STATUS_UNLINKED);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_INDEX_SLOT, undefined);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT, undefined);
}

// 15.2.1.16.4 ModuleInstantiate()
function ModuleInstantiate()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleInstantiate");

    // Step 1
    let module = this;

    // Step 2
    if (module.status === MODULE_STATUS_LINKING ||
        module.status === MODULE_STATUS_EVALUATING)
    {
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);
    }

    // Step 3
    let stack = new_List();

    // Steps 4-5
    try {
        InnerModuleLinking(module, stack, 0);
    } catch (error) {
        for (let i = 0; i < stack.length; i++) {
            let m = stack[i];
            if (m.status === MODULE_STATUS_LINKING) {
                HandleModuleInstantiationFailure(m);
            }
        }

        // Handle OOM when appending to the stack or over-recursion errors.
        if (stack.length === 0 && module.status === MODULE_STATUS_LINKING) {
            HandleModuleInstantiationFailure(module);
        }

        assert(module.status !== MODULE_STATUS_LINKING,
               "Expected unlinked status after failed linking");

        throw error;
    }

    // Step 6
    assert(module.status === MODULE_STATUS_LINKED ||
           module.status === MODULE_STATUS_EVALUATED ||
           module.status === MODULE_STATUS_EVALUATED_ERROR,
           "Bad module status after successful linking");

    // Step 7
    assert(stack.length === 0,
           "Stack should be empty after successful linking");

    // Step 8
    return undefined;
}

// https://tc39.es/ecma262/#sec-InnerModuleLinking
// ES2020 15.2.1.16.1.1 InnerModuleLinking
function InnerModuleLinking(module, stack, index)
{
    // Step 1
    // TODO: Support module records other than Cyclic Module Records.
    // 1. If module is not a Cyclic Module Record, then
    //     a. Perform ? module.Link().
    //     b. Return index.

    // Step 2
    if (module.status === MODULE_STATUS_LINKING ||
        module.status === MODULE_STATUS_LINKED ||
        module.status === MODULE_STATUS_EVALUATED ||
        module.status === MODULE_STATUS_EVALUATED_ERROR)
    {
        return index;
    }

    // Step 3. Assert: module.[[Status]] is unlinked.
    if (module.status !== MODULE_STATUS_UNLINKED)
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);

    // Step 4. Set module.[[Status]] to linking.
    ModuleSetStatus(module, MODULE_STATUS_LINKING);

    // Step 5. Set module.[[DFSIndex]] to index.
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_INDEX_SLOT, index);
    // Step 6. Set module.[[DFSAncestorIndex]] to index.
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT, index);
    // Step 7. Set index to index + 1.
    index++;

    // Step 8. Append module to stack.
    DefineDataProperty(stack, stack.length, module);

    // Step 9. For each String required that is an element of module.[[RequestedModules]], do
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        // Step 9.a
        let required = requestedModules[i].moduleRequest;
        let requiredModule = CallModuleResolveHook(module, required, MODULE_STATUS_UNLINKED);

        // Step 9.b
        index = InnerModuleLinking(requiredModule, stack, index);

        // TODO: Check if requiredModule is a Cyclic Module Record
        // Step 9.c.i
        assert(requiredModule.status === MODULE_STATUS_LINKING ||
               requiredModule.status === MODULE_STATUS_LINKED ||
               requiredModule.status === MODULE_STATUS_EVALUATED ||
               requiredModule.status === MODULE_STATUS_EVALUATED_ERROR,
               "Bad required module status after InnerModuleLinking");

        // Step 9.c.ii
        assert((requiredModule.status === MODULE_STATUS_LINKING) ===
               ArrayContains(stack, requiredModule),
              "Required module should be in the stack iff it is currently being instantiated");

        assert(typeof requiredModule.dfsIndex === "number", "Bad dfsIndex");
        assert(typeof requiredModule.dfsAncestorIndex === "number", "Bad dfsAncestorIndex");

        // Step 9.c.iii
        if (requiredModule.status === MODULE_STATUS_LINKING) {
            UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT,
                                  std_Math_min(module.dfsAncestorIndex,
                                               requiredModule.dfsAncestorIndex));
        }
    }

    // Step 10
    InitializeEnvironment(module);

    // Step 11
    assert(CountArrayValues(stack, module) === 1,
           "Current module should appear exactly once in the stack");
    // Step 12
    assert(module.dfsAncestorIndex <= module.dfsIndex,
           "Bad DFS ancestor index");

    // Step 13
    if (module.dfsAncestorIndex === module.dfsIndex) {
        let requiredModule;
        do {
            // 13.b.i-ii
            requiredModule = callFunction(std_Array_pop, stack);
            // TODO: 13.b.ii. Assert: requiredModule is a Cyclic Module Record.
            // Step 13.b.iv
            ModuleSetStatus(requiredModule, MODULE_STATUS_LINKED);
        } while (requiredModule !== module);
    }

    // Step 14
    return index;
}

function GetModuleEvaluationError(module)
{
    assert(IsObject(module) && IsModule(module),
           "Non-module passed to GetModuleEvaluationError");
    assert(module.status === MODULE_STATUS_EVALUATED_ERROR,
           "Bad module status in GetModuleEvaluationError");
    return UnsafeGetReservedSlot(module, MODULE_OBJECT_EVALUATION_ERROR_SLOT);
}

function RecordModuleEvaluationError(module, error)
{
    // Set the module's EvaluationError slot to indicate a failed module
    // evaluation.

    assert(IsObject(module) && IsModule(module),
           "Non-module passed to RecordModuleEvaluationError");

    if (module.status === MODULE_STATUS_EVALUATED_ERROR) {
        // It would be nice to assert that |error| is the same as the one we
        // previously recorded, but that's not always true in the case of out of
        // memory and over-recursion errors.
        return;
    }

    ModuleSetStatus(module, MODULE_STATUS_EVALUATED_ERROR);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_EVALUATION_ERROR_SLOT, error);
}

// https://tc39.es/ecma262/#sec-moduleevaluation
// ES2020 15.2.1.16.2 ModuleEvaluate
function ModuleEvaluate()
{
    if (!IsObject(this) || !IsModule(this))
        return callFunction(CallModuleMethodIfWrapped, this, "ModuleEvaluate");

    // Step 2
    let module = this;

    // Step 3
    if (module.status !== MODULE_STATUS_LINKED &&
        module.status !== MODULE_STATUS_EVALUATED &&
        module.status !== MODULE_STATUS_EVALUATED_ERROR)
    {
        ThrowInternalError(JSMSG_BAD_MODULE_STATUS);
    }

    // Top-level Await Step 4
    if (module.status === MODULE_STATUS_EVALUATED) {
      module = GetCycleRoot(module);
    }

    // Top-level Await Step 5
    if (module.topLevelCapability) {
      return module.topLevelCapability;
    }

    const capability = CreateTopLevelCapability(module);

    // Step 4
    let stack = new_List();

    // Steps 5-6
    try {
        InnerModuleEvaluation(module, stack, 0);
        if (!IsAsyncEvaluating(module)) {
          ModuleTopLevelCapabilityResolve(module);
        }
        // Steps 7-8
        assert(module.status === MODULE_STATUS_EVALUATED,
               "Bad module status after successful evaluation");
        assert(stack.length === 0,
               "Stack should be empty after successful evaluation");
    } catch (error) {
        for (let i = 0; i < stack.length; i++) {
            let m = stack[i];
            assert(m.status === MODULE_STATUS_EVALUATING,
                   "Bad module status after failed evaluation");
            RecordModuleEvaluationError(m, error);
        }

        // Handle OOM when appending to the stack or over-recursion errors.
        if (stack.length === 0)
            RecordModuleEvaluationError(module, error);

        assert(module.status === MODULE_STATUS_EVALUATED_ERROR,
               "Bad module status after failed evaluation");

        ModuleTopLevelCapabilityReject(module, error);
    }

    // Step 9
    return capability;
}

// https://tc39.es/ecma262/#sec-innermoduleevaluation
// ES2020 15.2.1.16.2.1 InnerModuleEvaluation
function InnerModuleEvaluation(module, stack, index)
{

    // Step 1
    // TODO: Support module records other than Cyclic Module Records.

    // Step 2
    if (module.status === MODULE_STATUS_EVALUATED_ERROR)
        throw GetModuleEvaluationError(module);

    if (module.status === MODULE_STATUS_EVALUATED)
        return index;

    // Step 3
    if (module.status === MODULE_STATUS_EVALUATING)
        return index;

    // Step 4
    assert(module.status === MODULE_STATUS_LINKED,
          "Bad module status in InnerModuleEvaluation");

    // Step 5
    ModuleSetStatus(module, MODULE_STATUS_EVALUATING);

    // Steps 6-8
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_INDEX_SLOT, index);
    UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT, index);

    UnsafeSetReservedSlot(module, MODULE_OBJECT_PENDING_ASYNC_DEPENDENCIES_SLOT, 0);

    index++;

    // Step 9
    DefineDataProperty(stack, stack.length, module);

    // Step 10
    let requestedModules = module.requestedModules;
    for (let i = 0; i < requestedModules.length; i++) {
        let required = requestedModules[i].moduleRequest;
        let requiredModule =
            CallModuleResolveHook(module, required, MODULE_STATUS_LINKED);

        index = InnerModuleEvaluation(requiredModule, stack, index);

        assert(requiredModule.status === MODULE_STATUS_EVALUATING ||
               requiredModule.status === MODULE_STATUS_EVALUATED,
              "Bad module status after InnerModuleEvaluation");

        assert((requiredModule.status === MODULE_STATUS_EVALUATING) ===
               ArrayContains(stack, requiredModule),
               "Required module should be in the stack iff it is currently being evaluated");

        assert(typeof requiredModule.dfsIndex === "number", "Bad dfsIndex");
        assert(typeof requiredModule.dfsAncestorIndex === "number", "Bad dfsAncestorIndex");

        if (requiredModule.status === MODULE_STATUS_EVALUATING) {
            UnsafeSetReservedSlot(module, MODULE_OBJECT_DFS_ANCESTOR_INDEX_SLOT,
                                  std_Math_min(module.dfsAncestorIndex,
                                               requiredModule.dfsAncestorIndex));
        } else {
          requiredModule = GetCycleRoot(requiredModule);
          assert(requiredModule.status >= MODULE_STATUS_EVALUATED,
                `Bad module status in InnerModuleEvaluation: ${requiredModule.status}`);
          if (requiredModule.status == MODULE_STATUS_EVALUATED_ERROR) {
            throw GetModuleEvaluationError(requiredModule);
          }
        }
        if (IsAsyncEvaluating(requiredModule)) {
            UnsafeSetReservedSlot(module,
                                  MODULE_OBJECT_PENDING_ASYNC_DEPENDENCIES_SLOT,
                                  module.pendingAsyncDependencies + 1);
            AppendAsyncParentModule(requiredModule, module);
        }
    }

    if (module.pendingAsyncDependencies > 0 || module.async) {
      InitAsyncEvaluating(module);
      if (module.pendingAsyncDependencies === 0) {
        ExecuteAsyncModule(module);
      }
    } else {
      ExecuteModule(module);
    }

    // Step 12
    assert(CountArrayValues(stack, module) === 1,
           "Current module should appear exactly once in the stack");

    // Step 13
    assert(module.dfsAncestorIndex <= module.dfsIndex,
           "Bad DFS ancestor index");

    // Step 14
    if (module.dfsAncestorIndex === module.dfsIndex) {
        let cycleRoot = module;
        let requiredModule;
        do {
            requiredModule = callFunction(std_Array_pop, stack);
            ModuleSetStatus(requiredModule, MODULE_STATUS_EVALUATED);
            SetCycleRoot(requiredModule, cycleRoot);
        } while (requiredModule !== module);
    }

    // Step 15
    return index;
}

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

// https://tc39.es/proposal-top-level-await/#sec-execute-async-module
function ExecuteAsyncModule(module) {
  // Steps 1-3.
  assert(module.status == MODULE_STATUS_EVALUATING ||
         module.status == MODULE_STATUS_EVALUATED, "bad status for async module");
  // Step 4-11 done in AsyncAwait opcode

  ExecuteModule(module);
}

