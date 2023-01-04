PRCList
=======

A circular linked list.


Syntax
------

.. code::

   #include <prclist.h>

   typedef struct PRCListStr PRCList;

   typedef struct PRCListStr {
     PRCList *next;
     PRCList *previous;
   };


Description
-----------

PRClist defines a node in a circular linked list. It can be used as the
anchor of a list and can be embedded in data structures that are
maintained in a linked list.
