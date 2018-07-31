import aDefault from "./src/mod1";
import { aNamed, aNamed as anotherNamed } from "./src/mod2";
import { original as anAliased } from "./src/mod3";
import * as aNamespace from "./src/mod4";

import aDefault2 from "./src/mod5";
import { aNamed2, aNamed2 as anotherNamed2 } from "./src/mod6";
import { original as anAliased2 } from "./src/mod7";

import aDefault3 from "./src/mod9";
import { aNamed3, aNamed3 as anotherNamed3 } from "./src/mod10";
import { original as anAliased3 } from "./src/mod11";

import optimizedOut from "./src/optimized-out";
optimizedOut();

export default function root() {
  console.log("pause here", root);

  console.log(aDefault);
  console.log(anAliased);
  console.log(aNamed);
  console.log(anotherNamed);
  console.log(aNamespace);

  try {
    // None of these are callable in this code, but we still want to make sure
    // they map properly even if the only reference is in a call expressions.
    console.log(aDefault2());
    console.log(anAliased2());
    console.log(aNamed2());
    console.log(anotherNamed2());

    console.log(new aDefault3());
    console.log(new anAliased3());
    console.log(new aNamed3());
    console.log(new anotherNamed3());
  } catch (e) {}
}

export function example(){}
