// |reftest| module

var x = "ok";

export {x as "*"};

import {"*" as y} from "./module-export-name-star.js"

assertEq(y, "ok");

if (typeof reportCompare === "function")
  reportCompare(true, true);
