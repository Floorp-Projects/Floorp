/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Brian Stell <bstell@netscape.com>
 *   Louie Zhao  <louie.zhao@sun.com>
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
#ifndef NS_FT2_FONT_NODE_H
#define NS_FT2_FONT_NODE_H

#include "nsFontMetricsGTK.h"

class nsFT2FontNode {
public:
  static void       FreeGlobals();
  static nsresult   InitGlobals();
  static void       GetFontNames(const char* aPattern, nsFontNodeArray* aNodes);

#if (defined(MOZ_ENABLE_FREETYPE2))
protected:
  static PRBool ParseXLFD(char *, char**, char**, char**, char**);
  static nsFontNode* LoadNode(nsITrueTypeFontCatalogEntry*,
                              const char*,
                              nsFontNodeArray*);
  static PRBool      LoadNodeTable();
  static nsHashtable *mFreeTypeNodes;
  static PRBool      sInited;
  static nsIFontCatalogService* sFcs;
#endif //MOZ_ENABLE_FREETYPE2

};

#endif

