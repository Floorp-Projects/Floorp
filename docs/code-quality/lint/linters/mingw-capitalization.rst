MinGW capitalization
====================

This linter verifies that Windows include file are lowercase.
It might break the mingw build otherwise.


Run Locally
-----------

This mozlint linter can be run using mach:

.. parsed-literal::

    $ mach lint --linter mingw-capitalization <file paths>


Configuration
-------------

This linter is enabled on the whole code base except WebRTC


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/mingw-capitalization.yml>`
* :searchfox:`Source <tools/lint/cpp/mingw-capitalization.py>`
