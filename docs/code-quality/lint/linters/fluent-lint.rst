Fluent Lint
===========

Fluent lint is a linter for Fluent files (.ftl). Currently, it includes:

* Checks for invalid typography in messages (e.g. straight single or double quotes).
* Checks for comments layout.
* Checks for identifiers (minimum length, allowed characters).
* Hard-coded brand names.


Run Locally
-----------

The mozlint integration of fluent-lint can be run using mach:

.. parsed-literal::

    $ mach lint --linter fluent-lint <file paths>

Alternatively, omit the ``--linter fluent-lint`` and run all configured linters, which will include
fluent-lint.


Run on Taskcluster
------------------

The fluent-lint job shows up as text(fluent) in the linting job. It should run automatically if
changes are made to fluent (ftl) files.


Configuration
-------------

The main configuration file is found in :searchfox:`tools/lint/fluent-lint/exclusions.yml`. This provides
a way of excluding identifiers or files from checking. In general, exclusions are only to be
used for identifiers that are generated programmatically, but unfortunately, there are other
exclusions that are required for historical reasons. In almost all cases, it should *not* be
necessary to add new exclusions to this file.


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/fluent-lint.yml>`
* :searchfox:`Source <tools/lint/fluent-lint/__init__.py>`
* :searchfox:`Test <tools/lint/test/test_fluent_lint.py>`
