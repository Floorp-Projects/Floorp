PRStaticLinkTable
=================

A static link table entry can be created by a client of the runtime so
that other clients can access static or dynamic libraries transparently.
The basic function on a dynamic library is to acquire a pointer to a
function that the library exports. If, during initialization, such
entries are manually created, then future attempts to link to the
symbols can be treated in a consistent fashion.


Syntax
------

.. code::

   #include <prlink.h>

   typedef struct PRStaticLinkTable {
       const char *name;
       void (*fp)();
   } PRStaticLinkTable;
