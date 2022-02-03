This chapter describes the NSPR API for managing linked lists. The API
is a set of macros for initializing a circular (doubly linked) list,
inserting and removing elements from the list. The macros are not thread
safe. The caller must provide for mutually-exclusive access to the list,
and for the nodes being added and removed from the list.

-  `Linked List Types <#Linked_List_Types>`__
-  `Linked List Macros <#Linked_List_Macros>`__

.. _Linked_List_Types:

Linked List Types
-----------------

The :ref:`PRCList` type represents a circular linked list.

.. _Linked_List_Macros:

Linked List Macros
------------------

Macros that create and operate on linked lists are:

 - :ref:`PR_INIT_CLIST`
 - :ref:`PR_INIT_STATIC_CLIST`
 - :ref:`PR_APPEND_LINK`
 - :ref:`PR_INSERT_LINK`
 - :ref:`PR_NEXT_LINK`
 - :ref:`PR_PREV_LINK`
 - :ref:`PR_REMOVE_LINK`
 - :ref:`PR_REMOVE_AND_INIT_LINK`
 - :ref:`PR_INSERT_BEFORE`
 - :ref:`PR_INSERT_AFTER`
 - :ref:`PR_CLIST_IS_EMPTY`
 - :ref:`PR_LIST_HEAD`
 - :ref:`PR_LIST_TAIL`
