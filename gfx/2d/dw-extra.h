/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * New DirectWrite interfaces based on Win10 Fall Creators Update versions
 * of dwrite_3.h and dcommon.h (from SDK 10.0.17061.0). This particular
 * subset of declarations is intended to be just sufficient to compile the
 * Gecko DirectWrite font code; it omits many other new interfaces, etc.
 */

#ifndef DWRITE_EXTRA_H
#define DWRITE_EXTRA_H

#pragma once

interface IDWriteFontResource;
interface IDWriteFontFaceReference1;

enum DWRITE_GLYPH_IMAGE_FORMATS {
    DWRITE_GLYPH_IMAGE_FORMATS_NONE                   = 0x00000000,
    DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE               = 0x00000001,
    DWRITE_GLYPH_IMAGE_FORMATS_CFF                    = 0x00000002,
    DWRITE_GLYPH_IMAGE_FORMATS_COLR                   = 0x00000004,
    DWRITE_GLYPH_IMAGE_FORMATS_SVG                    = 0x00000008,
    DWRITE_GLYPH_IMAGE_FORMATS_PNG                    = 0x00000010,
    DWRITE_GLYPH_IMAGE_FORMATS_JPEG                   = 0x00000020,
    DWRITE_GLYPH_IMAGE_FORMATS_TIFF                   = 0x00000040,
    DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8 = 0x00000080,
};

#ifdef DEFINE_ENUM_FLAG_OPERATORS
DEFINE_ENUM_FLAG_OPERATORS(DWRITE_GLYPH_IMAGE_FORMATS);
#endif

#define DWRITE_MAKE_FONT_AXIS_TAG(a,b,c,d) \
    (static_cast<DWRITE_FONT_AXIS_TAG>(DWRITE_MAKE_OPENTYPE_TAG(a,b,c,d)))

enum DWRITE_FONT_AXIS_TAG : UINT32 {
    DWRITE_FONT_AXIS_TAG_WEIGHT       = DWRITE_MAKE_FONT_AXIS_TAG('w', 'g', 'h', 't'),
    DWRITE_FONT_AXIS_TAG_WIDTH        = DWRITE_MAKE_FONT_AXIS_TAG('w', 'd', 't', 'h'),
    DWRITE_FONT_AXIS_TAG_SLANT        = DWRITE_MAKE_FONT_AXIS_TAG('s', 'l', 'n', 't'),
    DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE = DWRITE_MAKE_FONT_AXIS_TAG('o', 'p', 's', 'z'),
    DWRITE_FONT_AXIS_TAG_ITALIC       = DWRITE_MAKE_FONT_AXIS_TAG('i', 't', 'a', 'l'),
};

enum DWRITE_FONT_AXIS_ATTRIBUTES {
    DWRITE_FONT_AXIS_ATTRIBUTES_NONE     = 0x0000,
    DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE = 0x0001,
    DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN   = 0x0002,
};

struct DWRITE_FONT_AXIS_VALUE {
    DWRITE_FONT_AXIS_TAG axisTag;
    FLOAT value;
};

struct DWRITE_FONT_AXIS_RANGE {
    DWRITE_FONT_AXIS_TAG axisTag;
    FLOAT minValue;
    FLOAT maxValue;
};

struct DWRITE_GLYPH_IMAGE_DATA;

interface DWRITE_DECLARE_INTERFACE("27F2A904-4EB8-441D-9678-0563F53E3E2F") IDWriteFontFace4
    : public IDWriteFontFace3
{
    STDMETHOD_(DWRITE_GLYPH_IMAGE_FORMATS, GetGlyphImageFormats)() PURE;
    STDMETHOD(GetGlyphImageFormats)(
        UINT16 glyphId,
        UINT32 pixelsPerEmFirst,
        UINT32 pixelsPerEmLast,
        _Out_ DWRITE_GLYPH_IMAGE_FORMATS* glyphImageFormats
        ) PURE;
    STDMETHOD(GetGlyphImageData)(
        _In_ UINT16 glyphId,
        UINT32 pixelsPerEm,
        DWRITE_GLYPH_IMAGE_FORMATS glyphImageFormat,
        _Out_ DWRITE_GLYPH_IMAGE_DATA* glyphData,
        _Outptr_result_maybenull_ void** glyphDataContext
        ) PURE;
    STDMETHOD_(void, ReleaseGlyphImageData)(
        void* glyphDataContext
        ) PURE;
};

