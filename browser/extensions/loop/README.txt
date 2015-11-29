This is the directory for the Loop desktop implementation and the standalone
client.

The desktop implementation is the UX built into Firefox, activated by the Loop
button on the toolbar. The standalone client is the link-clicker UX for any
modern browser that supports WebRTC.

The standalone client is a set of web pages intended to be hosted on a
standalone server referenced by the loop-server.

The standalone client exists in standalone/ but shares items
(from content/shared/) with the desktop implementation. See the README.md
file in the standalone/ directory for how to run the server locally.

Working with React JSX files
============================

Our views use [React](http://facebook.github.io/react/) written in JSX files
and transpiled to JS before we commit. You need to install the JSX compiler
using npm in order to compile the .jsx files into regular .js ones:

    npm install -g react-tools@0.12.2

Once installed, run build-jsx with the --watch option from
browser/components/loop, eg.:

    cd browser/components/loop
    ./build-jsx --watch

build-jsx can also be do a one-time compile pass instead of watching if
the --watch argument is omitted.  Be sure to commit any transpiled files
at the same time as changes to their sources.


Hacking
=======
Please be sure to execute

  browser/components/loop/run-all-loop-tests.sh

from the top level before requesting review on a patch.

Linting
=======
run-all-loop-tests.sh will take care of this for you automatically, after
you've installed the dependencies by typing:

  ( cd standalone ; make install )

If you install eslint and the react plugin globally:

  npm install -g eslint
  npm install -g eslint-plugin-react

You can also run it by hand in the browser/components/loop directory:

  eslint --ext .js --ext .jsx --ext .jsm .

Test coverage
=============
Initial setup
  cd test
  npm install

To run
  npm run build-coverage

It will create a `coverage` folder under test/

Front-End Unit Tests
====================
The unit tests for Loop reside in three directories:

- test/desktop-local
- test/shared
- test/standalone

You can run these as part of the run-all-loop-tests.sh command above, or you can run these individually in Firefox. To run them individually, start the standalone client (see standalone/README.md) and load:

  http://localhost:3000/test/


Functional Tests
================
These are currently a work in progress, but it's already possible to run a test
if you have a [loop-server](https://github.com/mozilla-services/loop-server)
install that is properly configured.  From the top-level gecko directory,
execute:

  export LOOP_SERVER=/Users/larry/src/loop-server
  ./mach marionette-test browser/components/loop/test/functional/manifest.ini

Once the automation is complete, we'll include this in run-all-loop-tests.sh
as well.


UI-Showcase
===========
This is a tool giving the layouts for all the frontend views of Loop, allowing debugging and testing of css layouts and local component behavior.

To access it, start the standalone client (see standalone/README.md) and load:

  http://localhost:3000/ui/
