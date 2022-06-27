// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "yuv_row.h"
#include "mozilla/SSE.h"

#define kCoefficientsRgbU kCoefficientsRgbY + 2048
#define kCoefficientsRgbV kCoefficientsRgbY + 4096

extern "C" {

#if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_M_IX86)
#if defined(__clang__)
// clang-cl has a bug where it doesn't mangle names in inline asm
// so let's do the mangling in the preprocessor (ugh)
// (but we still need to declare a dummy extern for the parser)
extern void* _kCoefficientsRgbY;
#define kCoefficientsRgbY _kCoefficientsRgbY
#endif

__declspec(naked)
void FastConvertYUVToRGB32Row_SSE(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       convertend

 convertloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [kCoefficientsRgbV + 8 * ebx]
    movzx     ebx, byte ptr [edx + 1]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    add       edx, 2
    movq      mm2, [kCoefficientsRgbY + 8 * ebx]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 convertend :
    sub       ecx, 2
    jns       convertloop

    and       ecx, 1  // odd number of pixels?
    jz        convertdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 convertdone :

    popad
    ret
  }
}

__declspec(naked)
void ConvertYUVToRGB32Row_SSE(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int step) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    mov       ebx, [esp + 32 + 24]  // step
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, ebx
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void RotateConvertYUVToRGB32Row_SSE(const uint8* y_buf,
                                    const uint8* u_buf,
                                    const uint8* v_buf,
                                    uint8* rgb_buf,
                                    int width,
                                    int ystep,
                                    int uvstep) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    mov       ebx, [esp + 32 + 28]  // uvstep
    add       edi, ebx
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    add       esi, ebx
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    mov       ebx, [esp + 32 + 24]  // ystep
    add       edx, ebx
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    movzx     eax, byte ptr [edx]
    add       edx, ebx
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 wend :
    sub       ecx, 2
    jns       wloop

    and       ecx, 1  // odd number of pixels?
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
 wdone :

    popad
    ret
  }
}

__declspec(naked)
void DoubleYUVToRGB32Row_SSE(const uint8* y_buf,
                             const uint8* u_buf,
                             const uint8* v_buf,
                             uint8* rgb_buf,
                             int width) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    jmp       wend

 wloop :
    movzx     eax, byte ptr [edi]
    add       edi, 1
    movzx     ebx, byte ptr [esi]
    add       esi, 1
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [edx]
    paddsw    mm0, [kCoefficientsRgbV + 8 * ebx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    punpckldq mm1, mm1
    movntq    [ebp], mm1

    movzx     ebx, byte ptr [edx + 1]
    add       edx, 2
    paddsw    mm0, [kCoefficientsRgbY + 8 * ebx]
    psraw     mm0, 6
    packuswb  mm0, mm0
    punpckldq mm0, mm0
    movntq    [ebp+8], mm0
    add       ebp, 16
 wend :
    sub       ecx, 4
    jns       wloop

    add       ecx, 4
    jz        wdone

    movzx     eax, byte ptr [edi]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    movzx     eax, byte ptr [esi]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    movzx     eax, byte ptr [edx]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    jmp       wend1

 wloop1 :
    movd      [ebp], mm1
    add       ebp, 4
 wend1 :
    sub       ecx, 1
    jns       wloop1
 wdone :
    popad
    ret
  }
}

// This version does general purpose scaling by any amount, up or down.
// The only thing it cannot do is rotation by 90 or 270.
// For performance the chroma is under-sampled, reducing cost of a 3x
// 1080p scale from 8.4 ms to 5.4 ms.
__declspec(naked)
void ScaleYUVToRGB32Row_SSE(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            uint8* rgb_buf,
                            int width,
                            int source_dx) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]   // Y
    mov       edi, [esp + 32 + 8]   // U
    mov       esi, [esp + 32 + 12]  // V
    mov       ebp, [esp + 32 + 16]  // rgb
    mov       ecx, [esp + 32 + 20]  // width
    xor       ebx, ebx              // x
    jmp       scaleend

 scaleloop :
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += source_dx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]  // x += source_dx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm2, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 6
    psraw     mm2, 6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 8
 scaleend :
    sub       ecx, 2
    jns       scaleloop

    and       ecx, 1  // odd number of pixels?
    jz        scaledone

    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [edi + eax]
    movq      mm0, [kCoefficientsRgbU + 8 * eax]
    mov       eax, ebx
    sar       eax, 17
    movzx     eax, byte ptr [esi + eax]
    paddsw    mm0, [kCoefficientsRgbV + 8 * eax]
    mov       eax, ebx
    sar       eax, 16
    movzx     eax, byte ptr [edx + eax]
    movq      mm1, [kCoefficientsRgbY + 8 * eax]
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1

 scaledone :
    popad
    ret
  }
}

