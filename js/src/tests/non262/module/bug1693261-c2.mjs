// |reftest| skip -- support file
import "./bug1693261-async.mjs";
if (globalThis.testArray === undefined) {
  globalThis.testArray = [];
}
globalThis.testArray.push("c2");
