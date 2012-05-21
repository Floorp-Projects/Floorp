/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This executes a very simple search for all functions that call a
// given set of functions. It's intended to be the simplest possible
// way of refactoring a common pattern of function calls. Of course,
// it's still up to a human to decide if the replacement is truely
// suitable, but this gets the low-hanging fruit.

// Expects the variable 'patterns' to hold an object with replacement
// function names as keys and function lists as values. Any function
// in the tested source that calls all of the functions named in the
// list will be listed in the output as being likely candidates to
// instead call the replacement function.

include("unstable/lazy_types.js");

var matches = {};

function identity(x) x;

function process_cp_pre_genericize(fndecl)
{
    var c = [];
    function calls(t, stack)
    {
        try {
            t.tree_check(CALL_EXPR);
            var fn = callable_arg_function_decl(CALL_EXPR_FN(t));
            if (fn)
                c.push(decl_name_string(fn));
        }
        catch (e if e.TreeCheckError) { }
    }

    walk_tree(DECL_SAVED_TREE(fndecl), calls);

    for (let [fnreplace, pattern] in patterns)
        if (pattern.map(function(e){ return c.some(function(f) { return e == f; }); }).every(identity))
            if (fnreplace != (n = decl_name_string(fndecl)))
                print(fnreplace +" could probably be used in "+ n);
}
