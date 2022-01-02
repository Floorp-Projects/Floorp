License
=======

This linter verifies if a file has a known license header.

By default, Firefox uses MPL-2 license with the `appropriate headers <https://www.mozilla.org/en-US/MPL/headers/>`_.
In some cases (thirdpardy code), a file might have a different header file.
If this is the case, one of the significant line of the header should be listed in the list `of valid licenses
<https://searchfox.org/mozilla-central/source/tools/lint/license/valid-licenses.txt>`_.

Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter license <file paths>


Configuration
-------------

This linter is enabled on most of the whole code base.

Autofix
-------

This linter provides a ``--fix`` option. The python script is doing the change itself
and will use the right header MPL-2 header depending on the language.
It will add the license at the right place in case the file is a script (ie starting with ``!#``
or a XML file ``<?xml>``).


Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/license.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/license/__init__.py>`_
