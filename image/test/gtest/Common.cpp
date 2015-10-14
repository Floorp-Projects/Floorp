/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#include <cstdlib>
#include "gtest/gtest.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIProperties.h"
#include "nsNetUtil.h"
#include "mozilla/RefPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"

namespace mozilla {

using namespace gfx;

using std::abs;

///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////

// These macros work like gtest's ASSERT_* macros, except that they can be used
// in functions that return values.
#define ASSERT_TRUE_OR_RETURN(e, rv) \
  EXPECT_TRUE(e);                    \
  if (!(e)) {                        \
    return rv;                       \
  }

#define ASSERT_EQ_OR_RETURN(a, b, rv) \
  EXPECT_EQ(a, b);                    \
  if ((a) != (b)) {                   \
    return rv;                        \
  }

#define ASSERT_LE_OR_RETURN(a, b, rv) \
  EXPECT_LE(a, b);                    \
  if (!((a) <= (b))) {                 \
    return rv;                        \
  }

already_AddRefed<nsIInputStream>
LoadFile(const char* aRelativePath)
{
  nsresult rv;

  nsCOMPtr<nsIProperties> dirService =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  ASSERT_TRUE_OR_RETURN(dirService != nullptr, nullptr);

  // Retrieve the current working directory.
  nsCOMPtr<nsIFile> file;
  rv = dirService->Get(NS_OS_CURRENT_WORKING_DIR,
                       NS_GET_IID(nsIFile), getter_AddRefs(file));
  ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);

  // Construct the final path by appending the working path to the current
  // working directory.
  file->AppendNative(nsAutoCString(aRelativePath));

  // Construct an input stream for the requested file.
  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), file);
  ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);

  // Ensure the resulting input stream is buffered.
  if (!NS_InputStreamIsBuffered(inputStream)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream),
                                   inputStream, 1024);
    ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);
    inputStream = bufStream;
  }

  return inputStream.forget();
}

bool
IsSolidColor(SourceSurface* aSurface, BGRAColor aColor, bool aFuzzy)
{
  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  ASSERT_TRUE_OR_RETURN(dataSurface != nullptr, false);

  ASSERT_EQ_OR_RETURN(dataSurface->Stride(), aSurface->GetSize().width * 4,
                      false);

  DataSourceSurface::ScopedMap mapping(dataSurface,
                                       DataSourceSurface::MapType::READ);
  ASSERT_TRUE_OR_RETURN(mapping.IsMapped(), false);

  uint8_t* data = dataSurface->GetData();
  ASSERT_TRUE_OR_RETURN(data != nullptr, false);

  int32_t length = dataSurface->Stride() * aSurface->GetSize().height;
  for (int32_t i = 0 ; i < length ; i += 4) {
    if (aFuzzy) {
      ASSERT_LE_OR_RETURN(abs(aColor.mBlue - data[i + 0]), 1, false);
      ASSERT_LE_OR_RETURN(abs(aColor.mGreen - data[i + 1]), 1, false);
      ASSERT_LE_OR_RETURN(abs(aColor.mRed - data[i + 2]), 1, false);
      ASSERT_LE_OR_RETURN(abs(aColor.mAlpha - data[i + 3]), 1, false);
    } else {
      ASSERT_EQ_OR_RETURN(aColor.mBlue,  data[i + 0], false);
      ASSERT_EQ_OR_RETURN(aColor.mGreen, data[i + 1], false);
      ASSERT_EQ_OR_RETURN(aColor.mRed,   data[i + 2], false);
      ASSERT_EQ_OR_RETURN(aColor.mAlpha, data[i + 3], false);
    }
  }

  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Test Data
///////////////////////////////////////////////////////////////////////////////

ImageTestCase GreenPNGTestCase()
{
  return ImageTestCase("green.png", "image/png", IntSize(100, 100));
}

ImageTestCase GreenGIFTestCase()
{
  return ImageTestCase("green.gif", "image/gif", IntSize(100, 100));
}

ImageTestCase GreenJPGTestCase()
{
  return ImageTestCase("green.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_IS_FUZZY);
}

ImageTestCase GreenBMPTestCase()
{
  return ImageTestCase("green.bmp", "image/bmp", IntSize(100, 100));
}

ImageTestCase GreenICOTestCase()
{
  // This ICO contains a 32-bit BMP, and we use a BMP's alpha data by default
  // when the BMP is embedded in an ICO, so it's transparent.
  return ImageTestCase("green.ico", "image/x-icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenIconTestCase()
{
  return ImageTestCase("green.icon", "image/icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenFirstFrameAnimatedGIFTestCase()
{
  return ImageTestCase("first-frame-green.gif", "image/gif", IntSize(100, 100),
                       TEST_CASE_IS_ANIMATED);
}

ImageTestCase GreenFirstFrameAnimatedPNGTestCase()
{
  return ImageTestCase("first-frame-green.png", "image/png", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IS_ANIMATED);
}

ImageTestCase CorruptTestCase()
{
  return ImageTestCase("corrupt.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_HAS_ERROR);
}

ImageTestCase TransparentPNGTestCase()
{
  return ImageTestCase("transparent.png", "image/png", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentGIFTestCase()
{
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase FirstFramePaddingGIFTestCase()
{
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentIfWithinICOBMPTestCase(TestCaseFlags aFlags)
{
  // This is a BMP that is only transparent when decoded as if it is within an
  // ICO file. (Note: aFlags needs to be set to TEST_CASE_DEFAULT_FLAGS or
  // TEST_CASE_IS_TRANSPARENT accordingly.)
  return ImageTestCase("transparent-if-within-ico.bmp", "image/bmp",
                       IntSize(32, 32), aFlags);
}

ImageTestCase RLE4BMPTestCase()
{
  return ImageTestCase("rle4.bmp", "image/bmp", IntSize(320, 240),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase RLE8BMPTestCase()
{
  return ImageTestCase("rle8.bmp", "image/bmp", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase NoFrameDelayGIFTestCase()
{
  // This is an invalid (or at least, questionably valid) GIF that's animated
  // even though it specifies a frame delay of zero. It's animated, but it's not
  // marked TEST_CASE_IS_ANIMATED because the metadata decoder can't detect that
  // it's animated.
  return ImageTestCase("no-frame-delay.gif", "image/gif", IntSize(100, 100));
}

} // namespace mozilla
