/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is the font size inflation manager.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Per-block-formatting-context manager of font size inflation for pan and zoom UI. */

#include "nsFontInflationData.h"
#include "FramePropertyTable.h"

using namespace mozilla;

static void
DestroyFontInflationData(void *aPropertyValue)
{
  delete static_cast<nsFontInflationData*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(FontInflationDataProperty, DestroyFontInflationData);

/* static */ nsFontInflationData*
nsFontInflationData::FindFontInflationDataFor(const nsIFrame *aFrame)
{
  // We have one set of font inflation data per block formatting context.
  const nsIFrame *bfc = FlowRootFor(aFrame);

  return static_cast<nsFontInflationData*>(
             bfc->Properties().Get(FontInflationDataProperty()));
}
