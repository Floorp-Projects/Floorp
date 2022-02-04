NSPR_LOG_FILE
=============

This environment variable specifies the file to which log messages are
directed.


Syntax
------

::

   filespec

*filespec* is a filename. The exact syntax is platform specific.


Description
-----------

Use this environment variable to specify a log file other than the
default. If :ref:`NSPR_LOG_FILE` is not in the environment, then log output
is written to ``stdout`` or ``stderr``, depending on the platform.  Set
:ref:`NSPR_LOG_FILE` to the name of the log file you want to use. NSPR
logging, when enabled, writes to the file named in this environment
variable.

For MS Windows systems, you can set :ref:`NSPR_LOG_FILE` to the special
(case-sensitive) value ``WinDebug``. This value causes logging output to
be written using the Windows function ``OutputDebugString()``, which
writes to the debugger window.
