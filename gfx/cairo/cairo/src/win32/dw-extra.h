/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* MinGW workarounds
 *
 * It doesn't define operators for DWRITE_GLYPH_IMAGE_FORMATS.
 * The DWRITE_COLOR_GLYPH_RUN struct isn't valid.
 * <https://sourceforge.net/p/mingw-w64/bugs/913/>
 */

#ifndef DWRITE_EXTRA_H
#define DWRITE_EXTRA_H

#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 10

typedef struct DWRITE_COLOR_GLYPH_RUN1_WORKAROUND DWRITE_COLOR_GLYPH_RUN1_WORKAROUND;
struct DWRITE_COLOR_GLYPH_RUN1_WORKAROUND : DWRITE_COLOR_GLYPH_RUN
{
    DWRITE_GLYPH_IMAGE_FORMATS glyphImageFormat;
    DWRITE_MEASURING_MODE measuringMode;
};

#else
typedef DWRITE_COLOR_GLYPH_RUN1 DWRITE_COLOR_GLYPH_RUN1_WORKAROUND;
#endif

DEFINE_ENUM_FLAG_OPERATORS(DWRITE_GLYPH_IMAGE_FORMATS);

#endif /* DWRITE_EXTRA_H */
