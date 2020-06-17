// |reftest| skip-if(!xulRuntime.shell) -- needs async stack capture

function toMessage(stack) {
  // Provide the stack string in the error message for debugging.
  return `[stack: ${stack.replace(/\n/g, "\\n")}]`;
}

// Test when AggregateError isn't created from a Promise Job.
{
  let p = Promise.any([]); // line 10

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^@.+any-stack.js:10/m.test(stack), true, toMessage(stack));
  });
}

// Same as above, but now with surrounding function context.
function testNoJobQueue() {
  let p = Promise.any([]); // line 24

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^testNoJobQueue@.+any-stack.js:24/m.test(stack), true, toMessage(stack));
  });
}
testNoJobQueue();

// Test when AggregateError is created from a Promise Job.
{
  let rejected = Promise.reject(0);
  let p = Promise.any([rejected]); // line 40

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^Promise.any\*@.+any-stack.js:40/m.test(stack), true, toMessage(stack));
  });
}

// Same as above, but now with surrounding function context.
function testFromJobQueue() {
  let rejected = Promise.reject(0);
  let p = Promise.any([rejected]); // line 55

  p.then(v => {
    reportCompare(0, 1, "expected error");
  }, e => {
    assertEq(e.name, "AggregateError");
    var {stack} = e;

    assertEq(/^Promise.any\*testFromJobQueue@.+any-stack.js:55/m.test(stack), true, toMessage(stack));
  });
}
testFromJobQueue();

if (typeof reportCompare === "function")
  reportCompare(0, 0);
