Processes, Threads and IPC
==========================

These pages contain the documentation for Gecko's architecture for platform
process and thread creation, communication and synchronization.  They live
in mozilla-central in the 'ipc/docs' directory.

.. toctree::
    :maxdepth: 3

    ipdl
    processes
    utility_process

For inter-process communication involving Javascript, see `JSActors`_.  They
are a very limited case, used for communication between elements in the DOM,
which may exist in separate processes.  They only involve the main process and
content processes -- no other processes run Javascript.

.. _JSActors: /dom/ipc/jsactors.html
