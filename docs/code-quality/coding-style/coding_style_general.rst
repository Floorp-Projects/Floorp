
Mode line
~~~~~~~~~

Files should have Emacs and vim mode line comments as the first two
lines of the file, which should set ``indent-tabs-mode`` to ``nil``. For new
files, use the following, specifying two-space indentation:

.. code-block:: cpp

   /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=2 et sw=2 tw=80: */
   /* This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

Be sure to use the correct ``Mode`` in the first line, don't use ``C++`` in
JavaScript files.
