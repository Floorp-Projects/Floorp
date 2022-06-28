Spotless
========

`Spotless <https://github.com/diffplug/spotless>`__ is a pluggable formatter
for Gradle and Android.

In our current configuration, Spotless includes the
`Google Java Format plug-in <https://github.com/google/google-java-format>`__
which formats all our Java code using the Google Java coding style guidelines.


Run Locally
-----------

The mozlint integration of spotless can be run using mach:

.. parsed-literal::

    $ mach lint --linter android-format

Alternatively, omit the ``--linter android-format`` and run all configured linters, which will include
spotless.


Autofix
-------

The spotless linter provides a ``--fix`` option.
