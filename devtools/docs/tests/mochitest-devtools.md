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
Note that the mochitests *must* have focus while running.

