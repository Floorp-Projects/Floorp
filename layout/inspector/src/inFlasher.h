/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inFlasher_h__
#define __inFlasher_h__

#include "inIFlasher.h"
#include "nsCoord.h"
#include "nsColor.h"

class nsRenderingContext;

#define BOUND_INNER 0
#define BOUND_OUTER 1

#define DIR_VERTICAL 0
#define DIR_HORIZONTAL 1

class inFlasher : public inIFlasher
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIFLASHER

  inFlasher();
  virtual ~inFlasher();

protected:
  void DrawOutline(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                   nsRenderingContext* aRenderContext,
                   bool aDrawBegin, bool aDrawEnd);
  void DrawLine(nscoord aX, nscoord aY, nscoord aLength,
                bool aDir, bool aBounds,
                nsRenderingContext* aRenderContext);

  nscolor mColor;

  uint16_t mThickness;
  bool mInvert;
};

// {9286E71A-621A-4b91-851E-9984C1A2E81A}
#define IN_FLASHER_CID \
{ 0x9286e71a, 0x621a, 0x4b91, { 0x85, 0x1e, 0x99, 0x84, 0xc1, 0xa2, 0xe8, 0x1a } }

#endif // __inFlasher_h__
