================================
Backup Resources Reference
================================

A ``BackupResource`` is the base class used to represent a group of data within
a user profile that is logical to backup together. For example, the
``PlacesBackupResource`` represents both the ``places.sqlite`` SQLite database,
as well as the ``favicons.sqlite`` database. The ``AddonsBackupResource``
represents not only the preferences for various addons, but also the XPI files
that those addons are defined in.

Each ``BackupResource`` subclass is registered for use by the
``BackupService`` by adding it to the default set of exported classes in the
``BackupResources`` module in ``BackupResources.sys.mjs``.

.. js:autoclass:: BackupResource
  :members:
  :private-members:
