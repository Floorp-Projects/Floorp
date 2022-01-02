File permission
===============

This linter verifies if a file has unnecessary permissions.
If a file has execution permissions (+x), file-perm will
generate a warning.

It will ignore files starting with ``#!`` for types of files
that typically have shebang lines (such as python, node or
shell scripts).

This linter does not have any affect on Windows.


Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter file-perm <file paths>


Configuration
-------------

This linter is enabled on the whole code base.

This job is configured as `tier 2 <https://wiki.mozilla.org/Sheriffing/Job_Visibility_Policy#Overview_of_the_Job_Visibility_Tiers>`_.

Autofix
-------

This linter provides a ``--fix`` option. The python script is doing the change itself.


Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/file-perm.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/file-perm/__init__.py>`_
