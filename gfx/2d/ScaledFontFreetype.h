/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTFREETYPE_H_
#define MOZILLA_GFX_SCALEDFONTFREETYPE_H_

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class ScaledFontFreetype : public ScaledFontBase
{
public:

  ScaledFontFreetype(gfxFont* aFont, Float aSize);
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTFREETYPE_H_ */
