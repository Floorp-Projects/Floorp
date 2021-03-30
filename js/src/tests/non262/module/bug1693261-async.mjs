// |reftest| skip -- support file
if (globalThis.testArray === undefined) {
  globalThis.testArray = [];
}
globalThis.testArray.push("async 1");
await 0;
globalThis.testArray.push("async 2");
