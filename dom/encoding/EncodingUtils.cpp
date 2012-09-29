/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodingUtils.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

EncodingUtils* gEncodings = nullptr;

struct LabelEncoding
{
  const char* mLabel;
  const char* mEncoding;
};

static const LabelEncoding labelsEncodings[] = {
  {"unicode-1-1-utf-8", "utf-8"},
  {"utf-8", "utf-8"},
  {"utf8", "utf-8"},
  {"cp864", "ibm864"},
  {"csibm864", "ibm864"},
  {"ibm-864", "ibm864"},
  {"ibm864", "ibm864"},
  {"866", "ibm866"},
  {"cp866", "ibm866"},
  {"csibm866", "ibm866"},
  {"ibm866", "ibm866"},
  {"csisolatin2", "iso-8859-2"},
  {"iso-8859-2", "iso-8859-2"},
  {"iso-ir-101", "iso-8859-2"},
  {"iso8859-2", "iso-8859-2"},
  {"iso88592", "iso-8859-2"},
  {"iso_8859-2", "iso-8859-2"},
  {"iso_8859-2:1987", "iso-8859-2"},
  {"l2", "iso-8859-2"},
  {"latin2", "iso-8859-2"},
  {"csisolatin3", "iso-8859-3"},
  {"iso-8859-3", "iso-8859-3"},
  {"iso-ir-109", "iso-8859-3"},
  {"iso8859-3", "iso-8859-3"},
  {"iso88593", "iso-8859-3"},
  {"iso_8859-3", "iso-8859-3"},
  {"iso_8859-3:1988", "iso-8859-3"},
  {"l3", "iso-8859-3"},
  {"latin3", "iso-8859-3"},
  {"csisolatin4", "iso-8859-4"},
  {"iso-8859-4", "iso-8859-4"},
  {"iso-ir-110", "iso-8859-4"},
  {"iso8859-4", "iso-8859-4"},
  {"iso88594", "iso-8859-4"},
  {"iso_8859-4", "iso-8859-4"},
  {"iso_8859-4:1988", "iso-8859-4"},
  {"l4", "iso-8859-4"},
  {"latin4", "iso-8859-4"},
  {"csisolatincyrillic", "iso-8859-5"},
  {"cyrillic", "iso-8859-5"},
  {"iso-8859-5", "iso-8859-5"},
  {"iso-ir-144", "iso-8859-5"},
  {"iso8859-5", "iso-8859-5"},
  {"iso88595", "iso-8859-5"},
  {"iso_8859-5", "iso-8859-5"},
  {"iso_8859-5:1988", "iso-8859-5"},
  {"arabic", "iso-8859-6"},
  {"asmo-708", "iso-8859-6"},
  {"csiso88596e", "iso-8859-6"},
  {"csiso88596i", "iso-8859-6"},
  {"csisolatinarabic", "iso-8859-6"},
  {"ecma-114", "iso-8859-6"},
  {"iso-8859-6", "iso-8859-6"},
  {"iso-8859-6-e", "iso-8859-6"},
  {"iso-8859-6-i", "iso-8859-6"},
  {"iso-ir-127", "iso-8859-6"},
  {"iso8859-6", "iso-8859-6"},
  {"iso88596", "iso-8859-6"},
  {"iso_8859-6", "iso-8859-6"},
  {"iso_8859-6:1987", "iso-8859-6"},
  {"csisolatingreek", "iso-8859-7"},
  {"ecma-118", "iso-8859-7"},
  {"elot_928", "iso-8859-7"},
  {"greek", "iso-8859-7"},
  {"greek8", "iso-8859-7"},
  {"iso-8859-7", "iso-8859-7"},
  {"iso-ir-126", "iso-8859-7"},
  {"iso8859-7", "iso-8859-7"},
  {"iso88597", "iso-8859-7"},
  {"iso_8859-7", "iso-8859-7"},
  {"iso_8859-7:1987", "iso-8859-7"},
  {"sun_eu_greek", "iso-8859-7"},
  {"csiso88598e", "iso-8859-8"},
  {"csiso88598i", "iso-8859-8"},
  {"csisolatinhebrew", "iso-8859-8"},
  {"hebrew", "iso-8859-8"},
  {"iso-8859-8", "iso-8859-8"},
  {"iso-8859-8-e", "iso-8859-8"},
  {"iso-8859-8-i", "iso-8859-8"},
  {"iso-ir-138", "iso-8859-8"},
  {"iso8859-8", "iso-8859-8"},
  {"iso88598", "iso-8859-8"},
  {"iso_8859-8", "iso-8859-8"},
  {"iso_8859-8:1988", "iso-8859-8"},
  {"logical", "iso-8859-8"},
  {"visual", "iso-8859-8"},
  {"csisolatin6", "iso-8859-10"},
  {"iso-8859-10", "iso-8859-10"},
  {"iso-ir-157", "iso-8859-10"},
  {"iso8859-10", "iso-8859-10"},
  {"iso885910", "iso-8859-10"},
  {"l6", "iso-8859-10"},
  {"latin6", "iso-8859-10"},
  {"iso-8859-13", "iso-8859-13"},
  {"iso8859-13", "iso-8859-13"},
  {"iso885913", "iso-8859-13"},
  {"iso-8859-14", "iso-8859-14"},
  {"iso8859-14", "iso-8859-14"},
  {"iso885914", "iso-8859-14"},
  {"csisolatin9", "iso-8859-15"},
  {"iso-8859-15", "iso-8859-15"},
  {"iso8859-15", "iso-8859-15"},
  {"iso885915", "iso-8859-15"},
  {"iso_8859-15", "iso-8859-15"},
  {"l9", "iso-8859-15"},
  {"iso-8859-16", "iso-8859-16"},
  {"cskoi8r", "koi8-r"},
  {"koi", "koi8-r"},
  {"koi8", "koi8-r"},
  {"koi8-r", "koi8-r"},
  {"koi8_r", "koi8-r"},
  {"koi8-u", "koi8-u"},
  {"csmacintosh", "macintosh"},
  {"mac", "macintosh"},
  {"macintosh", "macintosh"},
  {"x-mac-roman", "macintosh"},
  {"dos-874", "windows-874"},
  {"iso-8859-11", "windows-874"},
  {"iso8859-11", "windows-874"},
  {"iso885911", "windows-874"},
  {"tis-620", "windows-874"},
  {"windows-874", "windows-874"},
  {"cp1250", "windows-1250"},
  {"windows-1250", "windows-1250"},
  {"x-cp1250", "windows-1250"},
  {"cp1251", "windows-1251"},
  {"windows-1251", "windows-1251"},
  {"x-cp1251", "windows-1251"},
  {"ansi_x3.4-1968", "windows-1252"},
  {"ascii", "windows-1252"},
  {"cp1252", "windows-1252"},
  {"cp819", "windows-1252"},
  {"csisolatin1", "windows-1252"},
  {"ibm819", "windows-1252"},
  {"iso-8859-1", "windows-1252"},
  {"iso-ir-100", "windows-1252"},
  {"iso8859-1", "windows-1252"},
  {"iso88591", "windows-1252"},
  {"iso_8859-1", "windows-1252"},
  {"iso_8859-1:1987", "windows-1252"},
  {"l1", "windows-1252"},
  {"latin1", "windows-1252"},
  {"us-ascii", "windows-1252"},
  {"windows-1252", "windows-1252"},
  {"x-cp1252", "windows-1252"},
  {"cp1253", "windows-1253"},
  {"windows-1253", "windows-1253"},
  {"x-cp1253", "windows-1253"},
  {"cp1254", "windows-1254"},
  {"csisolatin5", "windows-1254"},
  {"iso-8859-9", "windows-1254"},
  {"iso-ir-148", "windows-1254"},
  {"iso8859-9", "windows-1254"},
  {"iso88599", "windows-1254"},
  {"iso_8859-9", "windows-1254"},
  {"iso_8859-9:1989", "windows-1254"},
  {"l5", "windows-1254"},
  {"latin5", "windows-1254"},
  {"windows-1254", "windows-1254"},
  {"x-cp1254", "windows-1254"},
  {"cp1255", "windows-1255"},
  {"windows-1255", "windows-1255"},
  {"x-cp1255", "windows-1255"},
  {"cp1256", "windows-1256"},
  {"windows-1256", "windows-1256"},
  {"x-cp1256", "windows-1256"},
  {"cp1257", "windows-1257"},
  {"windows-1257", "windows-1257"},
  {"x-cp1257", "windows-1257"},
  {"cp1258", "windows-1258"},
  {"windows-1258", "windows-1258"},
  {"x-cp1258", "windows-1258"},
  {"x-mac-cyrillic", "x-mac-cyrillic"},
  {"x-mac-ukrainian", "x-mac-cyrillic"},
  {"chinese", "gbk"},
  {"csgb2312", "gbk"},
  {"csiso58gb231280", "gbk"},
  {"gb2312", "gbk"},
  {"gb_2312", "gbk"},
  {"gb_2312-80", "gbk"},
  {"gbk", "gbk"},
  {"iso-ir-58", "gbk"},
  {"x-gbk", "gbk"},
  {"gb18030", "gb18030"},
  {"hz-gb-2312", "hz-gb-2312"},
  {"big5", "big5"},
  {"big5-hkscs", "big5"},
  {"cn-big5", "big5"},
  {"csbig5", "big5"},
  {"x-x-big5", "big5"},
  {"cseucpkdfmtjapanese", "euc-jp"},
  {"euc-jp", "euc-jp"},
  {"x-euc-jp", "euc-jp"},
  {"csiso2022jp", "iso-2022-jp"},
  {"iso-2022-jp", "iso-2022-jp"},
  {"csshiftjis", "shift-jis"},
  {"ms_kanji", "shift-jis"},
  {"shift-jis", "shift-jis"},
  {"shift_jis", "shift-jis"},
  {"sjis", "shift-jis"},
  {"windows-31j", "shift-jis"},
  {"x-sjis", "shift-jis"},
  {"cseuckr", "euc-kr"},
  {"csksc56011987", "x-windows-949"},
  {"euc-kr", "x-windows-949"},
  {"iso-ir-149", "x-windows-949"},
  {"korean", "x-windows-949"},
  {"ks_c_5601-1987", "x-windows-949"},
  {"ks_c_5601-1989", "x-windows-949"},
  {"ksc5601", "x-windows-949"},
  {"ksc_5601", "x-windows-949"},
  {"windows-949", "x-windows-949"},
  {"csiso2022kr", "iso-2022-kr"},
  {"iso-2022-kr", "iso-2022-kr"},
  {"utf-16", "utf-16le"},
  {"utf-16le", "utf-16le"},
  {"utf-16be", "utf-16be"}
};

