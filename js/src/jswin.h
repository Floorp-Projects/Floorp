/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is a wrapper around <windows.h> to prevent the mangling of
 * various function names throughout the codebase.
 */
#ifdef XP_WIN
# include <windows.h>
# undef GetProp
# undef SetProp
# undef CONST
# undef STRICT
# undef LEGACY
# undef THIS
#endif
