Test Manifest TOML
==================

This linter verifies syntax for ManifestParser TOML files.

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter test-manifest-toml <file paths>


Configuration
-------------

The configuration excludes all non-ManifestParser TOML files (as well as
generated TOML manifests).

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/test-manifest-toml.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/test-manifest-toml/__init__.py>`_

Errors Detected
---------------
* Invalid TOML
* Missing DEFAULT section (fixable)
* Sections not in alphabetical order (fixable)
* Section name not double quoted (fixable)
* Disabling a path by commenting out the section
* Conditional contains explicit ||
