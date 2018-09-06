
# Developing GCLI

## About the code

The majority of the GCLI source is stored in the ``lib`` directory.

The ``docs`` directory contains documentation.
The ``scripts`` directory contains RequireJS that GCLI uses.
The ``build`` directory contains files used when creating builds.
The ``mozilla`` directory contains the mercurial patch queue of patches to apply
to mozilla-central.
The ``selenium-tests`` directory contains selenium web-page integration tests.

The source in the ``lib`` directory is split into 4 sections:

- ``lib/demo`` contains commands used in the demo page. It is not needed except
  for demo purposes.
- ``lib/test`` contains a small test harness for testing GCLI.
- ``lib/gclitest`` contains tests that run in the test harness
- ``lib/gcli`` contains the actual meat

GCLI is split into a UI portion and a Model/Controller portion.


## The GCLI Model

The heart of GCLI is a ``Requisition``, which is an AST for the input. A
``Requisition`` is a command that we'd like to execute, and we're filling out
all the inputs required to execute the command.

A ``Requisition`` has a ``Command`` that is to be executed. Each Command has a
number of ``Parameter``s, each of which has a name and a type as detailed
above.

As you type, your input is split into ``Argument``s, which are then assigned to
``Parameter``s using ``Assignment``s. Each ``Assignment`` has a ``Conversion``
which stores the input argument along with the value that is was converted into
according to the type of the parameter.

There are special assignments called ``CommandAssignment`` which the
``Requisition`` uses to link to the command to execute, and
``UnassignedAssignment``used to store arguments that do not have a parameter
to be assigned to.


## The GCLI UI

There are several components of the GCLI UI. Each can have a script portion,
some template HTML and a CSS file. The template HTML is processed by
``domtemplate`` before use.

DomTemplate is fully documented in [it's own repository]
(https://github.com/joewalker/domtemplate).

The components are:

- ``Inputter`` controls the input field, processing special keyboard events and
  making sure that it stays in sync with the Requisition.
- ``Completer`` updates a div that is located behind the input field and used
  to display completion advice and hint highlights. It is stored in
  completer.js.
- ``Display`` is responsible for containing the popup hints that are displayed
  above the command line. Typically Display contains a Hinter and a RequestsView
  although these are not both required. Display itself is optional, and isn't
  planned for use in the first release of GCLI in Firefox.
- ``Hinter`` Is used to display input hints. It shows either a Menu or an
  ArgFetch component depending on the state of the Requisition
- ``Menu`` is used initially to select the command to be executed. It can act
  somewhat like the Start menu on windows.
- ``ArgFetch`` Once the command to be executed has been selected, ArgFetch
  shows a 'dialog' allowing the user to enter the parameters to the selected
  command.
- ``RequestsView`` Contains a set of ``RequestView`` components, each of which
  displays a command that has been invoked. RequestsView is a poor name, and
  should better be called ReportView

ArgFetch displays a number of Fields. There are fields for most of the Types
discussed earlier. See 'Writing Fields' above for more information.


## Testing

GCLI contains 2 test suites:

- JS level testing is run with the ``test`` command. The tests are located in
  ``lib/gclitest`` and they use the test runner in ``lib/test``. This is fairly
  comprehensive, however it does not do UI level testing.
  If writing a new test it needs to be registered in ``lib/gclitest/index``.
  For an example of how to write tests, see ``lib/gclitest/testSplit.js``.
  The test functions are implemented in ``lib/test/assert``.
- Browser integration tests are included in ``browser_webconsole_gcli_*.js``,
  in ``toolkit/components/console/hudservice/tests/browser``. These are
  run with the rest of the Mozilla test suite.


## Coding Conventions

The coding conventions for the GCLI project come from the Bespin/Skywriter and
Ace projects. They are roughly [Crockford]
(http://javascript.crockford.com/code.html) with a few exceptions and
additions:

* ``var`` does not need to be at the top of each function, we'd like to move
  to ``let`` when it's generally available, and ``let`` doesn't have the same
  semantic twists as ``var``.

* Strings are generally enclosed in single quotes.

* ``eval`` is to be avoided, but we don't declare it evil.

The [Google JavaScript conventions]
(https://google-styleguide.googlecode.com/svn/trunk/javascriptguide.xml) are
more detailed, we tend to deviate in:

* Custom exceptions: We generally just use ``throw new Error('message');``

* Multi-level prototype hierarchies: Allowed; we don't have ``goog.inherits()``

* ``else`` begins on a line by itself:

        if (thing) {
          doThis();
        }
        else {
          doThat();
        }


## Startup

Internally GCLI modules have ``startup()``/``shutdown()`` functions which are
called on module init from the top level ``index.js`` of that 'package'.

In order to initialize a package all that is needed is to require the package
index (e.g. ``require('package/index')``).

The ``shutdown()`` function was useful when GCLI was used in Bespin as part of
dynamic registration/de-registration. It is not known if this feature will be
useful in the future. So it has not been entirely removed, it may be at some
future date.


## Running the Unit Tests

Start the GCLI static server:

    cd path/to/gcli
    node gcli.js

Now point your browser to http://localhost:9999/localtest.html. When the page
loads the tests will be automatically run outputting to the console, or you can
enter the ``test`` command to run the unit tests.


## Contributing Code

Please could you do the following to help minimize the amount of rework that we
do:

1. Check the unit tests run correctly (see **Running the Unit Tests** above)
2. Check the code follows the style guide. At a minimum it should look like the
   code around it. For more detailed notes, see **Coding Conventions** above
3. Help me review your work by using good commit comments. Which means 2 things
   * Well formatted messages, i.e. 50 char summary including bug tag, followed
     by a blank line followed by a more in-depth message wrapped to 72 chars
     per line. This is basically the format used by the Linux Kernel. See the
     [commit log](https://github.com/joewalker/gcli/commits/master) for
     examples. The be extra helpful, please use the "shortdesc-BUGNUM: " if
     possible which also helps in reviews.
   * Commit your changes as a story. Make it easy for me to understand the
     changes that you've made.
4. Sign your work. To improve tracking of who did what, we follow the sign-off
   procedure used in the Linux Kernel.
   The sign-off is a simple line at the end of the explanation for the
   patch, which certifies that you wrote it or otherwise have the right to
   pass it on as an open-source patch.  The rules are pretty simple: if you
   can certify the below:

        Developer's Certificate of Origin 1.1

        By making a contribution to this project, I certify that:

        (a) The contribution was created in whole or in part by me and I
            have the right to submit it under the open source license
            indicated in the file; or

        (b) The contribution is based upon previous work that, to the best
            of my knowledge, is covered under an appropriate open source
            license and I have the right under that license to submit that
            work with modifications, whether created in whole or in part
            by me, under the same open source license (unless I am
            permitted to submit under a different license), as indicated
            in the file; or

        (c) The contribution was provided directly to me by some other
            person who certified (a), (b) or (c) and I have not modified
            it.

        (d) I understand and agree that this project and the contribution
            are public and that a record of the contribution (including all
            personal information I submit with it, including my sign-off) is
            maintained indefinitely and may be redistributed consistent with
            this project or the open source license(s) involved.

   then you just add a line saying

        Signed-off-by: Random J Developer <random@developer.example.org>

   using your real name (sorry, no pseudonyms or anonymous contributions.)

Thanks for wanting to contribute code.

