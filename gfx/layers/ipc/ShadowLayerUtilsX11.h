/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifndef mozilla_layers_ShadowLayerUtilsX11_h
#define mozilla_layers_ShadowLayerUtilsX11_h

#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>

#include "IPC/IPCMessageUtils.h"

#define MOZ_HAVE_SURFACEDESCRIPTORX11
#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

class gfxXlibSurface;

namespace mozilla {
namespace layers {

struct SurfaceDescriptorX11 {
  SurfaceDescriptorX11()
  { }

  SurfaceDescriptorX11(gfxXlibSurface* aSurf);

  SurfaceDescriptorX11(Drawable aDrawable, XID aFormatID,
                       const gfxIntSize& aSize);

  // Default copy ctor and operator= are OK

  bool operator==(const SurfaceDescriptorX11& aOther) const {
    // Define == as two descriptors having the same XID for now,
    // ignoring size and render format.  If the two indeed refer to
    // the same valid XID, then size/format are "actually" the same
    // anyway, regardless of the values of the fields in
    // SurfaceDescriptorX11.
    return mId == aOther.mId;
  }

  already_AddRefed<gfxXlibSurface> OpenForeign() const;

  Drawable mId;
  XID mFormat; // either a PictFormat or VisualID
  gfxIntSize mSize;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mId);
    WriteParam(aMsg, aParam.mSize);
    WriteParam(aMsg, aParam.mFormat);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mId) &&
            ReadParam(aMsg, aIter, &aResult->mSize) &&
            ReadParam(aMsg, aIter, &aResult->mFormat));
  }
};

} // namespace IPC

#endif  // mozilla_layers_ShadowLayerUtilsX11_h
