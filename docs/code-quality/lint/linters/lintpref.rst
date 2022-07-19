Lintpref
========

The lintpref linter is a simple linter for libpref files to check for duplicate
entries between :searchfox:`modules/libpref/init/all.js` and
:searchfox:`modules/libpref/init/StaticPrefList.yaml`. If a duplicate is found,
lintpref will raise an error and emit the ``all.js`` line where you can find
the duplicate entry.


Running Locally
---------------

The linter can be run using mach:

 .. parsed-literal::

     $ mach lint --linter lintpref


Fixing Lintpref Errors
----------------------

In most cases, duplicate entries should be avoided and the duplicate removed
from ``all.js``. If for any reason a pref should exist in both files, the pref
should be added to ``IGNORE_PREFS`` in :searchfox:`tools/lint/libpref/__init__.py`.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/lintpref.yml>`
* :searchfox:`Source <tools/lint/libpref/__init__.py>`
