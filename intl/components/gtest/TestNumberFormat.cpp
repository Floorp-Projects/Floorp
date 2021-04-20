/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/NumberFormat.h"

namespace mozilla {
namespace intl {

template <typename C>
class Buffer {
 public:
  using CharType = C;

  bool allocate(size_t aSize) {
    mBuffer.resize(aSize);
    return true;
  }

  void* data() { return mBuffer.data(); }

  size_t size() const { return mBuffer.size(); }

  void written(size_t aAmount) { mWritten = aAmount; }

  std::vector<C> mBuffer;
  size_t mWritten = 0;
};

TEST(IntlNumberFormat, Basic)
{
  NumberFormat nf("en-US");
  Buffer<uint8_t> buf8;
  ASSERT_TRUE(nf.format(1234.56, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "1,234.56");
  Buffer<char16_t> buf16;
  ASSERT_TRUE(nf.format(1234.56, buf16));
  ASSERT_EQ(std::u16string_view(static_cast<const char16_t*>(buf16.data()),
                                buf16.mWritten),
            u"1,234.56");
  const char16_t* res16 = nf.format(1234.56);
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"1,234.56");

  NumberFormat nfAr("ar");
  ASSERT_TRUE(nfAr.format(1234.56, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "١٬٢٣٤٫٥٦");
  ASSERT_TRUE(nfAr.format(1234.56, buf16));
  ASSERT_EQ(std::u16string_view(static_cast<const char16_t*>(buf16.data()),
                                buf16.mWritten),
            u"١٬٢٣٤٫٥٦");
  res16 = nfAr.format(1234.56);
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"١٬٢٣٤٫٥٦");
}

TEST(IntlNumberFormat, Numbers)
{
  NumberFormat nf("es-ES");
  Buffer<uint8_t> buf8;
  ASSERT_TRUE(nf.format(123456.789, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "123.456,789");
  Buffer<char16_t> buf16;
  ASSERT_TRUE(nf.format(123456.789, buf16));
  ASSERT_EQ(std::u16string_view(static_cast<const char16_t*>(buf16.data()),
                                buf16.mWritten),
            u"123.456,789");
  const char16_t* res = nf.format(123456.789);
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"123.456,789");
}

TEST(IntlNumberFormat, SignificantDigits)
{
  NumberFormatOptions options;
  options.mSignificantDigits = Some(std::make_pair(3, 5));
  NumberFormat nf("es-ES", options);
  Buffer<uint8_t> buf8;
  ASSERT_TRUE(nf.format(123456.789, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "123.460");
  ASSERT_TRUE(nf.format(0.7, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "0,700");
}

TEST(IntlNumberFormat, Currency)
{
  NumberFormatOptions options;
  options.mCurrency = Some(
      std::make_pair("MXN", NumberFormatOptions::CurrencyDisplayStyle::Symbol));
  NumberFormat nf("es-MX", options);
  Buffer<uint8_t> buf8;
  ASSERT_TRUE(nf.format(123456.789, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "$123,456.79");
  Buffer<char16_t> buf16;
  ASSERT_TRUE(nf.format(123456.789, buf16));
  ASSERT_EQ(std::u16string_view(static_cast<const char16_t*>(buf16.data()),
                                buf16.mWritten),
            u"$123,456.79");
  const char16_t* res = nf.format(123456.789);
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"$123,456.79");
}

TEST(IntlNumberFormat, Unit)
{
  NumberFormatOptions options;
  options.mUnit = Some(std::make_pair("meter-per-second",
                                      NumberFormatOptions::UnitDisplay::Long));
  NumberFormat nf("es-MX", options);
  Buffer<uint8_t> buf8;
  ASSERT_TRUE(nf.format(12.34, buf8));
  ASSERT_EQ(
      std::string_view(static_cast<const char*>(buf8.data()), buf8.mWritten),
      "12.34 metros por segundo");
  Buffer<char16_t> buf16;
  ASSERT_TRUE(nf.format(12.34, buf16));
  ASSERT_EQ(std::u16string_view(static_cast<const char16_t*>(buf16.data()),
                                buf16.mWritten),
            u"12.34 metros por segundo");
  const char16_t* res = nf.format(12.34);
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros por segundo");

  // Create a string view into a longer string and make sure everything works
  // correctly.
  const char* unit = "meter-per-second-with-some-trailing-garbage";
  options.mUnit = Some(std::make_pair(std::string_view(unit, 5),
                                      NumberFormatOptions::UnitDisplay::Long));
  NumberFormat nf2("es-MX", options);
  res = nf2.format(12.34);
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros");

  options.mUnit = Some(std::make_pair(std::string_view(unit, 16),
                                      NumberFormatOptions::UnitDisplay::Long));
  NumberFormat nf3("es-MX", options);
  res = nf3.format(12.34);
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros por segundo");
}

}  // namespace intl
}  // namespace mozilla
