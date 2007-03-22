/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Brian Stell <bstell@netscape.com>
 *   Louie Zhao  <louie.zhao@sun.com>
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
#ifndef nsFontDebug_h__
#define nsFontDebug_h__

#define NS_FONT_DEBUG_LOAD_FONT         0x01
#define NS_FONT_DEBUG_CALL_TRACE        0x02
#define NS_FONT_DEBUG_FIND_FONT         0x04
#define NS_FONT_DEBUG_SIZE_FONT         0x08
#define NS_FONT_DEBUG_SCALED_FONT       0x10
#define NS_FONT_DEBUG_BANNED_FONT       0x20
#define NS_FONT_DEBUG_FONT_CATALOG      0x100
#define NS_FONT_DEBUG_FONT_SCAN         0x200
#define NS_FONT_DEBUG_FREETYPE_FONT     0x400
#define NS_FONT_DEBUG_FREETYPE_GRAPHICS 0x800

#undef NS_FONT_DEBUG
#define NS_FONT_DEBUG 1
#ifdef NS_FONT_DEBUG

# define DEBUG_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, 0xFFFF)

# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
              if (gFontDebug & (type)) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO
#else
# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
            PR_END_MACRO
#endif

#define FIND_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FIND_FONT)

#define SIZE_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_SIZE_FONT)

#define SCALED_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_SCALED_FONT)

#define BANNED_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_BANNED_FONT)

#define FONT_CATALOG_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FONT_CATALOG)

#define FONT_SCAN_PRINTF(x) \
            PR_BEGIN_MACRO \
              if (gFontDebug & NS_FONT_DEBUG_FONT_SCAN) { \
                printf x ; \
                fflush(stdout); \
              } \
            PR_END_MACRO

#define FREETYPE_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FREETYPE_FONT)

#ifdef MOZ_ENABLE_FREETYPE2
extern PRUint32 gFontDebug;
#endif

#endif

