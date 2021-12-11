// code in arrow function default arguments is strict if arrow is strict

load(libdir + "asserts.js");

assertThrowsInstanceOf(
    () => Function("'use strict'; (a = function (obj) { with (obj) f(); }) => { }"),
    SyntaxError);

assertThrowsInstanceOf(
    () => Function("'use strict'; (a = obj => { with (obj) f(); }) => { }"),
    SyntaxError);
