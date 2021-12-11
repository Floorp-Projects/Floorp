Lintpref
========

The lintpref linter is a simple linter for libpref files to check for duplicate
entries between ``modules/libpref/init/all.js`` and ``modules/libpref/init/StaticPrefList.yaml``.
If a duplicate is found, lintpref will raise an error and emit the ``all.js`` line
where you can find the duplicate entry.


Running Locally
---------------

The linter can be run using mach:

 .. parsed-literal::

     $ mach lint --linter lintpref


Fixing Lintpref Errors
----------------------

In most cases, duplicate entries should be avoided and the duplicate removed
from ``all.js``. If for any reason a pref should exist in both files, the pref
should be added to ``IGNORE_PREFS`` in ``tools/lint/libpref/__init__.py``.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/lintpref.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/libpref/__init__.py>`_
