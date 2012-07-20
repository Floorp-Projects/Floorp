function outer() {
    "use strict";
    function inner() {}
    return inner;
}
assertEq(outer().toString().indexOf("use strict") != -1, true);
function outer2() {
    "use strict";
    function inner() blah;
    return inner;
}
assertEq(outer2().toString().indexOf("/* use strict */") != -1, true);
