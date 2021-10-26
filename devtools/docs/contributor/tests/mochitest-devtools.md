# Automated tests: DevTools mochitests

To run the whole suite of browser mochitests for DevTools (sit back and relax):

```bash
./mach mochitest --subsuite devtools --tag devtools
```
To run a specific tool's suite of browser mochitests:

```bash
./mach mochitest devtools/client/<tool>
```

For example, run all of the debugger browser mochitests:

```bash
./mach mochitest devtools/client/debugger
```
To run a specific DevTools mochitest:

```bash
./mach mochitest devtools/client/path/to/the/test_you_want_to_run.js
```
Note that the mochitests *must* have focus while running. The tests run in the browser which looks like someone is magically testing your code by hand. If the browser loses focus, the tests will stop and fail after some time. (Again, sit back and relax)

In case you'd like to run the mochitests without having to care about focus and be able to touch your computer while running:

```bash
./mach mochitest --headless devtools/client/<tool>
```

You can also run just a single test:

```bash
./mach mochitest --headless devtools/client/path/to/the/test_you_want_to_run.js
```
