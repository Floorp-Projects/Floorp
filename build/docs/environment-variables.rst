.. _environment_variables:

================================================
Environment Variables Impacting the Build System
================================================

Various environment variables have an impact on the behavior of the
build system. This document attempts to document them.

AUTOCLOBBER
   If defines, the build system will automatically clobber as needed.
   The default behavior is to print a message and error out when a
   clobber is needed.

   This variable is typically defined in a :ref:`mozconfig <mozconfig>`
   file via ``mk_add_options``.

REBUILD_CHECK
   If defined, the build system will print information about why
   certain files were rebuilt.

   This feature is disabled by default because it makes the build slower.

MACH_NO_TERMINAL_FOOTER
   If defined, the terminal footer displayed when building with mach in
   a TTY is disabled.

MACH_NO_WRITE_TIMES
   If defined, mach commands will not prefix output lines with the
   elapsed time since program start. This option is equivalent to
   passing ``--log-no-times`` to mach.
