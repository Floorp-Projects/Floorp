/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The header <X11/X.h> defines "None" as a macro that expands to "0L".
// This is terrible because many enumerations have an enumerator named "None".
// To work around this, we undefine the macro "None", and define a replacement
// macro named "X11None".
// Include this header after including X11 headers, where necessary.
#ifdef None
#  undef None
#  define X11None 0L
// <X11/X.h> also defines "RevertToNone" as a macro that expands to "(int)None".
// Since we are undefining "None", that stops working. To keep it working,
// we undefine "RevertToNone" and redefine it in terms of "X11None".
#  ifdef RevertToNone
#    undef RevertToNone
#    define RevertToNone (int)X11None
#  endif
#endif

// X11 also defines Always, which conflicts with some style system enum variant
// names, so get rid of that too, given we don't use it anywhere else.
#ifdef Always
#  undef Always
#endif

// And Complex...
#ifdef Complex
#  undef Complex
#endif

// X11/Xlib.h also defines True and False, get rid of those too for
// the same reasons as above...
#ifdef True
#  undef True
#  define X11True 1
#endif
#ifdef False
#  undef False
#  define X11False 0
#endif
