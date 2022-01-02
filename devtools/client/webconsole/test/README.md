# Console Tests

The console panel uses currently two different frameworks for tests:

- Mochitest - [Mochitest](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Mochitest) is an automated testing framework built on top of the MochiKit JavaScript libraries. It's just one of the automated regression testing frameworks used by Mozilla.

Mochitests are located in `devtools/client/webconsole/test/browser/` and can be run with the following command:

```sh
./mach test devtools/client/webconsole/test/browser/
```

These tests can be run on CI when pushing to TRY. Not all tests are enabled at the moment since they were copied over from the old frontend (See Bug 1400847).

- Mocha + Enzyme - [mocha](https://mochajs.org/) Mocha is JavaScript test framework running on Node.js
  [Enzyme](http://airbnb.io/enzyme/) is a JavaScript Testing utility for React that makes it easier to assert, manipulate, and traverse your React Components' output.

These tests are located in `tests/node/`, and can be run with the following command:

```sh
cd devtools/client/webconsole/test/node && npm install && npm test
```

or using yarn with

```sh
cd devtools/client/webconsole/test/node && yarn && yarn test
```

---

The team is leaning towards Enzyme since it's well known and suitable for React.
It's also easier to contribute to tests written on top of Enzyme.

# Stubs

Many tests depends on fix data structures (aka stubs) that mimic
<abbr title="Remote Debugging Protocol">RDP<abbr> packets that represents Console logs.
Stubs are stored in `test/fixtures` directory and you might automatically generate them.

## Append new stubs

- Append new entry into the `getCommands` function return value in on of the `\tests\browser\browser_webconsole_stubs_*.js`
  and run the generator using `mach` command, with the `WEBCONSOLE_STUBS_UPDATE=true` environment variable.

For console API stubs, you can do:

`./mach test devtools/client/webconsole/test/browser/browser_webconsole_stubs_console_api.js --headless --setenv WEBCONSOLE_STUBS_UPDATE=true`

This will override `tests/node/fixtures/stubs/consoleApi.js`.

The same can be done in:

- `browser_webconsole_stubs_css_message.js` (writes to `tests/node/fixtures/stubs/cssMessage.js`)
- `browser_webconsole_stubs_evaluation_result.js` (writes to `tests/node/fixtures/stubs/evaluationResult.js`)
- `browser_webconsole_stubs_network_event.js` (writes to `tests/node/fixtures/stubs/networkEvent.js`)
- `browser_webconsole_stubs_page_error.js` (writes to `tests/node/fixtures/stubs/pageError.js`)

If you made some changes that impact all stubs, you can update all at once using:

`./mach test devtools/client/webconsole/test/browser/browser_webconsole_stubs --headless --setenv WEBCONSOLE_STUBS_UPDATE=true`
