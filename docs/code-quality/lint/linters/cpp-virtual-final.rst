cpp virtual final
=================

This linter detects the virtual function declarations with multiple specifiers.

It matches our coding style:
Method declarations must use at most one of the following keywords: virtual, override, or final.

As this linter uses some simple regular expression, it can miss some declarations.

Run Locally
-----------

This linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter cpp-virtual-final <file paths>


Configuration
-------------

This linter is enabled on all C family files.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/cpp-virtual-final.yml>`_

