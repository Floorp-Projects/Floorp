let tag = new WebAssembly.Tag({parameters: []});

function construct(options) {
	return new WebAssembly.Exception(tag, [], options);
}
function noStack(options) {
	assertEq(construct(options).stack, undefined, 'no stack');
}
function hasStack(options) {
	assertEq(typeof construct(options).stack === 'string', true, 'has stack');
}

// Test valid option constructors
noStack(undefined);
noStack(null);
noStack({});
noStack({traceStack: false});
noStack({traceStack: 0});
hasStack({traceStack: true});
hasStack({traceStack: 1});

// Test invalid option constructors
assertErrorMessage(() => construct('not an object'), TypeError, /cannot be converted/);

// Test that 'stack' is read-only
let exception = construct({traceStack: true});
exception.stack = 0;
assertEq(typeof exception.stack === 'string', true, 'is read-only');
