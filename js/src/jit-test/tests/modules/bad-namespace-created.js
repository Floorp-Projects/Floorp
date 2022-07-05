// Prior to https://github.com/tc39/ecma262/pull/916 it was possible for a
// module namespace object to be successfully created that was later found to be
// erroneous. Test that this is no longer the case.

"use strict";

load(libdir + "asserts.js");

let a = registerModule('A', parseModule('import "B"; export {x} from "C"'));
registerModule('B', parseModule('import * as a from "A"'));
registerModule('C', parseModule('export * from "D"; export * from "E"'));
registerModule('D', parseModule('export let x'));
registerModule('E', parseModule('export let x'));

assertThrowsInstanceOf(() => moduleLink(a), SyntaxError);
