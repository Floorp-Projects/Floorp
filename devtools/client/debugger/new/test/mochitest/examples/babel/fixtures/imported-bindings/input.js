import aDefault from "./src/mod1";
import { aNamed } from "./src/mod2";
import { original as anAliased } from "./src/mod3";
import * as aNamespace from "./src/mod4";

export default function root() {
  console.log(anAliased);
  console.log(aNamed);
  console.log(anAliased);
  console.log(aNamespace);
  console.log("pause here", root);
}
