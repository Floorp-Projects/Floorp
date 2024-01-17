import-headjs-globals
=====================

Import globals from head.js and from any files that were imported by
head.js (as far as we can correctly resolve the path).

This rule is included in the test configurations.

The following file import patterns are supported:

-  ``Services.scriptloader.loadSubScript(path)``
-  ``loader.loadSubScript(path)``
-  ``loadSubScript(path)``
-  ``loadHelperScript(path)``
-  ``import-globals-from path``

If path does not exist because it is generated e.g.
``testdir + "/somefile.js"`` we do our best to resolve it.

The following patterns are supported:

-  ``Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");``
-  ``loader.lazyRequireGetter(this, "name2"``
-  ``loader.lazyServiceGetter(this, "name3"``
-  ``loader.lazyGetter(this, "toolboxStrings"``
-  ``ChromeUtils.defineLazyGetter(this, "clipboardHelper"``
