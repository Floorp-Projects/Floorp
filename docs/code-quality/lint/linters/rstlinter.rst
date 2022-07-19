RST Linter
==========

`rstcheck`_ is a popular linter for restructuredtext.


Run Locally
-----------

The mozlint integration of rst linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter rst <file paths>


Configuration
-------------

All directories will have rst linter run against them.
If you wish to exclude a subdirectory of an included one, you can add it to the ``exclude``
directive.


.. _rstcheck: https://github.com/myint/rstcheck


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/rst.yml>`
* :searchfox:`Source <tools/lint/rst/__init__.py>`
