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

No build step is necessary. Use `mach` and run mochitests according to your regular Firefox workflow.

For .js, .jsx, .sass, or .css files
-----------------------------------

Prerequisites
`````````````

You will need the following:

- Node.js 8+ (On Mac, the best way to install Node.js is to use the install link on the `Node.js homepage`_)
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
The majority of New Tab / Messaging unit tests are written using
`mocha <https://mochajs.org>`_, and other errors that may show up there are
`SCSS <https://sass-lang.com/documentation/syntax>`_ issues flagged by
`sasslint <https://github.com/sasstools/sass-lint/tree/master>`_.  These things
are all run using `npm test` under the `newtab` slug in Treeherder/Try, so if
that slug turns red, these tests are what is failing.  To execute them, do this:

.. code-block:: shell

  npm test --prefix browser/components/newtab

These tests are not currently run by `mach test`, but there's a
`task filed to fix that <https://bugzilla.mozilla.org/show_bug.cgi?id=1581165>`_.

Mochitests and xpcshell tests run normally, using `mach test`.

GitHub workflow
---------------
The files in this directory, including vendor dependencies, are synchronized with the https://github.com/mozilla/activity-stream repository. If you prefer a GitHub-based workflow, you can look at the documentation there to learn more.

..  _Node.js homepage: https://nodejs.org/
