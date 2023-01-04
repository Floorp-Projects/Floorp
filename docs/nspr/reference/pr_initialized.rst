PR_Initialized
==============

Checks whether the runtime has been initialized.


Syntax
------

.. code::

   #include <prinit.h>

   PRBool PR_Initialized(void);


Returns
~~~~~~~

The function returns one of the following values:

-  If :ref:`PR_Init` has already been called, ``PR_TRUE``.
-  If :ref:`PR_Init` has not already been called, ``PR_FALSE``.
