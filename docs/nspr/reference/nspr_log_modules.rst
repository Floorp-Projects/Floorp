NSPR_LOG_MODULES
================

This environment variable specifies which log modules have logging
enabled.


Syntax
------

::

   moduleName:level[, moduleName:level]*

*moduleName* is the name specified in a
`:ref:`PR_NewLogModule` <http://www-archive.mozilla.org/projects/nspr/reference/html/prlog.html#25372>`__
call or one of the handy magic names listed below.

*level* is a numeric value between 0 and 5, with the values having the
following meanings:

-  0 = PR_LOG_NONE: nothing should be logged
-  1 = PR_LOG_ALWAYS: important; intended to always be logged
-  2 = PR_LOG_ERROR: errors
-  3 = PR_LOG_WARNING: warnings
-  4 = PR_LOG_DEBUG: debug messages, notices
-  5: everything!


Description
-----------

Specify a ``moduleName`` that is associated with the ``name`` argument
in a call to
`:ref:`PR_NewLogModule` <http://www-archive.mozilla.org/projects/nspr/reference/html/prlog.html#25372>`__
and a non-zero ``level`` value to enable logging for the named
``moduleName``.

Special log module names are provided for controlling NSPR's log service
at execution time. These controls should be set in the
:ref:`NSPR_LOG_MODULES` environment variable at execution time to affect
NSPR's log service for your application.

-  **all** The name ``all`` enables all log modules. To enable all log
   module calls to
   ```PR_LOG`` <http://www-archive.mozilla.org/projects/nspr/reference/html/prlog.html#25497>`__,
   set the variable as follows:

   ::

      set NSPR_LOG_MODULES=all:5

-  **timestamp** Including ``timestamp`` results in a timestamp of the
   form "2015-01-15 21:24:26.049906 UTC - " prefixing every logged line.

-  **append** Including ``append`` results in log entries being appended
   to the existing contents of the file referenced by NSPR_LOG_FILE.  If
   not specified, the existing contents of NSPR_LOG_FILE will be lost as
   a new file is created with the same filename.

-  **sync** The name ``sync`` enables unbuffered logging.   This ensures
   that all log messages are flushed to the operating system as they are
   written, but may slow the program down.

-  **bufsize:size** The name ``bufsize:``\ *size* sets the log buffer to
   *size*.

Examples
--------

Log everything from the Toolkit::Storage component that happens,
prefixing each line with the timestamp when it was logged to the file
/tmp/foo.log (which will be replaced each time the executable is run).

::

   set NSPR_LOG_MODULES=timestamp,mozStorage:5
   set NSPR_LOG_FILE=/tmp/foo.log

.. _Logging_with_Try_Server:

Logging with Try Server
-----------------------

-  For **mochitest**, edit variable :ref:`NSPR_LOG_MODULES` in
   ``testing/mochitest/runtests.py`` before pushing to try. You would be
   able to download the log file as an artifact from the Log viewer.
-  (other tests?)
