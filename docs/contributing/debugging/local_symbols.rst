Symbolicating TreeHerder stacks locally
=======================================

When using tools like the :ref:`Dark Matter Detector (DMD)` or
:ref:`refcount logging<Refcount Tracing and Balancing>` to
investigate issues occurring on TreeHerder that you can't reproduce locally, you
can often end up with unsymbolicated stacks. Fortunately, there is a way to
symbolicate these stacks on your own machine.

These instructions are for a Linux TreeHerder build for MacOS, so they might
require some modifications for other combinations of platforms.

Download ``target.tar.bz2`` and ``target.crashreporter-symbols.zip`` from the
Build job. **Note that these files are very large so you'll want to delete
them and the extracted files when you are done.**

These files each contain a large number of files, so I'd recommend creating
a directory for each of them. Call these ``<TARGET_DIR>`` and ``<SYMB_DIR>``,
and move the prior two files into these two directories, respectively.

Go to ``<TARGET_DIR>`` and run something like

.. code-block:: shell

    tar xf target.tar.bz2

then go to ``<SYMB_DIR>`` and run something like

.. code-block:: shell

    unzip target.crashreporter-symbols.zip

You should be able to delete the two original files now.

Next we need to ensure that the locations of binaries are rewritten from
where they are on TreeHerder to where we have them locally. We'll do this by
editing ``fix_stacks.py``. This file is located in the ``tools/rb/`` directory of
the Firefox source directory. You need to add these two lines to the function
``fixSymbols``, after ``line_str`` is defined and before it is written to
``fix_stacks.stdin``. I've done this right before the definition of
``is_missing_newline``.

.. code-block:: python

    line_str = line_str.replace("/builds/worker/workspace/build/application/firefox/firefox",
                                "<TARGET_DIR>/firefox/firefox-bin")
    line_str = line_str.replace("/builds/worker/workspace/build/application/firefox/libxul.so",
                                "<TARGET_DIR>/firefox/libxul.so")

The initial locations should appear verbatim in the stack you are trying to
symbolicate, so double check that they match. Also, ``<TARGET_DIR>`` of course
needs to be replaced with the actual local directories where those files are
located. Note that the ``firefox`` executable is changed to ``firefox-bin``.
I don't know why that is necessary, but only the latter existed for me.

Finally, we need to make it so that the stack fixer can find the location of
the breakpad symbols we downloaded. If you are running ``fix_stacks.py`` via
``dmd.py`` or directly (in a recent version), you can do this by running with the
environment variable ``BREAKPAD_SYMBOLS_PATH`` set to the ``<SYMB_DIR>`` from above.
If that doesn't work, you'll have to edit ``initFixStacks`` in ``fix_stacks.py`` to
set ``breakpadSymsDir`` to ``<SYMB_DIR>``.

With all of that done, you should now be able to run ``dmd.py`` or ``fix_stacks.py``
to fix the stacks. Note that the stack fixing process can take a minute or two.
