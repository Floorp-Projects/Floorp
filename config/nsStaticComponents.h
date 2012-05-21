/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticComponents_h__
#define nsStaticComponents_h__

#include "mozilla/Module.h"

// These symbols are provided by nsStaticComponents.cpp, and also by other
// static component providers such as nsStaticXULComponents (libxul).

extern mozilla::Module const *const *const kPStaticModules[];

#endif