interface DWRITE_DECLARE_INTERFACE("98EFF3A5-B667-479A-B145-E2FA5B9FDC29") IDWriteFontFace5
    : public IDWriteFontFace4
{
    STDMETHOD_(UINT32, GetFontAxisValueCount)() PURE;
    STDMETHOD(GetFontAxisValues)(
        _Out_writes_(fontAxisValueCount) DWRITE_FONT_AXIS_VALUE* fontAxisValues,
        UINT32 fontAxisValueCount
        ) PURE;
    STDMETHOD_(BOOL, HasVariations)() PURE;
    STDMETHOD(GetFontResource)(
        _COM_Outptr_ IDWriteFontResource** fontResource
        ) PURE;
    STDMETHOD_(BOOL, Equals)(IDWriteFontFace* fontFace) PURE;
};

interface DWRITE_DECLARE_INTERFACE("1F803A76-6871-48E8-987F-B975551C50F2") IDWriteFontResource
    : public IUnknown
{
    STDMETHOD(GetFontFile)(
        _COM_Outptr_ IDWriteFontFile** fontFile
        ) PURE;
    STDMETHOD_(UINT32, GetFontFaceIndex)() PURE;
    STDMETHOD_(UINT32, GetFontAxisCount)() PURE;
    STDMETHOD(GetDefaultFontAxisValues)(
        _Out_writes_(fontAxisValueCount) DWRITE_FONT_AXIS_VALUE* fontAxisValues,
        UINT32 fontAxisValueCount
        ) PURE;
    STDMETHOD(GetFontAxisRanges)(
        _Out_writes_(fontAxisRangeCount) DWRITE_FONT_AXIS_RANGE* fontAxisRanges,
        UINT32 fontAxisRangeCount
        ) PURE;
    STDMETHOD_(DWRITE_FONT_AXIS_ATTRIBUTES, GetFontAxisAttributes)(
        UINT32 axisIndex
        ) PURE;
    STDMETHOD(GetAxisNames)(
        UINT32 axisIndex,
        _COM_Outptr_ IDWriteLocalizedStrings** names
        ) PURE;
    STDMETHOD_(UINT32, GetAxisValueNameCount)(
        UINT32 axisIndex
        ) PURE;
    STDMETHOD(GetAxisValueNames)(
        UINT32 axisIndex,
        UINT32 axisValueIndex,
        _Out_ DWRITE_FONT_AXIS_RANGE* fontAxisRange,
        _COM_Outptr_ IDWriteLocalizedStrings** names
        ) PURE;
    STDMETHOD_(BOOL, HasVariations)() PURE;
    STDMETHOD(CreateFontFace)(
        DWRITE_FONT_SIMULATIONS fontSimulations,
        _In_reads_(fontAxisValueCount) DWRITE_FONT_AXIS_VALUE const* fontAxisValues,
        UINT32 fontAxisValueCount,
        _COM_Outptr_ IDWriteFontFace5** fontFace
        ) PURE;
    STDMETHOD(CreateFontFaceReference)(
        DWRITE_FONT_SIMULATIONS fontSimulations,
        _In_reads_(fontAxisValueCount) DWRITE_FONT_AXIS_VALUE const* fontAxisValues,
        UINT32 fontAxisValueCount,
        _COM_Outptr_ IDWriteFontFaceReference1** fontFaceReference
        ) PURE;
};

#endif /* DWRITE_EXTRA_H */
