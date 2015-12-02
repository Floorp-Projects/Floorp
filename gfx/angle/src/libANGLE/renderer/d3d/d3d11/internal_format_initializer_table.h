//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// internal_format_initializer_table:
//   Contains table to go from internal format and dxgi format to initializer function
//   for TextureFormat
//

#ifndef LIBANGLE_RENDERER_D3D_D3D11_INTERNALFORMATINITIALIZERTABLE_H_
#define LIBANGLE_RENDERER_D3D_D3D11_INTERNALFORMATINITIALIZERTABLE_H_

#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"

#include <map>

namespace rx
{

namespace d3d11
{

InitializeTextureDataFunction GetInternalFormatInitializer(GLenum internalFormat,
                                                           DXGI_FORMAT dxgiFormat);

}  // namespace d3d11

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_INTERNALFORMATINITIALIZERTABLE_H_
