PRUnichar
=========

An unsigned 16-bit type, like ``char`` in Java or the "characters" of a
JavaScript string defined in
`/mozilla/xpcom/base/nscore.h <http://hg.mozilla.org/mozilla-central/file/d35b4d003e9e/xpcom/base/nscore.h>`__.


Syntax
------

.. code::

   #if defined(NS_WIN32)
     typedef wchar_t PRUnichar;
   #else
     typedef PRUInt16 PRUnichar;
   #endif
