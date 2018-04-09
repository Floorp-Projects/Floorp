# Console Tests
The console panel uses currently two different frameworks for tests:

* Mochitest - [Mochitest](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Mochitest) is an automated testing framework built on top of the MochiKit JavaScript libraries. It's just one of the automated regression testing frameworks used by Mozilla.

Mochitests are located in `devtools/client/webconsole/test/mochitest/` and can be run with the following command:

```sh
./mach test devtools/client/webconsole/test/mochitest/
```

These tests can be run on CI when pushing to TRY. Not all tests are enabled at the moment since they were copied over from the old frontend (See Bug 1400847).

* Mocha + Enzyme - [mocha](https://mochajs.org/) Mocha is JavaScript test framework running on Node.js
[Enzyme](http://airbnb.io/enzyme/) is a JavaScript Testing utility for React that makes it easier to assert, manipulate, and traverse your React Components' output.

These tests are located in `test/components/` and `test/store/`, and can be run with the following command:

```sh
devtools/client/webconsole/test/ && npm install && npm test
```

or using yarn with

```sh
devtools/client/webconsole/test/ && yarn && yarn test
```

**⚠️️️️️️️️️️ These tests are not ran on CI at the moment. You need to run them manually when working on the console. (See Bug 1312823)**

---

The team is leaning towards Enzyme since it's well known and suitable for React.
It's also easier to contribute to tests written on top of Enzyme.

# Stubs
Many tests depends on fix data structures (aka stubs) that mimic
<abbr title="Remote Debugging Protocol">RDP<abbr> packets that represents Console logs.
Stubs are stored in `test/fixtures` directory and you might automatically generate them.

## Append new Console API stubs
See how to generate stubs for Console API calls.

* Append new entry into `consoleApiCommands` array. The array is defined in this module:
`\test\fixtures\stub-generators\stub-snippets.js`
* Generate stubs with existing mochitest:
`\test\fixtures\stub-generators\browser_webconsole_check_stubs_console_api.js`

Run the generator using `mach` command.
`./mach test devtools/client/webconsole/test/fixtures/stub-generators/browser_webconsole_check_stubs_console_api.js --headless`

This will override `test/fixtures/stubs/consoleApi.js`.

## Append new CSS Messages stubs
See how to generate stubs for CSS messages.

* Append new entry into `cssMessage` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_check_stubs_css_message.js`

This will override `test/fixtures/stubs/cssMessage.js`.

## Append new Evaluation Result stubs
See how to generate stubs for evaluation results.

* Append new entry into `evaluationResultCommands` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_check_stubs_evaluation_result.js`

This will override `test/fixtures/stubs/evaluationResult.js`.

## Append new Network Events stubs
See how to generate stubs for network events

* Append new entry into `networkEvent` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_update_stubs_network_event.js`

This will override `test/fixtures/stubs/networkEvent.js`.

## Append new Page Error stubs
See how to generate stubs for page errors.

* Append new entry into `pageError` array into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_update_stubs_page_error.js`

This will override `test/fixtures/stubs/pageError.js`.
