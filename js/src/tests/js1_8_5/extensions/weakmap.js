/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Andreas Gal <gal@mozilla.com>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 547941;
var summary = 'js weak maps';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
    enterFunc ('test');
    printBugNumber(BUGNUMBER);
    printStatus(summary);

    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, todo) {
        let thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = true;
        }

        check(function() thrown, todo);
    }

    var key = {};
    var map = new WeakMap();

    check(function() !map.has(key));
    check(function() map.delete(key) == false);
    check(function() map.set(key, 42) === map);
    check(function() map.get(key) == 42);
    check(function() typeof map.get({}) == "undefined");
    check(function() map.get({}, "foo") == undefined);

    gc(); gc(); gc();

    check(function() map.get(key) == 42);
    check(function() map.delete(key) == true);
    check(function() map.delete(key) == false);
    check(function() map.delete({}) == false);

    check(function() typeof map.get(key) == "undefined");
    check(function() !map.has(key));
    check(function() map.delete(key) == false);

    var value = { };
    check(function() map.set(new Object(), value) === map);
    gc(); gc(); gc();

    check(function() map.has("non-object key") == false);
    check(function() map.has() == false);
    check(function() map.get("non-object key") == undefined);
    check(function() map.get() == undefined);
    check(function() map.delete("non-object key") == false);
    check(function() map.delete() == false);

    check(function() map.set(key) === map);
    check(function() map.get(key) == undefined);

    checkThrows(function() map.set("non-object key", value));

    print ("done");

    reportCompare(0, TestFailCount, "weak map tests");

    exitFunc ('test');
}
