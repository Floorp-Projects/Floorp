PRLogModuleLevel
================

The enumerated type :ref:`PRLogModuleLevel` defines levels of logging
available to application programs.


Syntax
------

::

   #include <prlog.h>

   typedef enum PRLogModuleLevel {
      PR_LOG_NONE = 0,
      PR_LOG_ALWAYS = 1,
      PR_LOG_ERROR = 2,
      PR_LOG_WARNING = 3,
      PR_LOG_DEBUG = 4,

      PR_LOG_NOTICE = PR_LOG_DEBUG,
      PR_LOG_WARN = PR_LOG_WARNING,
      PR_LOG_MIN = PR_LOG_DEBUG,
      PR_LOG_MAX = PR_LOG_DEBUG
   } PRLogModuleLevel;
