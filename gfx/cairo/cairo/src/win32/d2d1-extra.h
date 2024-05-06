/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Mingw-w64 does not have d2d1_2.h and d2d2_3.h.
 *
 *
 * We only need the definitions of two functions:
 *   ID2D1DeviceContext4::DrawColorBitmapGlyphRun()
 *   ID2D1DeviceContext4::DrawSvgGlyphRun()
 *
 * But we need to include all the prior functions in the same struct,
 * and parent structs, so that the functions are in the correct position
 * in the vtable. The parameters of the unused functions are not
 * required as we only need a function in the struct to create a
 * function pointer in the vtable.
 */

#ifndef D2D1_EXTRA_H
#define D2D1_EXTRA_H

#include <d2d1_1.h>

interface ID2D1DeviceContext1;
interface ID2D1DeviceContext2;
interface ID2D1DeviceContext3;
interface ID2D1DeviceContext4;
interface ID2D1SvgGlyphStyle;

enum D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION {
  D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT,
  D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DISABLE,
  D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_FORCE_DWORD
};

DEFINE_GUID(IID_ID2D1DeviceContext1, 0xd37f57e4, 0x6908, 0x459f, 0xa1, 0x99, 0xe7, 0x2f, 0x24, 0xf7, 0x99, 0x87);
MIDL_INTERFACE("d37f57e4-6908-459f-a199-e72f24f79987")
ID2D1DeviceContext1 : public ID2D1DeviceContext
{
  virtual void STDMETHODCALLTYPE CreateFilledGeometryRealization() = 0;
  virtual void STDMETHODCALLTYPE CreateStrokedGeometryRealization() = 0;
  virtual void STDMETHODCALLTYPE DrawGeometryRealization() = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID2D1DeviceContext1, 0xd37f57e4, 0x6908, 0x459f, 0xa1, 0x99, 0xe7, 0x2f, 0x24, 0xf7, 0x99, 0x87)
#endif


DEFINE_GUID(IID_ID2D1DeviceContext2, 0x394ea6a3, 0x0c34, 0x4321, 0x95, 0x0b, 0x6c, 0xa2, 0x0f, 0x0b, 0xe6, 0xc7);
MIDL_INTERFACE("394ea6a3-0c34-4321-950b-6ca20f0be6c7")
ID2D1DeviceContext2 : public ID2D1DeviceContext1
{
  virtual void STDMETHODCALLTYPE CreateInk() = 0;
  virtual void STDMETHODCALLTYPE CreateInkStyle() = 0;
  virtual void STDMETHODCALLTYPE CreateGradientMesh() = 0;
  virtual void STDMETHODCALLTYPE CreateImageSourceFromWic() = 0;
  virtual void STDMETHODCALLTYPE CreateLookupTable3D() = 0;
  virtual void STDMETHODCALLTYPE CreateImageSourceFromDxgi() = 0;
  virtual void STDMETHODCALLTYPE GetGradientMeshWorldBounds() = 0;
  virtual void STDMETHODCALLTYPE DrawInk() = 0;
  virtual void STDMETHODCALLTYPE DrawGradientMesh() = 0;
  virtual void STDMETHODCALLTYPE DrawGdiMetafile() = 0;
  virtual void STDMETHODCALLTYPE CreateTransformedImageSource() = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID2D1DeviceContext2, 0x394ea6a3, 0x0c34, 0x4321, 0x95, 0x0b, 0x6c, 0xa2, 0x0f, 0x0b, 0xe6, 0xc7)
#endif


DEFINE_GUID(IID_ID2D1DeviceContext3, 0x235a7496, 0x8351, 0x414c, 0xbc, 0xd4, 0x66, 0x72, 0xab, 0x2d, 0x8e, 0x00);
MIDL_INTERFACE("235a7496-8351-414c-bcd4-6672ab2d8e00")
ID2D1DeviceContext3 : public ID2D1DeviceContext2
{
  virtual void STDMETHODCALLTYPE CreateSpriteBatch() = 0;
  virtual void STDMETHODCALLTYPE DrawSpriteBatch() = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID2D1DeviceContext3, 0x235a7496, 0x8351, 0x414c, 0xbc, 0xd4, 0x66, 0x72, 0xab, 0x2d, 0x8e, 0x00)
#endif


DEFINE_GUID(IID_ID2D1SvgGlyphStyle, 0xaf671749, 0xd241, 0x4db8, 0x8e, 0x41, 0xdc, 0xc2, 0xe5, 0xc1, 0xa4, 0x38);
MIDL_INTERFACE("af671749-d241-4db8-8e41-dcc2e5c1a438")
ID2D1SvgGlyphStyle  : public ID2D1Resource
{
  virtual void STDMETHODCALLTYPE SetFill() = 0;
  virtual void STDMETHODCALLTYPE GetFill() = 0;
  virtual void STDMETHODCALLTYPE SetStroke() = 0;
  virtual void STDMETHODCALLTYPE GetStrokeDashesCount() = 0;
  virtual void STDMETHODCALLTYPE GetStroke() = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID2D1SvgGlyphStyle, 0xaf671749, 0xd241, 0x4db8, 0x8e, 0x41, 0xdc, 0xc2, 0xe5, 0xc1, 0xa4, 0x38)
#endif


DEFINE_GUID(IID_ID2D1DeviceContext4, 0x8c427831, 0x3d90, 0x4476, 0xb6, 0x47, 0xc4, 0xfa, 0xe3, 0x49, 0xe4, 0xdb);
MIDL_INTERFACE("8c427831-3d90-4476-b647-c4fae349e4db")
ID2D1DeviceContext4 : public ID2D1DeviceContext3
{
  virtual void STDMETHODCALLTYPE CreateSvgGlyphStyle() = 0;
  virtual void STDMETHODCALLTYPE DrawText() = 0;
  virtual void STDMETHODCALLTYPE DrawTextLayout() = 0;
  virtual void STDMETHODCALLTYPE DrawColorBitmapGlyphRun(
    DWRITE_GLYPH_IMAGE_FORMATS          glyphImageFormat,
    D2D1_POINT_2F                       baselineOrigin,
    const DWRITE_GLYPH_RUN              *glyphRun,
    DWRITE_MEASURING_MODE               measuringMode,
    D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION bitmapSnapOption) = 0;

  virtual void STDMETHODCALLTYPE DrawSvgGlyphRun(
    D2D1_POINT_2F          baselineOrigin,
    const DWRITE_GLYPH_RUN *glyphRun,
    ID2D1Brush             *defaultFillBrush,
    ID2D1SvgGlyphStyle     *svgGlyphStyle,
    UINT32                 colorPaletteIndex,
    DWRITE_MEASURING_MODE  measuringMode) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID2D1DeviceContext4, 0x8c427831, 0x3d90, 0x4476, 0xb6, 0x47, 0xc4, 0xfa, 0xe3, 0x49, 0xe4, 0xdb)
#endif

#endif
