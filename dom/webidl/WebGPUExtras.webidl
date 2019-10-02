/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Some parts of WebGPU.webidl need to be pulled into a different file due to a codegen
 * bug/missing support:
 * "TypeError: Dictionary contains a union that contains a dictionary in the same WebIDL file."
 * "This won't compile.  Move the inner dictionary to a different file."
 */

dictionary GPUBufferBinding {
    required GPUBuffer buffer;
    u64 offset = 0;
    u64 size;
};

dictionary GPUColorDict {
    required double r;
    required double g;
    required double b;
    required double a;
};

dictionary GPUOrigin2DDict {
    u32 x = 0;
    u32 y = 0;
};

dictionary GPUOrigin3DDict {
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

dictionary GPUExtent3DDict {
    required u32 width;
    required u32 height;
    required u32 depth;
};
