reject-osfile
=============================

Rejects calls into ``OS.File``. This is configured as a warning.
You should use |IOUtils|_ for new code.
If modifying old code, please consider swapping it in if possible; if this is tricky please ensure
a bug is on file.

.. |IOUtils| replace:: ``IOUtils``
.. _IOUtils: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/IOUtils.webidl
