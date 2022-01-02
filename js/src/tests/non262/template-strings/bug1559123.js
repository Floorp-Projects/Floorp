// |reftest| skip-if(!xulRuntime.shell)

const getStrings = (strings, ...values) => strings;
const getRef = () => getStrings`test`;
let c = getRef();
relazifyFunctions(getRef);
assertEq(getRef(), c);
// Note the failure case here looks like this:
// Assertion failed: got ["test"], expected ["test"]
// If you're reading this test and going "wtf", it's because this is testing
// reference identity of the array object - they're actually different arrays,
// but have the same contents.

if (typeof reportCompare === "function")
  reportCompare(true, true);
