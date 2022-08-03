======================
Private Browsing Proxy
======================

On Windows, Firefox ships with a small binary that always launches
``firefox.exe`` in Private Browsing mode (``private_browsing.exe``). Its sole
purpose for existing is to allow Private Browsing shortcuts to have their own
Visual Elements. This is most notably seen when pinning a Private Browsing
shortcut to the Start Menu -- Visual Elements are used for the icon there
rather than the shortcut's icon.

In addition to always passing ``-private-window``, ``private_browsing.exe``
will forward any other command line arguments given to it to ``firefox.exe``.
It will also forward shortcut information from the Windows ``STARTUPINFOW``
structure to ensure that Firefox knows how it was started.
