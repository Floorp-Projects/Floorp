User documentation requests
===========================

If you are working on a change (bugfix, enhancement, or feature) which
would benefit from user-facing documenation, please use the
``user-doc-firefox`` flag to request it.

This flag can be modified by anyone with ``EDITBUGS`` privileges.

.. figure:: /public/images/sumo-flag.png
   :alt: Image of flag in tracking section of bug

   Image of flag in tracking section of bug

The default value of the flag is ``---``.

If the bug needs user-facing documentation, set the flag to
``docs-needed``. This flag will be monitored by the support.mozilla.org
(SUMO) team.

Once the docs are ready to be published, set the flag to
``docs-completed``.

If it’s determined that documentation is not need after setting the flag
to ``docs-needed``, update the flag to ``none-needed`` so we know that
it’s been reviewed.

Summary
-------

=========== == ==============
From           To
=========== == ==============
—           to none-needed
—           to docs-needed
docs-needed to none-needed
docs-needed to docs-completed
=========== == ==============

Notes
-----

A flag is used instead of the old keywords because flags can be
restricted to a subset of products and components.
