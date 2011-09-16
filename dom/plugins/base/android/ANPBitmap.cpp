#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_bitmap_##name


#define SK_A32_BITS     8
#define SK_R32_BITS     8
#define SK_G32_BITS     8
#define SK_B32_BITS     8

#ifdef IS_BIG_ENDIAN
#define SK_R32_SHIFT    24
#define SK_G32_SHIFT    16
#define SK_B32_SHIFT    8
#define SK_A32_SHIFT    0
#else
#define SK_R32_SHIFT    0
#define SK_G32_SHIFT    8
#define SK_B32_SHIFT    16
#define SK_A32_SHIFT    24
#endif

#define SK_A32_MASK     ((1 << SK_A32_BITS) - 1)
#define SK_R32_MASK     ((1 << SK_R32_BITS) - 1)
#define SK_G32_MASK     ((1 << SK_G32_BITS) - 1)
#define SK_B32_MASK     ((1 << SK_B32_BITS) - 1)

#define SK_R16_BITS     5
#define SK_G16_BITS     6
#define SK_B16_BITS     5

#define SK_R16_SHIFT    (SK_B16_BITS + SK_G16_BITS)
#define SK_G16_SHIFT    (SK_B16_BITS)
#define SK_B16_SHIFT    0

#define SK_R16_MASK     ((1 << SK_R16_BITS) - 1)
#define SK_G16_MASK     ((1 << SK_G16_BITS) - 1)
#define SK_B16_MASK     ((1 << SK_B16_BITS) - 1)

bool
anp_bitmap_getPixelPacking(ANPBitmapFormat fmt, ANPPixelPacking* packing) {
  LOG("%s", __PRETTY_FUNCTION__);
  switch (fmt) {
    case kRGBA_8888_ANPBitmapFormat:
      if (packing) {
        packing->AShift = SK_A32_SHIFT;
        packing->ABits  = SK_A32_BITS;
        packing->RShift = SK_R32_SHIFT;
        packing->RBits  = SK_R32_BITS;
        packing->GShift = SK_G32_SHIFT;
        packing->GBits  = SK_G32_BITS;
        packing->BShift = SK_B32_SHIFT;
        packing->BBits  = SK_B32_BITS;
      }
      return true;
    case kRGB_565_ANPBitmapFormat:
      if (packing) {
        packing->AShift = 0;
        packing->ABits  = 0;
        packing->RShift = SK_R16_SHIFT;
        packing->RBits  = SK_R16_BITS;
        packing->GShift = SK_G16_SHIFT;
        packing->GBits  = SK_G16_BITS;
        packing->BShift = SK_B16_SHIFT;
        packing->BBits  = SK_B16_BITS;
      }
      return true;
  default:
    break;
  }
  return false;
}

void InitBitmapInterface(ANPBitmapInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, getPixelPacking);
}
