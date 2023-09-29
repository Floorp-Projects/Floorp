Filing good bugs
================

Getting started working on a bug can be hard, specially if you lack
context.

This guide is meant to provide a list of steps to provide the necessary
information to open an actionable bug.

-  **Use a descriptive title**. Avoid jargon and abbreviations where
   possible, they make it hard for other people to find existing bugs,
   and to understand them.
-  **Explain the problem in depth** and provide the steps to reproduce. Be
   as specific as possible, and include things such as operating system
   and version if reporting a bug.
-  If you can, **list files and lines of code** that may need to be
   modified. Ideally provide a patch for getting started.
   Do not hesitate to add permalinks to `Searchfox <https://searchfox.org/mozilla-central/source/>`_
   pointing to the file and lines.
-  If applicable, **provide a test case** or document that can be used to
   test the bug is solved. For example, if the bug title was “HTML
   inspector fails when inspecting a page with one million of nodes”,
   you would provide an HTML document with one million of nodes, and we
   could use it to test the implementation, and make sure you’re looking
   at the same thing we’re looking at. You could use services like
   jsfiddle, codepen or jsbin to share your test cases. Other people use
   GitHub, or their own web server.

Good first bugs
---------------

If you're looking to open a bug as a "good first bug" for new contributors, please adhere to the following guidelines:

1. **Complexity**
   Ensure the bug is not overly complex. New contributors are already navigating the learning curve of Firefox's codebase and workflows. An overly complicated bug can be overwhelming.

2. **Tagging**
   Use the Bugzilla keyword ``good-first-bug`` to mark it appropriately.

3. **Language Specification**
   In the whiteboard section, specify the primary language of the bug using the ``[lang=XX]`` format. For instance, ``[lang=C++]`` for C++ or ``[lang=py]`` for Python.

4. **Mentorship**
   Commit to guiding the new contributor by adding yourself as a mentor for the bug. Please keep the mentor field up to date, if you no longer have time set it to empty or find another available mentor.


5. **Documentation**
   Provide links to essential documentation that can assist the contributor. For example, you might include:

   - :ref:`How To Contribute Code To Firefox`
   - :ref:`Firefox Contributors' Quick Reference`
   - :ref:`Working with stack of patches Quick Reference`

By following these guidelines, you'll be setting up new contributors for success and fostering a welcoming environment for them.

These good first bugs can be browsed on https://codetribute.mozilla.org/
