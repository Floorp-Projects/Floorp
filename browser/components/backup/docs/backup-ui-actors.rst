==========================
Backup UI Actors Reference
==========================

The ``BackupUIParent`` and ``BackupUIChild`` actors allow UI widgets to access
the current state of the ``BackupService`` and to subscribe to state updates.

UI widgets that want to subscribe to state updates must ensure that they are
running in a process and on a page that the ``BackupUIParent/BackupUIChild``
actor pair are registered for, and then fire a ``BackupUI::InitWidget`` event.

It is expected that these UI widgets will respond to having their
``backupServiceState`` property set.

.. js:autoclass:: BackupUIParent
  :members:
  :private-members:

.. js:autoclass:: BackupUIChild
.. js::autoattribute:: BackupUIChild#inittedWidgets
  :members:
  :private-members:
