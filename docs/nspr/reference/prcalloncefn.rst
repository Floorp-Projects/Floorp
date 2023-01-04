PRCallOnceFN
============

Defines the signature of the function a client must implement.


Syntax
------

.. code::

   #include <prinit.h>

   typedef PRStatus (PR_CALLBACK *PRCallOnceFN)(void);


Description
-----------

The function is called to perform the initialization desired. The
function is expected to return a :ref:`PRStatus` indicating the outcome of
the process.
