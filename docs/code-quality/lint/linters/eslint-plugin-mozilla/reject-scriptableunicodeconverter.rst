reject-scriptableunicodeconverter
================================================

Rejects calls into ``Ci.nsIScriptableUnicodeConverter``. This is configured as a warning.
You should use |TextEncoder|_ or |TextDecoder|_  for new code.
If modifying old code, please consider swapping it in if possible; if this is tricky please ensure
a bug is on file.

.. |TextEncoder| replace:: ``TextEncoder``
.. _TextEncoder: https://searchfox.org/mozilla-central/source/dom/webidl/TextEncoder.webidl

.. |TextDecoder| replace:: ``TextDecoder``
.. _TextDecoder: https://searchfox.org/mozilla-central/source/dom/webidl/TextDecoder.webidl
