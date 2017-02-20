var BUGNUMBER = 1340089;
var summary = "Comprehension should check the binding names";

print(BUGNUMBER + ": " + summary);

// Non strict mode.
// Keywords, literals, 'let', and 'yield' are not allowed.

assertThrowsInstanceOf(function () {
    eval("[for (true of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("(for (true of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    eval("[for (throw of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("(for (throw of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    eval("[for (let of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("(for (let of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    eval("[for (yield of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    eval("(for (yield of [1]) 2)");
}, SyntaxError);

eval("[for (public of [1]) 2]");
eval("(for (public of [1]) 2)");

eval("[for (static of [1]) 2]");
eval("(for (static of [1]) 2)");

// Strict mode.
// All reserved words are not allowed.

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (true of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (true of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (throw of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (throw of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (let of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (let of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (yield of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (yield of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (public of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (public of [1]) 2)");
}, SyntaxError);

assertThrowsInstanceOf(function () {
    "use strict";
    eval("[for (static of [1]) 2]");
}, SyntaxError);
assertThrowsInstanceOf(function () {
    "use strict";
    eval("(for (static of [1]) 2)");
}, SyntaxError);

(function () {
    "use strict";
    eval("[for (await of [1]) 2]");
    eval("(for (await of [1]) 2)");
})();

if (typeof reportCompare === "function")
    reportCompare(true, true);
