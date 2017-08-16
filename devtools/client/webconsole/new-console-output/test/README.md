# Console Tests
The console panel uses currently two different frameworks for tests:

* Mochitest - [Mochitest](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Mochitest) is an automated testing framework built on top of the MochiKit JavaScript libraries. It's just one of the automated regression testing frameworks used by Mozilla

* Mocha + Enzyme - [mocha](https://mochajs.org/) Mocha is JavaScript test framework running on Node.js
[Enzyme](http://airbnb.io/enzyme/) is a JavaScript Testing utility for React that makes it easier to assert, manipulate, and traverse your React Components' output.

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
`./mach test devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/browser_webconsole_check_stubs_console_api.js`

## Append new CSS Messages stubs
See how to generate stubs for CSS messages.

* Append new entry into `cssMessage` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_check_stubs_css_message.js`

## Append new Evaluation Result stubs
See how to generate stubs for evaluation results.

* Append new entry into `evaluationResultCommands` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_check_stubs_evaluation_result.js`

## Append new Network Events stubs
See how to generate stubs for network events

* Append new entry into `networkEvent` map into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_update_stubs_network_event.js`

## Append new Page Error stubs
See how to generate stubs for page errors.

* Append new entry into `pageError` array into `stub-snippets.js`
* Generate stubs with: `browser_webconsole_update_stubs_page_error.js`
