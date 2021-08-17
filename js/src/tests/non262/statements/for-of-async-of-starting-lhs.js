if (typeof getRealmConfiguration === "undefined") {
  var getRealmConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getRealmConfiguration;
}


const AsyncFunction = async function(){}.constructor;

function assertNoError(f, msg) {
  try {
    f();
  } catch (e) {
    assertEq(true, false, `${msg}: ${e}`);
  }
}

function assertSyntaxError(code) {
  assertThrowsInstanceOf(function () { Function(code); }, SyntaxError, "Function:" + code);
  assertThrowsInstanceOf(function () { AsyncFunction(code); }, SyntaxError, "AsyncFunction:" + code);

  if (typeof parseModule === "function") {
    assertThrowsInstanceOf(function () { parseModule(code); }, SyntaxError, "Module:" + code);
  }
}

function assertNoSyntaxError(code) {
  assertNoError(function () { Function(code); }, "Function:" + code);
  assertNoError(function () { AsyncFunction(code); }, "AsyncFunction:" + code);

  if (typeof parseModule === "function") {
    assertNoError(function () { parseModule(code); }, "Module:" + code);
  }
}

function assertNoSyntaxErrorAsyncContext(code) {
  assertNoError(function () { AsyncFunction(code); }, "AsyncFunction:" + code);

  if (typeof parseModule === "function") {
    assertNoError(function () { parseModule(code); }, "Module:" + code);
  }
}

const invalidTestCases = [
  // for-in loop: LHS can't start with an async arrow function.
  "for ( async of => {} in [] ) ;",
  "for ( async o\\u0066 => {} in [] ) ;",

  // for-of loop: LHS can't start with an async arrow function.
  "for ( async of => {} of [] ) ;",
  "for ( async o\\u0066 => {} of [] ) ;",

  // for-of loop: LHS can't start with an identifier named "async".
  "for ( async of [] ) ;",

  // for-await-of loop: LHS can't start with an async arrow function.
  "for await ( async of => {} of [] ) ;",
  "for await ( async o\\u0066 => {} of [] ) ;",
];

for (let source of invalidTestCases) {
  assertSyntaxError(source);

  // Also test when the tokens are separated by newline characters.
  assertSyntaxError(source.split(" ").join("\n"));
}

// for-loop: async arrow functions are allowed in C-style for-loops.
assertNoSyntaxError("for ( async of => {} ; ; ) ;")

const validTestCases = [
  // for-loop: LHS can start with an identifier named "async".
  "for ( async ; ; ) ;",
  "for ( \\u0061sync ; ; ) ;",

  // for-in loop: LHS can start with an identifier named "async".
  "for ( async in [] ) ;",
  "for ( \\u0061sync in [] ) ;",

  // for-in loop: LHS can start with an property assignment starting with "async".
  "for ( async . prop in [] ) ;",
  "for ( async [ 0 ] in [] ) ;",

  // for-of loop: LHS can start with an identifier named "async" when escape characters are used.
  "for ( \\u0061sync of [] ) ;",

  // for-of loop: LHS can start with an property assignment starting with "async".
  "for ( async . prop of [] ) ;",
  "for ( async [ 0 ] of [] ) ;",
];

for (let source of validTestCases) {
  assertNoSyntaxError(source);

  // Also test when the tokens are separated by newline characters.
  assertNoSyntaxError(source.split(" ").join("\n"));
}

const validTestCasesAsync = [
  // for-await-of loop: LHS can start with an identifier named "async".
  "for await ( async of [] ) ;",
  "for await ( \\u0061sync of [] ) ;",

  // for-await-of loop: LHS can start with an property assignment starting with "async".
  "for await ( async . prop of [] ) ;",
  "for await ( async [ 0 ] of [] ) ;",
];

for (let source of validTestCasesAsync) {
  assertNoSyntaxErrorAsyncContext(source);

  // Also test when the tokens are separated by newline characters.
  assertNoSyntaxErrorAsyncContext(source.split(" ").join("\n"));
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
