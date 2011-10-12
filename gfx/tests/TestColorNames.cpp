/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestHarness.h"

#include <string.h>
#include "nsColor.h"
#include "nsColorNames.h"
#include "prprf.h"
#include "nsString.h"

// define an array of all color names
#define GFX_COLOR(_name, _value) #_name,
static const char* const kColorNames[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

// define an array of all color name values
#define GFX_COLOR(_name, _value) _value,
static const nscolor kColors[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

static const char* kJunkNames[] = {
  nsnull,
  "",
  "123",
  "backgroundz",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestColorNames");
  if (xpcom.failed())
    return 1;

  nscolor rgb;
  int rv = 0;

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work

  for (PRUint32 index = 0 ; index < NS_ARRAY_LENGTH(kColorNames); index++) {
    // Lookup color by name and make sure it has the right id
    nsCString tagName(kColorNames[index]);

    // Check that color lookup by name gets the right rgb value
    if (!NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) {
      fail("can't find '%s'", tagName.get());
      rv = 1;
    }
    if (rgb != kColors[index]) {
      fail("name='%s' ColorNameToRGB=%x kColors[%d]=%08x",
           tagName.get(), rgb, index, kColors[index]);
      rv = 1;
    }

    // fiddle with the case to make sure we can still find it
    tagName.SetCharAt(tagName.CharAt(0) - 32, 0);
    if (!NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) {
      fail("can't find '%s'", tagName.get());
      rv = 1;
    }
    if (rgb != kColors[index]) {
      fail("name='%s' ColorNameToRGB=%x kColors[%d]=%08x",
           tagName.get(), rgb, index, kColors[index]);
      rv = 1;
    }

    // Check that parsing an RGB value in hex gets the right values
    PRUint8 r = NS_GET_R(rgb);
    PRUint8 g = NS_GET_G(rgb);
    PRUint8 b = NS_GET_B(rgb);
    PRUint8 a = NS_GET_A(rgb);
    if (a != PR_UINT8_MAX) {
      // NS_HexToRGB() can not handle a color with alpha channel
      rgb = NS_RGB(r, g, b);
    }
    char cbuf[50];
    PR_snprintf(cbuf, sizeof(cbuf), "%02x%02x%02x", r, g, b);
    nscolor hexrgb;
    if (!NS_HexToRGB(NS_ConvertASCIItoUTF16(cbuf), &hexrgb)) {
      fail("hex conversion to color of '%s'", cbuf);
      rv = 1;
    }
    if (hexrgb != rgb) {
      fail("rgb=%x hexrgb=%x", rgb, hexrgb);
      rv = 1;
    }
  }

  // Now make sure we don't find some garbage
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(kJunkNames); i++) {
    nsCString tag(kJunkNames[i]);
    if (NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tag), &rgb)) {
      fail("found '%s'", kJunkNames[i] ? kJunkNames[i] : "(null)");
      rv = 1;
    }
  }

  if (rv == 0)
    passed("TestColorNames");
  return rv;
}
