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
 * Marco Pesenti Gritti <marco@gnome.org>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef gfxCore_h__
#define gfxCore_h__

#include "nscore.h"

// Side constants for use in various places
#define NS_SIDE_TOP     0
#define NS_SIDE_RIGHT   1
#define NS_SIDE_BOTTOM  2
#define NS_SIDE_LEFT    3

#if defined(MOZ_ENABLE_LIBXUL) || !defined(MOZILLA_INTERNAL_API)
#  define NS_GFX
#  define NS_GFX_(type) type
#  define NS_GFX_STATIC_MEMBER_(type) type
#elif defined(_IMPL_NS_GFX)
#  define NS_GFX NS_EXPORT
#  define NS_GFX_(type) NS_EXPORT_(type)
#  define NS_GFX_STATIC_MEMBER_(type) NS_EXPORT_STATIC_MEMBER_(type)
#else
#  define NS_GFX NS_IMPORT
#  define NS_GFX_(type) NS_IMPORT_(type)
#  define NS_GFX_STATIC_MEMBER_(type) NS_IMPORT_STATIC_MEMBER_(type)
#endif

#endif
