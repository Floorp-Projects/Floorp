/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab:
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
 * The Original Code is Golden Hills Computer Services code.
 *
 * The Initial Developer of the Original Code is
 * Brian Stell <bstell@ix.netcom.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brian Stell <bstell@ix.netcom.com>.
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


#ifndef nsFontPSDebug_h__
#define nsFontPSDebug_h__

#define NS_FONTPS_DEBUG_FIND_FONT         0x01
#define NS_FONTPS_DEBUG_ADD_ENTRY         0x02

#undef NS_FONTPS_DEBUG
#define NS_FONTPS_DEBUG 1
#ifdef NS_FONTPS_DEBUG

# define DEBUG_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, 0xFFFF)

# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
              if (gFontPSDebug & (type)) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO
#else
# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
            PR_END_MACRO
#endif

#define FIND_FONTPS_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONTPS_DEBUG_FIND_FONT)

#define ADD_ENTRY_FONTPS_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONTPS_DEBUG_ADD_ENTRY)

extern PRUint32 gFontPSDebug;

#endif

