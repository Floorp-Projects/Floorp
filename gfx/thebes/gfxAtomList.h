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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

/******

  This file contains the list of all gfx language nsIAtoms and their values
  
  It is designed to be used as inline input to gfxAtoms.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro GFX_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to GFX_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

// language codes specifically referenced in the gfx code
GFX_ATOM(en, "en")
GFX_ATOM(x_unicode, "x-unicode")
GFX_ATOM(x_western, "x-western")

GFX_ATOM(ja, "ja")
GFX_ATOM(ko, "ko")
GFX_ATOM(zh_cn, "zh-cn")
GFX_ATOM(zh_hk, "zh-hk")
GFX_ATOM(zh_tw, "zh-tw")

// additional codes used in nsUnicodeRange.cpp
GFX_ATOM(x_cyrillic, "x-cyrillic")
GFX_ATOM(el, "el")
GFX_ATOM(tr, "tr")
GFX_ATOM(he, "he")
GFX_ATOM(ar, "ar")
GFX_ATOM(x_baltic, "x-baltic")
GFX_ATOM(th, "th")
GFX_ATOM(x_devanagari, "x-devanagari")
GFX_ATOM(x_tamil, "x-tamil")
GFX_ATOM(x_armn, "x-armn")
GFX_ATOM(x_beng, "x-beng")
GFX_ATOM(x_cans, "x-cans")
GFX_ATOM(x_ethi, "x-ethi")
GFX_ATOM(x_geor, "x-geor")
GFX_ATOM(x_gujr, "x-gujr")
GFX_ATOM(x_guru, "x-guru")
GFX_ATOM(x_khmr, "x-khmr")
GFX_ATOM(x_knda, "x-knda")
GFX_ATOM(x_mlym, "x-mlym")
GFX_ATOM(x_orya, "x-orya")
GFX_ATOM(x_sinh, "x-sinh")
GFX_ATOM(x_telu, "x-telu")
GFX_ATOM(x_tibt, "x-tibt")

// used in gfxGDIFontList.h
GFX_ATOM(ko_xxx, "ko-xxx")
GFX_ATOM(x_central_euro, "x-central-euro")
GFX_ATOM(x_symbol, "x-symbol")

// referenced in all.js
GFX_ATOM(x_user_def, "x-user-def")
