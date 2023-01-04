PRBool
======

Boolean value.


Syntax
~~~~~~

.. code::

   #include <prtypes.h>

   typedef enum { PR_FALSE = 0, PR_TRUE = 1 } PRBool;


Description
~~~~~~~~~~~

Wherever possible, do not use PRBool in Mozilla C++ code. Use standard
C++ ``bool`` instead.

Otherwise, use :ref:`PRBool` for variables and parameter types. Use
``PR_FALSE`` and ``PR_TRUE`` for clarity of target type in assignments
and actual arguments. Use ``if (bool)``, ``while (!bool)``,
``(bool) ? x : y``, and so on to test Boolean values, just as you would
C ``int``-valued conditions.
