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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef __NS_ISVGCAIRO_REGION_H__
#define __NS_ISVGCAIRO_REGION_H__

#include "nsISVGRendererRegion.h"

#define NS_ISVGCAIROREGION_IID \
{ 0x92a3f5de, 0x694c, 0x4af1, { 0x81, 0x95, 0x23, 0xc7, 0xdb, 0x68, 0xc5, 0x17 } }

/**
 * \addtogroup cairo_renderer Cairo Rendering Engine
 * @{
 */

/**
 * 'Private' rendering engine interface
 */
class nsISVGCairoRegion : public nsISVGRendererRegion
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISVGCAIROREGION_IID)

  NS_IMETHOD_(PRBool) Contains(float x, float y) = 0;
};

/** @} */

#endif //__NS_ISVGCAIRO_REGION_H__
