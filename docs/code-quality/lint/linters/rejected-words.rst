Rejected words
==============

Reject some words we don't want to use in the code base for various reasons.

Run Locally
-----------

The mozlint integration of codespell can be run using mach:

.. parsed-literal::

    $ mach lint --linter rejected-words <file paths>


Configuration
-------------

This linter is enabled on the whole code base. Issues existing in the code base
are listed in the exclude list in the :searchfox:`rejected-words.yml
<tools/lint/rejected-words.yml>` file.

New words can be added in the `payload` section.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/rejected-words.yml>`
