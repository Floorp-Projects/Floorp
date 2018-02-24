//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Compile-time instances of many common TType values. These are looked up
// (statically or dynamically) through the methods defined in the namespace.
//

#include "compiler/translator/StaticType.h"

namespace sh
{

namespace StaticType
{

const TType *GetForFloatImage(TBasicType basicType)
{
    switch (basicType)
    {
        case EbtGImage2D:
            return Get<EbtImage2D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage3D:
            return Get<EbtImage3D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage2DArray:
            return Get<EbtImage2DArray, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImageCube:
            return Get<EbtImageCube, EbpUndefined, EvqGlobal, 1, 1>();
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

const TType *GetForIntImage(TBasicType basicType)
{
    switch (basicType)
    {
        case EbtGImage2D:
            return Get<EbtIImage2D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage3D:
            return Get<EbtIImage3D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage2DArray:
            return Get<EbtIImage2DArray, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImageCube:
            return Get<EbtIImageCube, EbpUndefined, EvqGlobal, 1, 1>();
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

const TType *GetForUintImage(TBasicType basicType)
{
    switch (basicType)
    {
        case EbtGImage2D:
            return Get<EbtUImage2D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage3D:
            return Get<EbtUImage3D, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImage2DArray:
            return Get<EbtUImage2DArray, EbpUndefined, EvqGlobal, 1, 1>();
        case EbtGImageCube:
            return Get<EbtUImageCube, EbpUndefined, EvqGlobal, 1, 1>();
        default:
            UNREACHABLE();
            return GetBasic<EbtVoid>();
    }
}

}  // namespace StaticType

}  // namespace sh
