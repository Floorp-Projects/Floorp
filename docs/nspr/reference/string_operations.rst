This chapter describes some of the key NSPR functions for manipulating
strings. Libraries built on top of NSPR, such as the Netscape security
libraries, use these functions to manipulate strings. If you are copying
or examining strings for use by such libraries or freeing strings that
were allocated by such libraries, you must use these NSPR functions
rather than the libc equivalents.

 - :ref:`PL_strlen`
 - :ref:`PL_strcpy`
 - :ref:`PL_strdup`
 - :ref:`PL_strfree`
