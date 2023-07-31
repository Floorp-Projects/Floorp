Places
======

This document describes the implementation of the Firefox Places component.

It is a robust system to manage History and Bookmarks through a database on the backend side and a model-view-controller system that connects frontend UI user manipulation.

History and Bookmarks
---------------------

In Firefox 2, History and Bookmarks used to be kept into separate databases in the Resource Description Framework format (`RDF format`_).

However, Firefox 3 implemented the Places system. It has been done due to multiple reasons, such as:

  * **Performance**: certain search or maintenance operations were very slow
  * **Reliability**: the filesystem facing side of RDF was not so robust, often causing corruption or unexpected states
  * **Flexibility**: being able to cross data allows for interesting features, like the Awesome Bar
  * **Maintainability**: the future of RDF was unclear, while SQLite is actively maintained and used by a large number of applications

  .. _RDF format: https://en.wikipedia.org/wiki/Resource_Description_Framework

Where to Start
--------------

For the high-level, non-technical summary of how History and Bookmarks works, read :doc:`nontechnical-overview`.
For more specific, technical details of implementation, read :doc:`architecture-overview`.

Governance
----------

See `Bookmarks & History (Places) </mots/index.html#bookmarks-history>`__

Table of Contents
-----------------

.. toctree::
   :maxdepth: 2

   nontechnical-overview
   architecture-overview
