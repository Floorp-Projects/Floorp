==========================
Migration Wizard Reference
==========================

The migration wizard is the piece of UI that allows users to migrate from other browsers to Firefox. Firefox has had a legacy migration wizard for many years, and this has historically been a top-level XUL dialog window. **This documentation is not for the legacy migration wizard**, but is instead for an in-progress replacement migration wizard that can be embedded in the following contexts:

1. In a top level stand-alone dialog window
2. In a tab dialog modal
3. Within privileged ``about:`` pages, like ``about:welcome``, and ``about:preferences``

To accommodate these contexts, the new migration wizard was developed as a reusable component using pure HTML, with an architecture that decouples the control of the wizard from how the wizard is presented to the user. This architecture not only helps to ensure that the wizard can function similarly in these different contexts, but also makes the component viewable in tools like Storybook for easier development.

High-level Overview
-------------------

The following diagram tries to illustrate how the pieces of the migration wizard fit together:

.. image:: migration-wizard-architecture-diagram.svg

``MigrationWizard`` reusable component
======================================

The ``MigrationWizard`` reusable component (``<migration-wizard>``) is a custom element that can be imported from ``migration-wizard.mjs``. If the module is imported into a DOM window context, then the custom element is also automatically registered for that document.

After binding to the document, the ``MigrationWizard`` dispatches a ``MigrationWizard:Init`` custom event, which causes a ``MigrationWizardChild`` to instantiate and be associated with it.

Notably, the ``MigrationWizard`` does not contain any internal logic or privileged code to perform any migrations or to directly interact with the migration mechanisms. Its sole function is to accept input from the user and emit that input as events. The associated ``MigrationWizardChild`` will listen for those events, and take care of calling into the ``MigrationWizard`` to update the state of the reusable component. This means that the reusable component can be embedded in unprivileged contexts and have its states presented in a tool like Storybook.

.. js:autoclass:: MigrationWizard
  :members:

``MigrationWizardChild``
=========================

The ``MigrationWizardChild`` is a ``JSWindowActorChild`` (see `JSActors`_) that is responsible for listening for events from a ``MigrationWizard``, and then either updating the state of that ``MigrationWizard`` immediately, or to message its paired ``MigrationWizardParent`` to perform tasks with ``MigrationUtils``.

  .. note::
    While a ``MigrationWizardChild`` can run in a content process (for out-of-process pages like ``about:welcome``), it can also run in parent-process contexts - for example, within a tab dialog or standalone window dialog. The same flow of events and messaging applies in all contexts.

The ``MigrationWizardChild`` also waives Xrays so that it can directly call the ``setState`` method to update the appearance of the ``MigrationWizard``. See `XrayVision`_ for much more information on Xrays.

.. js:autoclass:: MigrationWizardChild
  :members:

``MigrationWizardParent``
=========================

The ``MigrationWizardParent`` is a ``JSWindowActorParent`` (see `JSActors`_) that is responsible for listening for messages from the paired ``MigrationWizardChild`` to perform operations with ``MigrationUtils``. Effectively, all of the heavy lifting of actually performing the migrations will be kicked off by the ``MigrationWizardParent`` by calling into ``MigrationUtils``. State updates for things like migration progress will be sent back down to the ``MigrationWizardChild`` to then be reflected in the appearance of the ``MigrationWizard``.

Since the ``MigrationWizard`` might be embedded in unprivileged documents, additional checks are placed in the message handler for ``MigrationWizardParent`` to ensure that the document is either running in the parent process or the privileged about content process. The `JSActors`_ registration for ``MigrationWizardParent`` and ``MigrationWizardChild`` also ensures that the actors only load for built-in documents.

.. js:autoclass:: MigrationWizardParent
  :members:

``migration-dialog.html``
=========================

This document is meant for being loaded in a tab modal or window modal dialog, and embeds the ``MigrationWizard`` reusable component.

Pages like ``about:preferences`` or ``about:welcome`` can embed the ``MigrationWizard`` component directly, rather than use ``migration-dialog.html``.


.. _JSActors: /dom/ipc/jsactors.html
.. _XrayVision: /dom/scriptSecurity/xray_vision.html
