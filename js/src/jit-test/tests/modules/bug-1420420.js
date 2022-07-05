// Test re-instantiation module after failure.

load(libdir + "asserts.js");

registerModule("good", parseModule(`export let x`));

registerModule("y1", parseModule(`export let y`));
registerModule("y2", parseModule(`export let y`));
registerModule("bad", parseModule(`export* from "y1"; export* from "y2";`));

registerModule("a", parseModule(`import {x} from "good"; import {y} from "bad";`));

let b = registerModule("b", parseModule(`import "a";`));
let c = registerModule("c", parseModule(`import "a";`));

assertThrowsInstanceOf(() => moduleLink(b), SyntaxError);
assertThrowsInstanceOf(() => moduleLink(c), SyntaxError);
