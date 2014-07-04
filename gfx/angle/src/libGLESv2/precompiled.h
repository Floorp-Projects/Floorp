//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// precompiled.h: Precompiled header file for libGLESv2.

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2.h>

#include <GLES2/gl2ext.h>

#include <EGL/egl.h>

#include <assert.h>
#include <cstddef>
#include <float.h>
#include <stdint.h>
#include <intrin.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm> // for std::min and std::max
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(ANGLE_ENABLE_D3D9)
#include <d3d9.h>
#include <d3dcompiler.h>
#endif // ANGLE_ENABLE_D3D9

#if defined(ANGLE_ENABLE_D3D11)
#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#endif // ANGLE_ENABLE_D3D11
