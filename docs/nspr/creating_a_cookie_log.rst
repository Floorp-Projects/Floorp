Creating a cookie log
=====================

Creating a cookie log is often necessary to troubleshoot problems with
Firefox's cookie handling. If you are reading this, you have probably
been directed here from a bug report. Please follow the instructions
below to run Firefox with cookie logging enabled.

.. _Enabling_Cookie_Logging:

Enabling Cookie Logging
~~~~~~~~~~~~~~~~~~~~~~~

Windows
^^^^^^^

Open a command prompt (this is under Programs or Programs/Accessories in
normal installations of Windows).

#. Change to your Firefox directory (usually C:\Program Files\Mozilla
   Firefox)
#. Type "set NSPR_LOG_FILE=C:\temp\cookie-log.txt", enter
#. Type "set NSPR_LOG_MODULES=cookie:4" and press Enter
#. Run Firefox by typing "firefox.exe" and pressing Enter.

Linux
^^^^^

Start a command shell (these instructions are for bash, if you use
something else, you probably know how to modify these instructions
already).

#. Change to the installation directory for Firefox.
#. Type "export NSPR_LOG_FILE=~/cookie-log.txt" and press Enter.
#. Type "export NSPR_LOG_MODULES=cookie:4" and press Enter.
#. Run Firefox by typing "./firefox" and pressing Enter

macOS
^^^^^

Open Terminal.app, which is located in the /Applications/Utilities
folder (these instructions are for bash, the default shell in macOS
10.3 and higher; if you use something else, you probably know how to
modify these instructions already).

#. Change to the installation directory for Firefox, e.g. type "cd
   /Applications/Firefox.app/Contents/MacOS" and press Return.
#. Type "export NSPR_LOG_FILE=~/Desktop/cookie-log.txt" and press
   Return.
#. Type "export NSPR_LOG_MODULES=cookie:4" and press Return.
#. Run Firefox by typing "./firefox" and pressing Return (note that
   Firefox will launch behind windows for other applications).

Creating the Log
~~~~~~~~~~~~~~~~

Now that you have Firefox running with logging enabled, please try to
replicate the bug using the steps to reproduce from the bug report. Once
you have reproduced the bug, shut down Firefox. Close out of the command
prompt/shell/Terminal, and then launch Firefox normally. Finally, attach
the cookie-log.txt file to the bug where it was requested (by clicking
on Create New Attachment). It should be in C:\temp on Windows, your home
directory on Linux, or the Desktop on Mac OS X.

Thanks for helping us make Firefox better!
