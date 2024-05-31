function test_function_for_use_counter_integration(fn, counter, expected_growth = true) {
    let before = getUseCounterResults();
    assertEq(counter in before, true);

    console.log(fn.name)
    fn();

    let after = getUseCounterResults();
    if (expected_growth) {
        console.log(`Yes Increase ${counter}: Before `, before[counter], " After", after[counter]);
        assertEq(after[counter] > before[counter], true);
    } else {
        console.log(`No Increase ${counter}: Before `, before[counter], " After", after[counter]);
        assertEq(after[counter] == before[counter], true);
    }
}


class MyPromise extends Promise { }

function promise_all() {
    let p = Promise.all([Promise.resolve(1)]);
    assertEq(p instanceof Promise, true);
}
function promise_all_subclassing_type_ii() {
    let p = MyPromise.all([Promise.resolve(1)]);
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_all, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_all_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

function promise_allSettled() {
    let p = Promise.allSettled([Promise.resolve(1)]);
    assertEq(p instanceof Promise, true);
}
function promise_allSettled_subclassing_type_ii() {
    let p = MyPromise.allSettled([Promise.resolve(1)]);
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_allSettled, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_allSettled_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

function promise_any() {
    let p = Promise.any([Promise.resolve(1)]);
    assertEq(p instanceof Promise, true);
}
function promise_any_subclassing_type_ii() {
    let p = MyPromise.any([Promise.resolve(1)]);
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_any, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_any_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

function promise_race() {
    let p = Promise.race([Promise.resolve(1)]);
    assertEq(p instanceof Promise, true);
}
function promise_race_subclassing_type_ii() {
    let p = MyPromise.race([Promise.resolve(1)]);
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_race, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_race_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

function promise_resolve() {
    let p = Promise.resolve(1)
    assertEq(p instanceof Promise, true);
}
function promise_resolve_subclassing_type_ii() {
    let p = MyPromise.resolve(1)
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_resolve, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_resolve_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

function promise_reject() {
    let p = Promise.reject(1).catch(() => undefined)
    assertEq(p instanceof Promise, true);
}
function promise_reject_subclassing_type_ii() {
    let p = MyPromise.reject(1).catch(() => undefined)
    assertEq(p instanceof MyPromise, true);
}

test_function_for_use_counter_integration(promise_reject, "SubclassingPromiseTypeII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_reject_subclassing_type_ii, "SubclassingPromiseTypeII", /* expected_growth = */ true);

class Sentinel extends Promise { }
class MyPromise2 extends Promise {
    static get [Symbol.species]() {
        return Sentinel;
    }
}

function promise_finally() {
    let p = Promise.reject(0).finally(() => { });
    assertEq(p instanceof Promise, true);
    p.catch(() => { })
}

function promise_finally_subclassing_type_iii() {
    var p = MyPromise2.reject().finally(() => { });
    assertEq(p instanceof Sentinel, true);
    p.catch(() => { })
}

test_function_for_use_counter_integration(promise_finally, "SubclassingPromiseTypeIII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_finally_subclassing_type_iii, "SubclassingPromiseTypeIII", /* expected_growth = */ true);

function promise_then() {
    let p = Promise.resolve(0).then(() => { });
    assertEq(p instanceof Promise, true);
}

function promise_then_subclassing_type_iii() {
    var p = MyPromise2.resolve().then(() => { });
    assertEq(p instanceof Sentinel, true);
}

test_function_for_use_counter_integration(promise_then, "SubclassingPromiseTypeIII", /* expected_growth = */ false);
test_function_for_use_counter_integration(promise_then_subclassing_type_iii, "SubclassingPromiseTypeIII", /* expected_growth = */ true);


class MyDefaultPromiseBase extends Promise {
    static get [Symbol.species]() {
        return Promise;
    }
}

function promise_then_subclassing_type_iii_default_base() {
    var p = MyDefaultPromiseBase.resolve().then(() => { });
    assertEq(p instanceof Promise, true);
}

test_function_for_use_counter_integration(promise_then_subclassing_type_iii_default_base, "SubclassingPromiseTypeIII", /* expected_growth = */ true);



class MyDefaultPromise extends Promise {
    static get [Symbol.species]() {
        return this;
    }
}

function promise_then_subclassing_type_iii_default_exception() {
    var p = MyDefaultPromise.resolve().then(() => { });
    assertEq(p instanceof MyDefaultPromise, true);
}

test_function_for_use_counter_integration(promise_then_subclassing_type_iii_default_exception, "SubclassingPromiseTypeIII", /* expected_growth = */ false);
