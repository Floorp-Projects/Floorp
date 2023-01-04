PRLibrary
=========

An opaque structure identifying a library.


Syntax
------

.. code::

   #include <prlink.h>

   typedef struct PRLibrary PRLibrary;


Description
-----------

A PRLibrary is an opaque structure. A reference to such a structure can
be returned by some of the functions in the runtime and serve to
identify a particular instance of a library.