__declspec(naked)
void LinearScaleYUVToRGB32Row_SSE(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width,
                                  int source_dx) {
  __asm {
    pushad
    mov       edx, [esp + 32 + 4]  // Y
    mov       edi, [esp + 32 + 8]  // U
                // [esp + 32 + 12] // V
    mov       ebp, [esp + 32 + 16] // rgb
    mov       ecx, [esp + 32 + 20] // width
    imul      ecx, [esp + 32 + 24] // source_dx
    mov       [esp + 32 + 20], ecx // source_width = width * source_dx
    mov       ecx, [esp + 32 + 24] // source_dx
    xor       ebx, ebx             // x = 0
    cmp       ecx, 0x20000
    jl        lscaleend
    mov       ebx, 0x8000          // x = 0.5 for 1/2 or less
    jmp       lscaleend
lscaleloop:
    mov       eax, ebx
    sar       eax, 0x11

    movzx     ecx, byte ptr [edi + eax]
    movzx     esi, byte ptr [edi + eax + 1]
    mov       eax, ebx
    and       eax, 0x1fffe
    imul      esi, eax
    xor       eax, 0x1fffe
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 17
    movq      mm0, [kCoefficientsRgbU + 8 * ecx]

    mov       esi, [esp + 32 + 12]
    mov       eax, ebx
    sar       eax, 0x11

    movzx     ecx, byte ptr [esi + eax]
    movzx     esi, byte ptr [esi + eax + 1]
    mov       eax, ebx
    and       eax, 0x1fffe
    imul      esi, eax
    xor       eax, 0x1fffe
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 17
    paddsw    mm0, [kCoefficientsRgbV + 8 * ecx]

    mov       eax, ebx
    sar       eax, 0x10
    movzx     ecx, byte ptr [edx + eax]
    movzx     esi, byte ptr [1 + edx + eax]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]
    and       eax, 0xffff
    imul      esi, eax
    xor       eax, 0xffff
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 16
    movq      mm1, [kCoefficientsRgbY + 8 * ecx]

    cmp       ebx, [esp + 32 + 20]
    jge       lscalelastpixel

    mov       eax, ebx
    sar       eax, 0x10
    movzx     ecx, byte ptr [edx + eax]
    movzx     esi, byte ptr [edx + eax + 1]
    mov       eax, ebx
    add       ebx, [esp + 32 + 24]
    and       eax, 0xffff
    imul      esi, eax
    xor       eax, 0xffff
    imul      ecx, eax
    add       ecx, esi
    shr       ecx, 16
    movq      mm2, [kCoefficientsRgbY + 8 * ecx]

    paddsw    mm1, mm0
    paddsw    mm2, mm0
    psraw     mm1, 0x6
    psraw     mm2, 0x6
    packuswb  mm1, mm2
    movntq    [ebp], mm1
    add       ebp, 0x8

lscaleend:
    cmp       ebx, [esp + 32 + 20]
    jl        lscaleloop
    popad
    ret

lscalelastpixel:
    paddsw    mm1, mm0
    psraw     mm1, 6
    packuswb  mm1, mm1
    movd      [ebp], mm1
    popad
    ret
  };
}
#endif // if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_M_IX86)

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
#if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_M_IX86)
  if (mozilla::supports_sse()) {
    FastConvertYUVToRGB32Row_SSE(y_buf, u_buf, v_buf, rgb_buf, width);
    return;
  }
#endif

  FastConvertYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, 1);
}

void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int source_dx) {

#if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_M_IX86)
  if (mozilla::supports_sse()) {
    ScaleYUVToRGB32Row_SSE(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
    return;
  }
#endif

  ScaleYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int source_dx) {
#if defined(MOZILLA_MAY_SUPPORT_SSE) && defined(_M_IX86)
  if (mozilla::supports_sse()) {
    LinearScaleYUVToRGB32Row_SSE(y_buf, u_buf, v_buf, rgb_buf, width,
                                 source_dx);
    return;
  }
#endif

  LinearScaleYUVToRGB32Row_C(y_buf, u_buf, v_buf, rgb_buf, width, source_dx);
}

} // extern "C"
