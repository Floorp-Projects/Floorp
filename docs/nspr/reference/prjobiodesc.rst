PRJobIoDesc
===========


Syntax
------

.. code:: eval

   #include <prtpool.h>

   typedef struct PRJobIoDesc {
     PRFileDesc *socket;
     PRErrorCode error;
     PRIntervalTime timeout;
   } PRJobIoDesc;
