PR_LocalTimeParameters
======================

Returns the time zone offset information that maps the specified
:ref:`PRExplodedTime` to local time.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prtime.h>

   PRTimeParameters PR_LocalTimeParameters (
      const PRExplodedTime *gmt);

.. _Parameter:

Parameter
~~~~~~~~~

``gmt``
   A pointer to the clock/calendar time whose offsets are to be
   determined. This time should be specified in GMT.

.. _Returns:

Returns
~~~~~~~

A time parameters structure that expresses the time zone offsets at the
specified time.

.. _Description:

Description
-----------

This is a frequently-used time parameter callback function. You don't
normally call it directly; instead, you pass it as a parameter to
``PR_ExplodeTime()`` or ``PR_NormalizeTime()``.
