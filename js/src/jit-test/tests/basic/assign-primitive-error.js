'use strict';

load(libdir + 'asserts.js');

assertTypeErrorMessage(() => { var x = 1; x.a = 'a'; },
                       `can't assing to property "a" on 1: not an object`);
assertTypeErrorMessage(() => { var x = "hello"; x.b = 'a'; },
                       `can't assing to property "b" on "hello": not an object`);
assertTypeErrorMessage(() => { var x = false; x.c = 'a'; },
                       `can't assing to property "c" on false: not an object`);
assertTypeErrorMessage(() => { var x = Symbol(); x.d = 'a'; },
                       `can't assing to property "d" on Symbol(): not an object`);
assertTypeErrorMessage(() => { var x = Symbol("abc"); x.d = 'a'; },
                       `can't assing to property "d" on Symbol("abc"): not an object`);
assertTypeErrorMessage(() => { var x = 1; x[15] = 'a'; },
                       `can't assing to property 15 on 1: not an object`);
assertTypeErrorMessage(() => { var x = 1; x[Symbol("abc")] = 'a'; },
                       `can't assing to property Symbol("abc") on 1: not an object`);
