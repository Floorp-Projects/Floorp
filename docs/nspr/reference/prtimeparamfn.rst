PRTimeParamFn
=============

This type defines a callback function to calculate and return the time
parameter offsets from a calendar time object in GMT.


Syntax
------

.. code::

    #include <prtime.h>

    typedef PRTimeParameters (PR_CALLBACK_DECL *PRTimeParamFn)
       (const PRExplodedTime *gmt);


Description
-----------

The type :ref:`PRTimeParamFn` represents a callback function that, when
given a time instant in GMT, returns the time zone information (offset
from GMT and DST offset) at that time instant.
