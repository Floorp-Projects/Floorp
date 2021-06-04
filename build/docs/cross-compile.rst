=================
Cross-compilation
=================

If you are planning to perform cross-compilation e.g. for Linux/Aarch64, you
will probably want to use experimental feature ``--enable-bootstrap`` in your
``.mozconfig``. Then, you just have to specify the the target arch, after which
the build system will automatically set up the sysroot.

For example, cross-compiling for Linux/Aarch64:

.. code-block:: text

    ac_add_options --target=aarch64-linux-gnu
    ac_add_options --enable-bootstrap
