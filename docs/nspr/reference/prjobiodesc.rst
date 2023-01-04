PRJobIoDesc
===========


Syntax
------

.. code::

   #include <prtpool.h>

   typedef struct PRJobIoDesc {
     PRFileDesc *socket;
     PRErrorCode error;
     PRIntervalTime timeout;
   } PRJobIoDesc;
