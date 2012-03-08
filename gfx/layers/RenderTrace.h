/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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

// This is a general tool that will let you visualize platform operation.
// Currently used for the layer system, the general syntax allows this
// tools to be adapted to trace other operations.
//
// For the front end see: https://github.com/staktrace/rendertrace

// Uncomment this line to enable RENDERTRACE
//#define MOZ_RENDERTRACE
#ifdef MOZ_RENDERTRACE

#include "gfx3DMatrix.h"
#include "nsRect.h"

#ifndef GFX_RENDERTRACE_H
#define GFX_RENDERTRACE_H

namespace mozilla {
namespace layers {

class Layer;

void RenderTraceLayers(Layer *aLayer, const char *aColor, gfx3DMatrix aRootTransform = gfx3DMatrix(), bool aReset = true);

void RenderTraceInvalidateStart(Layer *aLayer, const char *aColor, const nsIntRect aRect);
void RenderTraceInvalidateEnd(Layer *aLayer, const char *aColor);

}
}

#endif //GFX_RENDERTRACE_H

#endif // MOZ_RENDERTRACE
