Trailing whitespaces
====================

This linter verifies if a file has unnecessary trailing whitespaces, Windows
carriage return or empty lines at the end of file.


Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter file-whitespace <file paths>


Configuration
-------------

This linter is enabled on most of the code base.

This job is configured as `tier 2 <https://wiki.mozilla.org/Sheriffing/Job_Visibility_Policy#Overview_of_the_Job_Visibility_Tiers>`_.

Autofix
-------

This linter provides a ``--fix`` option. The python script is doing the change itself.
For now, fix empty lines at end of file isn't supported.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/file-whitespace.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/file-whitespace/__init__.py>`_
