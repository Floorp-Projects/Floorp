/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function AsyncFunction_wrap(genFunction) {
    var wrapper = function() {
        // The try block is required to handle throws in default arguments properly.
        try {
            return AsyncFunction_start(callFunction(std_Function_apply, genFunction, this, arguments));
        } catch (e) {
            return Promise_reject(e);
        }
    };
    SetFunctionExtendedSlot(genFunction, 1, wrapper);
    var name = GetFunName(genFunction);
    if (typeof name !== 'undefined')
        SetFunName(wrapper, name);
    return wrapper;
}

function AsyncFunction_start(generator) {
    return AsyncFunction_resume(generator, undefined, generator.next);
}

function AsyncFunction_resume(gen, v, method) {
    let result;
    try {
        // get back into async function, run to next await point
        result = callFunction(method, gen, v);
    } catch (exc) {
        // The async function itself failed.
        return Promise_reject(exc);
    }

    if (result.done)
        return Promise_resolve(result.value);

    // If we get here, `await` occurred. `gen` is paused at a yield point.
    return callFunction(Promise_then,
                        Promise_resolve(result.value),
                        function(val) {
                            return AsyncFunction_resume(gen, val, gen.next);
                        }, function (err) {
                            return AsyncFunction_resume(gen, err, gen.throw);
                        });
}
