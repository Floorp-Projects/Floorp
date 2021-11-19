Trojan Source
=============

This linter verifies if a change is using some invalid unicode.

The goal of this linter is to identify some potential usage of this
technique:

https://trojansource.codes/

The code is inspired by the Red Hat script published:

https://access.redhat.com/security/vulnerabilities/RHSB-2021-007#diagnostic-tools

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter trojan-source <file paths>


Configuration
-------------

This linter is enabled on most of the code base on C/C++, Python and Rust.

Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/trojan-source.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/trojan-source/__init__.py>`_
