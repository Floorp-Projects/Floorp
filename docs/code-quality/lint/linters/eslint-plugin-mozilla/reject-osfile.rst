reject-osfile
=============

Rejects calls into ``OS.File`` and ``OS.Path``. This is configured as a warning.
You should use |IOUtils|_ and |PathUtils|_ respectively for new code.  If
modifying old code, please consider swapping it in if possible; if this is
tricky please ensure a bug is on file.

.. |IOUtils| replace:: ``IOUtils``
.. _IOUtils: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/IOUtils.webidl

.. |PathUtils| replace:: ``PathUtils``
.. _PathUtils: https://searchfox.org/mozilla-central/source/dom/chrome-webidl/PathUtils.webidl
