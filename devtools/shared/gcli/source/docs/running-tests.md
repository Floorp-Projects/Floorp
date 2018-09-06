
# Running Tests

GCLI has a test suite that can be run in a number of different environments.
Some of the tests don't work in all environments. These should be automatically
skipped when not applicable.


## Web

Running a limited set of test from the web is the easiest. Simply load
'localtest.html' and the unit tests should be run automatically, with results
displayed on the console. Tests can be re-run using the 'test' command.

It also creates a function 'testCommands()' to be run at a JS prompt, which
enables the test commands for debugging purposes.


## Firefox

GCLI's test suite integrates with Mochitest and runs automatically on each test
run. Dryice packages the tests to format them for the Firefox build system.

For more information about running Mochitest on Firefox (including GCLI) see
[the MDN, Mochitest docs](https://developer.mozilla.org/en/Mochitest)


# Node

Running the test suite under node can be done as follows:

    $ node gcli.js test

Or, using the `test` command:

    $ node gcli.js
    Serving GCLI to http://localhost:9999/
    This is also a limited GCLI prompt.
    Type 'help' for a list of commands, CTRL+C twice to exit:
    : test
    
    testCli: Pass (funcs=9, checks=208)
    testCompletion: Pass (funcs=1, checks=139)
    testExec: Pass (funcs=1, checks=133)
    testHistory: Pass (funcs=3, checks=13)
    ....
    
    Summary: Pass (951 checks)


# Travis CI

GCLI check-ins are automatically tested by [Travis CI](https://travis-ci.org/joewalker/gcli).


# Test Case Generation

GCLI can generate test cases automagically. Load ```localtest.html```, type a
command to be tested into GCLI, and the press F2. GCLI will output to the
console a template test case for the entered command.
