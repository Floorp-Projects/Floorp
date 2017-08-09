// Prior to https://github.com/tc39/ecma262/pull/916 it was possible for a
// module namespace object to be successfully created that was later found to be
// erroneous. Test that this is no longer the case.

"use strict";

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

moduleRepo['A'] = parseModule('import "B"; export {x} from "C"');
moduleRepo['B'] = parseModule('import * as a from "A"');
moduleRepo['C'] = parseModule('export * from "D"; export * from "E"');
moduleRepo['D'] = parseModule('export let x');
moduleRepo['E'] = parseModule('export let x');

let m = moduleRepo['A'];
assertThrowsInstanceOf(() => m.declarationInstantiation(), SyntaxError);
