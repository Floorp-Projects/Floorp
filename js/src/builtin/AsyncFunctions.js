/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Called when creating async function.
// See the comment for js::CreateAsyncFunction what unwrapped, wrapped and the
// return value are.
function AsyncFunction_wrap(unwrapped) {
    var wrapper = function() {
        // The try block is required to handle throws in default arguments properly.
        try {
            return AsyncFunction_start(callFunction(std_Function_apply, unwrapped, this, arguments));
        } catch (e) {
            var promiseCtor = GetBuiltinConstructor('Promise');
            return callFunction(Promise_static_reject, promiseCtor, e);
        }
    };
    return CreateAsyncFunction(wrapper, unwrapped);
}

function AsyncFunction_start(generator) {
    return AsyncFunction_resume(generator, undefined, generator.next);
}

function AsyncFunction_resume(gen, v, method) {
    var promiseCtor = GetBuiltinConstructor('Promise');
    let result;
    try {
        // get back into async function, run to next await point
        result = callFunction(method, gen, v);
    } catch (exc) {
        // The async function itself failed.
        return callFunction(Promise_static_reject, promiseCtor, exc);
    }

    if (result.done)
        return callFunction(Promise_static_resolve, promiseCtor, result.value);

    // If we get here, `await` occurred. `gen` is paused at a yield point.
    return callFunction(Promise_then,
                        callFunction(Promise_static_resolve, promiseCtor, result.value),
                        function(val) {
                            return AsyncFunction_resume(gen, val, gen.next);
                        }, function (err) {
                            return AsyncFunction_resume(gen, err, gen.throw);
                        });
}
