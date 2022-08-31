let m = registerModule('m', parseModule(`import {} from "s";`));
let l = registerModule('l', parseModule(`import {} from "s";`));
let s = registerModule('s', parseModule(`await 0;`));

let state = "init";

moduleLink(m);
moduleEvaluate(m).then(() => { state = "loaded"; });
drainJobQueue();

assertEq(state, "loaded");

import("l").then(() => { state = "complete"; });
drainJobQueue();

assertEq(state, "complete");
