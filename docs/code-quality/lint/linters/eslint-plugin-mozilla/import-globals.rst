import-globals
==============

Checks the filename of imported files e.g. ``Cu.import("some/path/Blah.jsm")``
adds Blah to the global scope.

Note: uses modules.json for some files where there are multiple exports.
