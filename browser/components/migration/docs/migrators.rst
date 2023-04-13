===================
Migrators Reference
===================

There are currently two types of migrators: browser migrators, and file migrators. Browser migrators will migrate various resources from another browser. A file migrator allows the user to migrate data through an intermediary file (like passwords from a .CSV file).

Browser migrators
=================

MigratorBase class
------------------
.. js:autoclass:: MigratorBase
  :members:

Chrome and Chrome variant migrators
-----------------------------------

The ``ChromeProfileMigrator`` is subclassed ino order to provide migration capabilities for variants of the Chrome browser.

ChromeProfileMigrator class
===========================
.. js:autoclass:: ChromeProfileMigrator
  :members:

BraveProfileMigrator class
==========================
.. js:autoclass:: BraveProfileMigrator
  :members:

CanaryProfileMigrator class
===========================
.. js:autoclass:: CanaryProfileMigrator
  :members:

ChromeBetaMigrator class
========================
.. js:autoclass:: ChromeBetaMigrator
  :members:

ChromeDevMigrator class
=======================
.. js:autoclass:: ChromeDevMigrator
  :members:

Chromium360seMigrator class
===========================
.. js:autoclass:: Chromium360seMigrator
  :members:

ChromiumEdgeMigrator class
==========================
.. js:autoclass:: ChromiumEdgeMigrator
  :members:

ChromiumEdgeBetaMigrator class
==============================
.. js:autoclass:: ChromiumEdgeBetaMigrator
  :members:

ChromiumProfileMigrator class
=============================
.. js:autoclass:: ChromiumProfileMigrator
  :members:

OperaProfileMigrator class
==========================
.. js:autoclass:: OperaProfileMigrator
  :members:

OperaGXProfileMigrator class
============================
.. js:autoclass:: OperaGXProfileMigrator
  :members:

VivaldiProfileMigrator class
============================
.. js:autoclass:: VivaldiProfileMigrator
  :members:

EdgeProfileMigrator class
-------------------------
.. js:autoclass:: EdgeProfileMigrator
  :members:

FirefoxProfileMigrator class
----------------------------
.. js:autoclass:: FirefoxProfileMigrator
  :members:

IEProfileMigrator class
-----------------------
.. js:autoclass:: IEProfileMigrator
  :members:

File migrators
==============

.. js:autofunction:: FilePickerConfigurationFilter
   :short-name:

.. js:autofunction:: FilePickerConfiguration
   :short-name:

FileMigratorBase class
----------------------
.. js:autoclass:: FileMigratorBase
  :members:

PasswordFileMigrator class
--------------------------
.. js:autoclass:: PasswordFileMigrator
  :members:
