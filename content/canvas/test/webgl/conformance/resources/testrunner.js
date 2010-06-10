/*
 * testrunner.js: simple object-oriented test runner for WebGL.
 *
 * Copyright (c) 2010 Cedric Vivier <cedricv@neonux.com>.
 * Use of this source code is governed by a MIT license.
 *
 * Example usage:
 *
 * var testsuite = {
 *     "enable an invalid capability should generate INVALID_VALUE" : function () {
 *         this.setup = function () {
 *             gl.enable(0x666);
 *         };
 *         this.expects = gl.INVALID_VALUE;
 *     },
 *     "not using correct parameter type should throw a TypeError" : function () {
 *         this.setup = function () {
 *             gl.enable("not a number");
 *         };
 *         this.expects = "TypeError";
 *     },
 *     "check that correct image is drawn" : function () {
 *         this.setup = function () {
 *             //draw something
 *         };
 *         this.expects = function () {
 *             //read pixels
 *             //return true if pixels are okay, false otherwise
 *         };
 *     }
 * };
 * runTestsuite(testsuite);
 *
 */

function runTestsuite(testsuite) {
	var testname;
	function testFatal(message) {
		testFailed("FATAL: " + testname + " :: " + message);
	}

	var n = 0;
	for (testname in testsuite) {
try {
		if (testname === "setup" || testname === "teardown") //special functions
			continue;

		var test = new testsuite[testname]();

		if (n > 0 && typeof(testsuite.teardown) === "function")
			testsuite.teardown();  // teardown code run _after_ EACH test
		if (typeof(testsuite.setup) === "function")
			testsuite.setup(); // setup code run _before_ EACH test

		n++;

		if (typeof(test.setup) !== "function") {
			testFatal("`setup' is not a function.");
			continue;
		}
		test.setup();

		var err = gl.getError();
		switch (typeof(test.expects)) {
		case "number": // expect a GL error
			if (err !== test.expects) {
				testFailed(testname + " :: got GL error `" + getGLErrorAsString(gl, err) + "' instead of expected `" + getGLErrorAsString(gl, test.expects) + "'.");
				continue;
			}
			break;
		case "string": // expect an exception starting with given string
				testFailed(testname + " :: expected a `" + test.expects + "' exception but no exception has been caught.");
				continue;
		case "function": // expect no error and the function to return true
			if (err !== gl.NO_ERROR) {
				testFailed(testname + " :: got GL error `" + getGLErrorAsString(gl, err) + "' when none was expected.");
				continue;
			}
			if (test.expects() !== true) {
				testFailed(testname);
				continue;
			}
			break;
		default:
			testFatal("`expects' is neither a function or a number (GL error) but `" + typeof(test.expects) + "'.");
			continue;
		}
		testPassed(testname);
} catch (e) {
	if (test && typeof(test.expects) === "string") { // an exception was expected
		if (e.toString().indexOf(test.expects) === 0) // got expected exception
			testPassed(testname);
		else // got another exception
			testFailed(testname + " :: caught exception `" + e + "' when a `" + test.expects + "' exception was expected.");
		continue;
	}
	testFailed(testname + " :: caught unexpected exception `" + e + "'.");
}
	}
}

