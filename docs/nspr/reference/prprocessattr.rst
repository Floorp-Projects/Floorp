PRProcessAttr
=============

Represents the attributes of a new process.


Syntax
------

::

   #include <prproces.h>

   typedef struct PRProcessAttr PRProcessAttr;


Description
-----------

This opaque structure describes the attributes of a process to be
created. Pass a pointer to a :ref:`PRProcessAttr` into ``PR_CreateProcess``
when you create a new process, specifying information such as standard
input/output redirection and file descriptor inheritance.
