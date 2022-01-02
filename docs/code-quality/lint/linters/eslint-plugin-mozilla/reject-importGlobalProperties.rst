reject-importGlobalProperties
=============================

Rejects calls to ``Cu.importGlobalProperties``. This is defined in the
recommended configuration to allow non-WebIDL imports, but reject others.

In some places in the tree, it will reject everything.
