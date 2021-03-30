// |reftest| skip -- support file
import "./bug1693261-c1.mjs";
if (globalThis.testArray === undefined) {
  globalThis.testArray = [];
}
globalThis.testArray.push("x");
