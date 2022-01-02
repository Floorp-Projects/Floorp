// |reftest| skip-if(!xulRuntime.shell) module async  -- needs drainJobQueue

import "./bug1693261-c1.mjs";
import "./bug1693261-c2.mjs";
import "./bug1693261-x.mjs";
if (globalThis.testArray === undefined) {
  globalThis.testArray = [];
}
globalThis.testArray.push("index");

function assertEqArray(actual, expected) {
	if (actual.length != expected.length) {
		throw new Error(
			"array lengths not equal: got " +
			JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
	}

	for (var i = 0; i < actual.length; ++i) {
		if (actual[i] != expected[i]) {
		throw new Error(
			"arrays not equal at element " + i + ": got " +
			JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
		}
	}
}

assertEqArray(globalThis.testArray, [
	'async 1', 'async 2', 'c1', 'c2', 'x', 'index'
])

drainJobQueue();
if (typeof reportCompare === "function")
  reportCompare(true, true);
