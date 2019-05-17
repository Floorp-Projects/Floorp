======================
Firefox Home (New Tab)
======================

All files related to Firefox Home, which includes content that appears on `about:home`,
`about:newtab`, and `about:welcome`, can we found in the `browser/components/newtab` directory.
Some of these source files (such as `.js`, `.jsx`, and `.sass`) require an additional build step.
We are working on migrating this to work with `mach`, but in the meantime, please
follow the following steps if you need to make changes in this directory:

For .jsm files
---------------

No build step is necessary. Use `mach` and run mochi tests according to your regular Firefox workflow.

For .js, .jsx, .sass, or .css files
-----------------------------------

Prerequisites
`````````````

You will need the following:

- Node.js 8+ (On Mac, the best way to install Node.js is to use the [install link on the Node.js homepage](https://nodejs.org/en/))
- npm (packaged with Node.js)

To install dependencies, run the following from the root of the mozilla-central repository
(or cd into browser/components/newtab to omit the `--prefix` in any of these commands):

.. code-block:: shell

  npm install --prefix browser/components/newtab


Which files should you edit?
````````````````````````````

You should not make changes to `.js` or `.css` files in `browser/components/newtab/css` or
`browser/components/newtab/data` directory. Instead, you should edit the `.jsx`, `.js`, and `.sass` files
in `browser/components/newtab/content-src` directory.

Building assets and running Firefox
```````````````````````````````````

To build assets and run Firefox, run the following from the root of the mozilla-central repository:

.. code-block:: shell

  npm run bundle --prefix browser/components/newtab && ./mach run

Running tests
`````````````

Mochi tests and xpcshell tests can be run normally. To run our additional unit tests, you can run the following:

.. code-block:: shell

  npm test --prefix browser/components/newtab

The Newtab team is currently responsible for fixing any test failures that result from changes
until these tests are running in Try, so this is currently an optional step.

GitHub workflow
---------------
The files in this directory, including vendor dependencies, are synchronized with the https://github.com/mozilla/activity-stream repository. If you prefer a GitHub-based workflow, you can look at the documentation there to learn more.
