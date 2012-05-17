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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2012
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

#ifndef MOZILLA_SVGFRAGMENTIDENTIFIER_H__
#define MOZILLA_SVGFRAGMENTIDENTIFIER_H__

#include "nsString.h"

class nsIDocument;
class nsSVGSVGElement;
class nsSVGViewElement;

namespace mozilla {

/**
 * Implements support for parsing SVG fragment identifiers
 * http://www.w3.org/TR/SVG/linking.html#SVGFragmentIdentifiers
 */
class SVGFragmentIdentifier
{
  // To prevent the class being instantiated
  SVGFragmentIdentifier() MOZ_DELETE;

public:
  /**
   * Process the SVG fragment identifier, if there is one.
   * @return true if we found something we recognised
   */
  static bool ProcessFragmentIdentifier(nsIDocument *aDocument,
                                        const nsAString &aAnchorName);

private:
 /**
  * Parse an SVG ViewSpec and set applicable attributes on the root element.
  * @return true if there is a valid ViewSpec
  */
  static bool ProcessSVGViewSpec(const nsAString &aViewSpec, nsSVGSVGElement *root);

  // Save and restore things we override in case we want to go back e.g. the
  // user presses the back button
  static void SaveOldPreserveAspectRatio(nsSVGSVGElement *root);
  static void RestoreOldPreserveAspectRatio(nsSVGSVGElement *root);
  static void SaveOldViewBox(nsSVGSVGElement *root);
  static void RestoreOldViewBox(nsSVGSVGElement *root);
  static void SaveOldZoomAndPan(nsSVGSVGElement *root);
  static void RestoreOldZoomAndPan(nsSVGSVGElement *root);
};

} // namespace mozilla

#endif // MOZILLA_SVGFRAGMENTIDENTIFIER_H__
