/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "plstr.h"
#include "nsColor.h"
#include "nsColorNames.h"

static int ComponentValue(const char* aColorSpec, int dpc)
{
  int component = 0;
  while (--dpc >= 0) {
    char ch = *aColorSpec++;
    if ((ch >= '0') && (ch <= '9')) {
      component = component*16 + (ch - '0');
    } else {
      // "ch&7" handles lower and uppercase hex alphabetics
      component = component*16 + (ch & 7) + 9;
    }
  }
  return component;
}

// Note: This handles 9 digits of hex to be compatible with eric
// bina's original code. However, it is pickyer with respect to what a
// legal color is and will only return true for perfectly legal color
// values.
NS_GFX PRBool NS_HexToRGB(const char* aColorSpec, nscolor* aResult)
{
  NS_PRECONDITION(nsnull != aColorSpec, "null ptr");
  if (nsnull == aColorSpec) {
    return PR_FALSE;
  }

  if (aColorSpec[0] == '#') {
    aColorSpec++;
  }

  int nameLen = PL_strlen(aColorSpec);
  if ((nameLen == 3) || (nameLen == 6) || (nameLen == 9)) {
    // Make sure the digits are legal
    for (int i = 0; i < nameLen; i++) {
      char ch = aColorSpec[i];
      if (((ch >= '0') && (ch <= '9')) ||
          ((ch >= 'a') && (ch <= 'f')) ||
          ((ch >= 'A') && (ch <= 'F'))) {
        // Legal character
        continue;
      }
      // Whoops. Illegal character.
      return PR_FALSE;
    }

    // Convert the ascii to binary
    int dpc = nameLen / 3;

    // Translate components from hex to binary
    int r = ComponentValue(aColorSpec, dpc);
    int g = ComponentValue(aColorSpec + dpc, dpc);
    int b = ComponentValue(aColorSpec + dpc*2, dpc);
    if (dpc == 1) {
      // Scale single digit component to an 8 bit value. Replicate the
      // single digit to compute the new value.
      r = (r << 4) | r;
      g = (g << 4) | g;
      b = (b << 4) | b;
    } else if (dpc == 3) {
      // Drop off the low digit from 12 bit values.
      r = r >> 4;
      g = g >> 4;
      b = b >> 4;
    }
    NS_ASSERTION((r >= 0) && (r <= 255), "bad r");
    NS_ASSERTION((g >= 0) && (g <= 255), "bad g");
    NS_ASSERTION((b >= 0) && (b <= 255), "bad b");
    if (nsnull != aResult) {
      *aResult = NS_RGB(r, g, b);
    }
    return PR_TRUE;
  }

  // Improperly formatted color value
  return PR_FALSE;
}

PRBool NS_ColorNameToRGB(const char* aColorName, nscolor* aResult)
{
  PRInt32 id = nsColorNames::LookupName(aColorName);
  if (id >= 0) {
    NS_ASSERTION(id < COLOR_MAX, "LookupName mess up");
    if (nsnull != aResult) {
      *aResult = nsColorNames::kColors[id];
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}
