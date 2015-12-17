Assuming that acorn's dependencies have not changed, to upgrade our tree's
acorn to a new version:

1. Clone the acorn repository, and check out the version you want to upgrade
to:

       $ git clone https://github.com/marijnh/acorn.git
       $ cd acorn
       $ git checkout <version>

2. Make sure that all tests pass:

       $ npm install .
       $ npm test

   If there are any test failures, do not upgrade to that version of acorn!

3. Copy acorn.js to our tree:

       $ cp acorn.js /path/to/mozilla-central/devtools/shared/acorn/acorn.js

4. Copy acorn_loose.js to our tree:

       $ cp acorn_loose.js /path/to/mozilla-central/devtools/shared/acorn/acorn_loose.js

5. Copy util/walk.js to our tree:

       $ cp util/walk.js /path/to/mozilla-central/devtools/shared/acorn/walk.js

6. Check and see if javascript pretty-printing and scratchpad work without any errors.  As of version 2.6.4 we need to comment out lines in acorn_loose.js that attempt to extend the acorn object, like `acorn.parse_dammit = parse_dammit`.

