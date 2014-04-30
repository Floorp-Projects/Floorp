// Test that we can track allocation sites.

enableTrackAllocations();

const tests = [
  { name: "object literal",  object: {},                    line: Error().lineNumber },
  { name: "array literal",   object: [],                    line: Error().lineNumber },
  { name: "regexp literal",  object: /(two|2)\s*problems/,  line: Error().lineNumber },
  { name: "new constructor", object: new function Ctor(){}, line: Error().lineNumber },
  { name: "new Object",      object: new Object(),          line: Error().lineNumber },
  { name: "new Array",       object: new Array(),           line: Error().lineNumber },
  { name: "new Date",        object: new Date(),            line: Error().lineNumber }
];

disableTrackAllocations();

for (let { name, object, line } of tests) {
  print("Entering test: " + name);

  let allocationSite = getObjectMetadata(object);
  print(allocationSite);

  assertEq(allocationSite.line, line);
}
