Rather than make changes to the built files here, make them upstream and then
upgrade our tree's copy of the built files.

To upgrade the source-map library:

    $ git clone https://github.com/mozilla/source-map.git
    $ cd source-map
    $ git checkout <latest-tagged-version>
    $ npm install
    $ npm run-script build  -or-  nodejs Makefile.dryice.js (if you have issues with npm)
    $ cp dist/source-map.js /path/to/mozilla-central/devtools/shared/sourcemap/source-map.js
    $ cp dist/test/* /path/to/mozilla-central/devtools/shared/sourcemap/tests/unit/

