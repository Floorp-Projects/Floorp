/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/ArrayUtils.h"
#include "qcms.h"
#include "transform_util.h"

const size_t allGBSize = 1 * 256 * 256 * 4;
static unsigned char* createAllGB() {
  unsigned char* buff = (unsigned char*)malloc(allGBSize);
  int pos = 0;
  for (int r = 0; r < 1; r++) {  // Skip all R values for speed
    for (int g = 0; g < 256; g++) {
      for (int b = 0; b < 256; b++) {
        buff[pos * 4 + 0] = r;
        buff[pos * 4 + 1] = g;
        buff[pos * 4 + 2] = b;
        buff[pos * 4 + 3] = 0x80;
        pos++;
      }
    }
  }

  return buff;
}

TEST(GfxQcms, Identity)
{
  // XXX: This means that we can't have qcms v2 unit test
  //      without changing the qcms API.
  qcms_enable_iccv4();

  qcms_profile* input_profile = qcms_profile_sRGB();
  qcms_profile* output_profile = qcms_profile_sRGB();

  EXPECT_FALSE(qcms_profile_is_bogus(input_profile));
  EXPECT_FALSE(qcms_profile_is_bogus(output_profile));

  const qcms_intent intent = QCMS_INTENT_DEFAULT;
  qcms_data_type input_type = QCMS_DATA_RGBA_8;
  qcms_data_type output_type = QCMS_DATA_RGBA_8;

  qcms_transform* transform = qcms_transform_create(
      input_profile, input_type, output_profile, output_type, intent);

  unsigned char* data_in = createAllGB();
  ;
  unsigned char* data_out = (unsigned char*)malloc(allGBSize);
  qcms_transform_data(transform, data_in, data_out, allGBSize / 4);

  qcms_profile_release(input_profile);
  qcms_profile_release(output_profile);
  qcms_transform_release(transform);

  free(data_in);
  free(data_out);
}

TEST(GfxQcms, LutInverseCrash)
{
  uint16_t lutTable1[] = {
      0x0000, 0x0000, 0x0000, 0x8000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF,
  };
  uint16_t lutTable2[] = {
      0xFFF0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0xFFFF,
  };

  // Crash/Assert test
  lut_inverse_interp16((uint16_t)5, lutTable1,
                       (int)mozilla::ArrayLength(lutTable1));
  lut_inverse_interp16((uint16_t)5, lutTable2,
                       (int)mozilla::ArrayLength(lutTable2));
}

TEST(GfxQcms, LutInverse)
{
  // mimic sRGB_v4_ICC mBA Output
  //
  //       XXXX
  //      X
  //     X
  // XXXX
  uint16_t value;
  uint16_t lutTable[256];

  for (int i = 0; i < 20; i++) {
    lutTable[i] = 0;
  }

  for (int i = 20; i < 200; i++) {
    lutTable[i] = (i - 20) * 0xFFFF / (200 - 20);
  }

  for (int i = 200; i < (int)mozilla::ArrayLength(lutTable); i++) {
    lutTable[i] = 0xFFFF;
  }

  for (uint16_t i = 0; i < 65535; i++) {
    lut_inverse_interp16(i, lutTable, (int)mozilla::ArrayLength(lutTable));
  }

  // Lookup the interesting points

  value =
      lut_inverse_interp16(0, lutTable, (int)mozilla::ArrayLength(lutTable));
  EXPECT_LE(value, 20 * 256);

  value =
      lut_inverse_interp16(1, lutTable, (int)mozilla::ArrayLength(lutTable));
  EXPECT_GT(value, 20 * 256);

  value = lut_inverse_interp16(65535, lutTable,
                               (int)mozilla::ArrayLength(lutTable));
  EXPECT_LT(value, 201 * 256);
}

TEST(GfxQcms, LutInverseNonMonotonic)
{
  // Make sure we behave sanely for non monotic functions
  //   X  X  X
  //  X  X  X
  // X  X  X
  uint16_t lutTable[256];

  for (int i = 0; i < 100; i++) {
    lutTable[i] = (i - 0) * 0xFFFF / (100 - 0);
  }

  for (int i = 100; i < 200; i++) {
    lutTable[i] = (i - 100) * 0xFFFF / (200 - 100);
  }

  for (int i = 200; i < 256; i++) {
    lutTable[i] = (i - 200) * 0xFFFF / (256 - 200);
  }

  for (uint16_t i = 0; i < 65535; i++) {
    lut_inverse_interp16(i, lutTable, (int)mozilla::ArrayLength(lutTable));
  }

  // Make sure we don't crash, hang or let sanitizers do their magic
}
