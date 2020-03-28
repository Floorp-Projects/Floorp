Defect severity
===============

Definition
----------

We use the ``severity`` field in Bugzilla to indicate the scope of a
bug's effect on Firefox.

The field is display alongside the bug's priority.

.. image:: screenshot-severity.png
   :alt: Screenshot of severity field


Levels
------

`Severities are
enumerated <https://wiki.mozilla.org/BMO/UserGuide/BugFields#severity>`__
as:

-  ``blocker`` Broken important user-facing feature, Blocks development
   and/or testing work
-  ``critical`` Affecting a large number of users (all users on AMD64,
   Windows, MacOS, Linux), or major areas of functionality (tls, DOM,
   JavaScript, FxA, Add-ons)
-  ``normal`` Default; Regular issue, some loss of functionality under
   specific circumstances
-  ``minor`` Affecting a small number of users (i.e. ArchLinux users on
   PowerPC), a problem with an easy workaround, or a cosmetic issue such
   as misspellings or text alignment

By default, all bugs have a severity of ``normal``.

Rules of thumb
--------------

-  *A crash may be be a critical defect, but not all crashes are
   critical defects*
-  If a bug's severity is ``critical`` or ``blocker`` then it **must**
   have an ``assignee``
-  Enhancement bug severity is ``normal``

The ``severity`` field vs the ``tracking_firefoxNN`` field
----------------------------------------------------------

The ``tracking_firefoxNN`` field is used by Release Management.

A bug with ``tracking_firefoxNN`` of ``blocking`` does not mean the the
bug's ``severity`` is ``blocking``, just that the bug is needed to be
``RESOLVED`` as ``FIXED`` or ``VERIFIED`` for a release train.
