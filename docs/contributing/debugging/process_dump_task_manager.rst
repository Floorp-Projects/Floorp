How to get a process dump with Windows Task Manager
===================================================

Introduction
------------

When tracking down the causes of process hangs, it is often helpful to
obtain a process dump while the process is experiencing a hang. This
article describes how to get a process dump with Task Manager on
Windows. (To get a process dump for Thunderbird or some other product,
substitute the product name where ever you see Firefox in these
instructions.)


Caution
-------

The memory dump that will be created through this process is a complete
snapshot of the state of Firefox when you create the file, so it
contains URLs of active tabs, history information, and possibly even
passwords depending on what you are doing when the snapshot is taken. It
is advisable to create a new, blank profile to use when reproducing the
hang and capturing the memory dump. Please ask for help doing this!


Requirements
------------

Windows
   To get a process dump, you need to be using Windows Vista or above.
A Firefox nightly or release
   You need a Firefox version for which symbols are available from the
   :ref:`symbol server <Using The Mozilla Symbol Server>`. You
   can use any `official nightly
   build <https://ftp.mozilla.org/pub/firefox/nightly/>`__ or released
   version of Firefox from Mozilla. You can find the latest trunk
   nightly builds under
   `http://ftp.mozilla.org/pub/mozilla.o.../latest-trunk/ <http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/>`__.


Creating the Dump File
----------------------

Ensure that Firefox is not already running.


Run Firefox, reproduce the hang
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Start Firefox and perform whatever steps are necessary to cause Firefox
to hang. Once the browser hangs, continue with the steps below.


After the hang
~~~~~~~~~~~~~~

#. Open Windows Task Manager (CTRL+SHIFT+ESC).
#. Find Firefox.exe among the list of processes.
#. Right-click Firefox.exe and select "Create dump file". Task manager
   should indicate where the dump file was written to.


See also
--------

-  :ref:`How to get a stacktrace for a bug report <How to get a stacktrace for a bug report>`
-  `How to create a user-mode process dump file in Windows Vista and in
   Windows 7
   (MSDN) <https://docs.microsoft.com/en-us/windows/client-management/generate-kernel-or-complete-crash-dump#manually-generate-a-memory-dump-file>`__
