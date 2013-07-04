// code in arrow function default arguments is strict if the body is strict

load(libdir + "asserts.js");

assertThrowsInstanceOf(
    () => Function("(a = function (obj) { with (obj) f(); }) => { 'use strict'; }"),
    SyntaxError);

assertThrowsInstanceOf(
    () => Function("(a = obj => { with (obj) f(); }) => { 'use strict'; }"),
    SyntaxError);
