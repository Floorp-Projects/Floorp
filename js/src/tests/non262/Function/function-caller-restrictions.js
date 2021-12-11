// Function#caller restrictions as proposed by
// https://github.com/claudepache/es-legacy-function-reflection/

function caller() {
    return caller.caller;
}

assertEq(caller(), null);
assertEq(Reflect.apply(caller, undefined, []), null);

assertEq([0].map(caller)[0], null);

(function strict() {
    "use strict";
    assertEq(caller(), null);
})();

(async function() {
    assertEq(caller(), null);
})();

assertEq(function*() {
    yield caller();
}().next().value, null);


if (typeof reportCompare === "function") {
    reportCompare(true, true);
}