EncodingUtils::EncodingUtils()
{
  MOZ_ASSERT(!gEncodings);

  const uint32_t numLabels = ArrayLength(labelsEncodings);
  mLabelsEncodings.Init(numLabels);

  for (uint32_t i = 0; i < numLabels; i++) {
    mLabelsEncodings.Put(NS_ConvertASCIItoUTF16(labelsEncodings[i].mLabel),
                         labelsEncodings[i].mEncoding);
  }
}

EncodingUtils::~EncodingUtils()
{
  MOZ_ASSERT(gEncodings && gEncodings == this);
}

void
EncodingUtils::Shutdown()
{
  NS_IF_RELEASE(gEncodings);
}

already_AddRefed<EncodingUtils>
EncodingUtils::GetOrCreate()
{
  if (!gEncodings) {
    gEncodings = new EncodingUtils();
    NS_ADDREF(gEncodings);
  }
  NS_ADDREF(gEncodings);
  return gEncodings;
}

uint32_t
EncodingUtils::IdentifyDataOffset(const char* aData,
                                  const uint32_t aLength,
                                  const char*& aRetval)
{
  // Truncating to pre-clear return value in case of failure.
  aRetval = "";

  // Minimum bytes in input stream data that represents
  // the Byte Order Mark is 2. Max is 3.
  if (aLength < 2) {
    return 0;
  }

  if (aData[0] == '\xFF' && aData[1] == '\xFE') {
    aRetval = "utf-16le";
    return 2;
  }

  if (aData[0] == '\xFE' && aData[1] == '\xFF') {
    aRetval = "utf-16be";
    return 2;
  }

  // Checking utf-8 byte order mark.
  // Minimum bytes in input stream data that represents
  // the Byte Order Mark for utf-8 is 3.
  if (aLength < 3) {
    return 0;
  }

  if (aData[0] == '\xEF' && aData[1] == '\xBB' && aData[2] == '\xBF') {
    aRetval = "utf-8";
    return 3;
  }
  return 0;
}

bool
EncodingUtils::FindEncodingForLabel(const nsAString& aLabel,
                                    const char*& aOutEncoding)
{
  nsRefPtr<EncodingUtils> self = EncodingUtils::GetOrCreate();
  MOZ_ASSERT(self);

  // Save aLabel first because it may be the same as aOutEncoding.
  nsString label(aLabel);
  // Truncating to clear aOutEncoding in case of failure.
  aOutEncoding = EmptyCString().get();

  EncodingUtils::TrimSpaceCharacters(label);
  if (label.IsEmpty()) {
    return false;
  }

  ToLowerCase(label);
  const char* encoding = self->mLabelsEncodings.Get(label);
  if (!encoding) {
    return false;
  }
  aOutEncoding = encoding;
  return true;
}

} // namespace dom
} // namespace mozilla
