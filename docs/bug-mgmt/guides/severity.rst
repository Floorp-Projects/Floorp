Defect Severity
===============

Definition
----------

We use the ``severity`` field in Bugzilla to indicate the scope of a
bug's effect on Firefox.

The field is display alongside the bug's priority.

.. image:: screenshot-severity.png
   :alt: Screenshot of severity field


Values
------

`Severities are
enumerated <https://wiki.mozilla.org/BMO/UserGuide/BugFields#severity>`__
as:

-  ``--``: Default for new bugs
-  ``N/A``: (not applicable): Only applies to bugs of type Task or Enhancement.
-  ``S1``: (Catastrophic) Blocks development/testing, may impact more than 25%
     of users, causes data loss, potential chemspill, and no workaround available
-  ``S2``: (Serious) Major Functionality/product severely impaired and a
     satisfactory workaround doesn't exist
-  ``S3``: (Normal) Blocks non-critical functionality and a work around exists
-  ``S4``: (Small/Trivial) minor significance, cosmetic issues, low or no impact to users

By default, new bugs have a severity of ``--``.

Rules of thumb
--------------

-  *A crash may be be a ``S1`` or ``S2`` defect, but not all crashes are
   critical defects*
-  If a bug's severity is ``S1`` or ``S2`` then it **must**
   have an ``assignee``
-  The severity of most bugs of type ``task`` and ``enhancement`` will be
   ``N/A``
-  **Do not** assign bugs of type ``defect`` the severity ``N/A``
