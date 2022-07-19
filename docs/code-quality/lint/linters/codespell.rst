Codespell
=========

`codespell <https://github.com/codespell-project/codespell/>`__ is a popular tool to look for typical typos in the source code.

It is enabled mostly for the documentation and English locale files.

Run Locally
-----------

The mozlint integration of codespell can be run using mach:

.. parsed-literal::

    $ mach lint --linter codespell <file paths>


Configuration
-------------

To enable codespell on new directory, add the path to the include
section in the :searchfox:`codespell.yml <tools/lint/codespell.yml>` file.

This job is configured as `tier 2 <https://wiki.mozilla.org/Sheriffing/Job_Visibility_Policy#Overview_of_the_Job_Visibility_Tiers>`_.

Autofix
-------

Codespell provides a ``--fix`` option. It is based on the ``-w`` option provided by upstream.


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/codespell.yml>`
* :searchfox:`Source <tools/lint/spell/__init__.py>`
