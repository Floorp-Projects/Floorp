/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsString.h"

// Test to see if a feature is implemented
PRBool
NS_SVG_TestFeature(const nsAString& fstr) {
  nsAutoString lstr(fstr);
  lstr.StripWhitespace();

#ifdef DEBUG_scooter
  NS_ConvertUTF16toUTF8 feature(lstr);
  printf("NS_SVG_TestFeature: testing for %s\n", feature.get());
#endif

#define SVG_SUPPORTED_FEATURE(str) if (lstr.Equals(NS_LITERAL_STRING(str).get())) return PR_TRUE;
#define SVG_UNSUPPORTED_FEATURE(str)
#include "nsSVGFeaturesList.h"
#undef SVG_SUPPORTED_FEATURE
#undef SVG_UNSUPPORTED_FEATURE
  return PR_FALSE;
}

// Test to see if a list of features are implemented
PRBool
NS_SVG_TestFeatures(const nsAString& fstr) {
  nsAutoString lstr(fstr);
  // Get an iterator on the string
  PRInt32 vbegin = 0;
  PRInt32 vlen = lstr.Length();
  while (vbegin < vlen) {
    PRInt32 vend = lstr.FindChar(PRUnichar(' '), vbegin);
    if (vend == kNotFound) {
      vend = vlen;
    }
    if (NS_SVG_TestFeature(Substring(lstr, vbegin, vend-vbegin)) == PR_FALSE) {
      return PR_FALSE;
    }
    vbegin = vend+1;
  }
  return PR_TRUE;
}
