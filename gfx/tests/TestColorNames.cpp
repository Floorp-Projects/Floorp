/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// No output means test passed.

#include <stdio.h>
#include <string.h>
#include "nsColorNames.h"
#include "prprf.h"
#include "nsString.h"

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
  nsColorName id;
  nsColorName index;
  int rv = 0;

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work
    
  index = eColorName_UNKNOWN;
  while (PRInt32(index) < (PRInt32 (eColorName_COUNT) - 1)) {
    // Lookup color by name and make sure it has the right id
    char tagName[512];
    index = nsColorName(PRInt32(index) + 1);
    nsColorNames::GetStringValue(index).ToCString(tagName, sizeof(tagName));

    id = nsColorNames::LookupName(NS_ConvertASCIItoUCS2(tagName));
    if (id == eColorName_UNKNOWN) {
      printf("bug: can't find '%s'\n", tagName);
      rv = -1;
    }
    if (id != index) {
      printf("bug: name='%s' id=%d index=%d\n", tagName, id, index);
      rv = -1;
    }

    // fiddle with the case to make sure we can still find it
    tagName[0] = tagName[0] - 32;
    id = nsColorNames::LookupName(NS_ConvertASCIItoUCS2(tagName));
    if (id == eColorName_UNKNOWN) {
      printf("bug: can't find '%s'\n", tagName);
      rv = -1;
    }
    if (id != index) {
      printf("bug: name='%s' id=%d index=%d\n", tagName, id, index);
      rv = -1;
    }

    // Check that color lookup by name gets the right rgb value
    nscolor rgb;
    if (!NS_ColorNameToRGB(NS_ConvertASCIItoUCS2(tagName), &rgb)) {
      printf("bug: name='%s' didn't NS_ColorNameToRGB\n", tagName);
      rv = -1;
    }
    if (nsColorNames::kColors[index] != rgb) {
      printf("bug: name='%s' ColorNameToRGB=%x kColors[%d]=%x\n",
             tagName, rgb, nsColorNames::kColors[index],
             nsColorNames::kColors[index]);
      rv = -1;
    }

    // Check that parsing an RGB value in hex gets the right values
    PRUint8 r = NS_GET_R(rgb);
    PRUint8 g = NS_GET_G(rgb);
    PRUint8 b = NS_GET_B(rgb);
    char cbuf[50];
    PR_snprintf(cbuf, sizeof(cbuf), "%02x%02x%02x", r, g, b);
    nscolor hexrgb;
    if (!NS_HexToRGB(NS_ConvertASCIItoUCS2(cbuf), &hexrgb)) {
      printf("bug: hex conversion to color of '%s' failed\n", cbuf);
      rv = -1;
    }
    if (hexrgb != rgb) {
      printf("bug: rgb=%x hexrgb=%x\n", rgb, hexrgb);
      rv = -1;
    }
  }

  // Now make sure we don't find some garbage
  for (int i = 0; i < (int) (sizeof(kJunkNames) / sizeof(const char*)); i++) {
    const char* tag = kJunkNames[i];
    id = nsColorNames::LookupName(NS_ConvertASCIItoUCS2(tag));
    if (id > eColorName_UNKNOWN) {
      printf("bug: found '%s'\n", tag ? tag : "(null)");
      rv = -1;
    }
  }

  return rv;
}
