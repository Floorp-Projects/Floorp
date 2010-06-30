/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* font constants shared by both thebes and layout */

#ifndef GFX_FONT_CONSTANTS_H
#define GFX_FONT_CONSTANTS_H

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

#define NS_FONT_STYLE_NORMAL            0
#define NS_FONT_STYLE_ITALIC            1
#define NS_FONT_STYLE_OBLIQUE           2

#define NS_FONT_WEIGHT_NORMAL           400
#define NS_FONT_WEIGHT_BOLD             700
#define NS_FONT_WEIGHT_BOLDER           1
#define NS_FONT_WEIGHT_LIGHTER          (-1)

#define NS_FONT_STRETCH_ULTRA_CONDENSED (-4)
#define NS_FONT_STRETCH_EXTRA_CONDENSED (-3)
#define NS_FONT_STRETCH_CONDENSED       (-2)
#define NS_FONT_STRETCH_SEMI_CONDENSED  (-1)
#define NS_FONT_STRETCH_NORMAL          0
#define NS_FONT_STRETCH_SEMI_EXPANDED   1
#define NS_FONT_STRETCH_EXPANDED        2
#define NS_FONT_STRETCH_EXTRA_EXPANDED  3
#define NS_FONT_STRETCH_ULTRA_EXPANDED  4
// These need to be more than double all of the values above so that we
// can add any multiple of any of these values to the values above.
#define NS_FONT_STRETCH_WIDER           10
#define NS_FONT_STRETCH_NARROWER        (-10)

#endif
