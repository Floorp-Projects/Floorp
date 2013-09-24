/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t) (void *data);

/**
 * Currently needs to be 'double' for Cairo compatibility. Could
 * become 'float', perhaps, in some configurations.
 */
typedef double gfxFloat;

/**
 * Priority of a line break opportunity.
 *
 * eNoBreak       The line has no break opportunities
 * eWordWrapBreak The line has a break opportunity only within a word. With
 *                word-wrap: break-word we will break at this point only if
 *                there are no other break opportunities in the line.
 * eNormalBreak   The line has a break opportunity determined by the standard
 *                line-breaking algorithm.
 *
 * Future expansion: split eNormalBreak into multiple priorities, e.g.
 *                    punctuation break and whitespace break (bug 389710).
 *                   As and when we implement it, text-wrap: unrestricted will
 *                    mean that priorities are ignored and all line-break
 *                    opportunities are equal.
 *
 * @see gfxTextRun::BreakAndMeasureText
 * @see nsLineLayout::NotifyOptionalBreakPosition
 */
enum gfxBreakPriority {
    eNoBreak       = 0,
    eWordWrapBreak,
    eNormalBreak
};

    /**
     * The format for an image surface. For all formats with alpha data, 0
     * means transparent, 1 or 255 means fully opaque.
     */
    enum gfxImageFormat {
        gfxImageFormatARGB32, ///< ARGB data in native endianness, using premultiplied alpha
        gfxImageFormatRGB24,  ///< xRGB data in native endianness
        gfxImageFormatA8,     ///< Only an alpha channel
        gfxImageFormatA1,     ///< Packed transparency information (one byte refers to 8 pixels)
        gfxImageFormatRGB16_565,  ///< RGB_565 data in native endianness
        gfxImageFormatUnknown
    };

    enum gfxSurfaceType {
        gfxSurfaceTypeImage,
        gfxSurfaceTypePDF,
        gfxSurfaceTypePS,
        gfxSurfaceTypeXlib,
        gfxSurfaceTypeXcb,
        gfxSurfaceTypeGlitz,           // unused, but needed for cairo parity
        gfxSurfaceTypeQuartz,
        gfxSurfaceTypeWin32,
        gfxSurfaceTypeBeOS,
        gfxSurfaceTypeDirectFB,        // unused, but needed for cairo parity
        gfxSurfaceTypeSVG,
        gfxSurfaceTypeOS2,
        gfxSurfaceTypeWin32Printing,
        gfxSurfaceTypeQuartzImage,
        gfxSurfaceTypeScript,
        gfxSurfaceTypeQPainter,
        gfxSurfaceTypeRecording,
        gfxSurfaceTypeVG,
        gfxSurfaceTypeGL,
        gfxSurfaceTypeDRM,
        gfxSurfaceTypeTee,
        gfxSurfaceTypeXML,
        gfxSurfaceTypeSkia,
        gfxSurfaceTypeSubsurface,
        gfxSurfaceTypeD2D,
        gfxSurfaceTypeMax
    };

    enum gfxContentType {
        GFX_CONTENT_COLOR       = 0x1000,
        GFX_CONTENT_ALPHA       = 0x2000,
        GFX_CONTENT_COLOR_ALPHA = 0x3000,
        GFX_CONTENT_SENTINEL    = 0xffff
    };

    /**
     * The memory used by this surface (as reported by KnownMemoryUsed()) can
     * either live in this process's heap, in this process but outside the
     * heap, or in another process altogether.
     */
    enum gfxMemoryLocation {
      GFX_MEMORY_IN_PROCESS_HEAP,
      GFX_MEMORY_IN_PROCESS_NONHEAP,
      GFX_MEMORY_OUT_OF_PROCESS
    };

#endif /* GFX_TYPES_H */
