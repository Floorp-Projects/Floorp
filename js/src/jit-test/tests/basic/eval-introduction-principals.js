// Ensure the introduction info for eval scripts respects principal checks.

function myAPI(f) { return f(); }

var contentGlobal = newGlobal({principal: 0x1});
contentGlobal.chrome = this;
contentGlobal.eval("\n" +
		   "function contentTest() { chrome.myAPI(eval.bind(undefined, 'chrome.stack = Error().stack;')) };\n" +
		   "contentTest();");

// Note that the stack below does not include the current filename or file
// line numbers, and there's no trace of the myAPI call between the two
// evals.
assertEq(stack, "@eval line 2 > eval:1:16\n" +
                "contentTest@eval:2:26\n" +
                "@eval:3:1\n");
