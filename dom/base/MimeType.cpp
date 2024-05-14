/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MimeType.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"

#include "mozilla/Logging.h"

using mozilla::LogLevel;

mozilla::LazyLogModule gMimeTypeLog("MimeType");

#define DEBUG_LOG(args) MOZ_LOG(gMimeTypeLog, LogLevel::Debug, args)
#define VERBOSE_LOG(args) MOZ_LOG(gMimeTypeLog, LogLevel::Verbose, args)
#define LOGSTR(x) (mozilla::ToString(x).c_str())

template <typename char_type>
/* static */ RefPtr<TMimeType<char_type>> TMimeType<char_type>::Parse(
    const nsTSubstring<char_type>& aMimeType) {
  // See https://mimesniff.spec.whatwg.org/#parsing-a-mime-type

  // Steps 1-2
  const char_type* pos = aMimeType.BeginReading();
  const char_type* end = aMimeType.EndReading();
  while (pos < end && NS_IsHTTPWhitespace(*pos)) {
    ++pos;
  }
  if (pos == end) {
    DEBUG_LOG(("Parse(%s): fail, empty", LOGSTR(aMimeType)));
    return nullptr;
  }
  while (end > pos && NS_IsHTTPWhitespace(*(end - 1))) {
    --end;
  }

  // Steps 3-4
  const char_type* typeStart = pos;
  while (pos < end && *pos != '/') {
    if (!NS_IsHTTPTokenPoint(*pos)) {
      DEBUG_LOG(("Parse(%s): fail, type has invalid char '%c'",
                 LOGSTR(aMimeType), *pos));
      return nullptr;
    }
    ++pos;
  }
  const char_type* typeEnd = pos;
  if (typeStart == typeEnd) {
    DEBUG_LOG(("Parse(%s): fail, empty type", LOGSTR(aMimeType)));
    return nullptr;
  }

  // Step 5
  if (pos == end) {
    DEBUG_LOG(("Parse(%s): no subtype", LOGSTR(aMimeType)));
    return nullptr;
  }

  // Step 6
  ++pos;

  // Step 7-9
  const char_type* subtypeStart = pos;
  const char_type* subtypeEnd = nullptr;
  while (pos < end && *pos != ';') {
    if (!NS_IsHTTPTokenPoint(*pos)) {
      // If we hit a whitespace, check that the rest of
      // the subtype is whitespace, otherwise fail.
      if (NS_IsHTTPWhitespace(*pos)) {
        subtypeEnd = pos;
        ++pos;
        while (pos < end && *pos != ';') {
          if (!NS_IsHTTPWhitespace(*pos)) {
            DEBUG_LOG(
                ("Parse(%s): fail, subtype has extra '%c' between space and ;",
                 LOGSTR(aMimeType), *pos));
            return nullptr;
          }
          ++pos;
        }
        break;
      }
      DEBUG_LOG(("Parse(%s): fail, subtype has invalid char '%c'",
                 LOGSTR(aMimeType), *pos));
      return nullptr;
    }
    ++pos;
  }
  if (subtypeEnd == nullptr) {
    subtypeEnd = pos;
  }
  if (subtypeStart == subtypeEnd) {
    DEBUG_LOG(("Parse(%s): fail, no subtype", LOGSTR(aMimeType)));
    return nullptr;
  }

  // Step 10
  nsTString<char_type> type;
  nsTString<char_type> subtype;
  for (const char_type* c = typeStart; c < typeEnd; ++c) {
    type.Append(ToLowerCaseASCII(*c));
  }
  for (const char_type* c = subtypeStart; c < subtypeEnd; ++c) {
    subtype.Append(ToLowerCaseASCII(*c));
  }
  RefPtr<TMimeType<char_type>> mimeType =
      new TMimeType<char_type>(type, subtype);

  // Step 11
  while (pos < end) {
    // Step 11.1
    ++pos;

    // Step 11.2
    while (pos < end && NS_IsHTTPWhitespace(*pos)) {
      ++pos;
    }

    const char_type* namePos = pos;

    // Steps 11.3 and 11.4
    nsTString<char_type> paramName;
    bool paramNameHadInvalidChars = false;
    while (pos < end && *pos != ';' && *pos != '=') {
      if (!NS_IsHTTPTokenPoint(*pos)) {
        paramNameHadInvalidChars = true;
      }
      paramName.Append(ToLowerCaseASCII(*pos));
      ++pos;
    }

    // Might as well check for base64 now
    if (*pos != '=') {
      // trim leading and trailing spaces
      while (namePos < pos && NS_IsHTTPWhitespace(*namePos)) {
        ++namePos;
      }
      if (namePos < pos && ToLowerCaseASCII(*namePos) == 'b' &&
          ++namePos < pos && ToLowerCaseASCII(*namePos) == 'a' &&
          ++namePos < pos && ToLowerCaseASCII(*namePos) == 's' &&
          ++namePos < pos && ToLowerCaseASCII(*namePos) == 'e' &&
          ++namePos < pos && ToLowerCaseASCII(*namePos) == '6' &&
          ++namePos < pos && ToLowerCaseASCII(*namePos) == '4') {
        while (++namePos < pos && NS_IsHTTPWhitespace(*namePos)) {
        }
        mimeType->mIsBase64 = namePos == pos;
      }
    }

    // Step 11.5
    if (pos < end) {
      if (*pos == ';') {
        VERBOSE_LOG(("Parse(%s): ignoring param %s with no value",
                     LOGSTR(aMimeType), LOGSTR(paramName)));
        continue;
      }
      ++pos;
    }

    // Step 11.6
    if (pos == end) {
      VERBOSE_LOG(("Parse(%s): ignoring param %s with no value",
                   LOGSTR(aMimeType), LOGSTR(paramName)));
      break;
    }

    // Step 11.7
    ParameterValue paramValue;
    bool paramValueHadInvalidChars = false;

    // Step 11.8
    if (*pos == '"') {
      // Step 11.8.1 (collect an HTTP quoted string)
      ++pos;
      while (true) {
        while (pos < end && *pos != '"' && *pos != '\\') {
          if (!NS_IsHTTPQuotedStringTokenPoint(*pos)) {
            paramValueHadInvalidChars = true;
          }
          if (!NS_IsHTTPTokenPoint(*pos)) {
            paramValue.mRequiresQuoting = true;
          }
          paramValue.Append(*pos);
          ++pos;
        }

        if (pos < end && *pos == '\\') {
          ++pos;
          if (pos < end) {
            if (!NS_IsHTTPQuotedStringTokenPoint(*pos)) {
              paramValueHadInvalidChars = true;
            }
            if (!NS_IsHTTPTokenPoint(*pos)) {
              paramValue.mRequiresQuoting = true;
            }
            paramValue.Append(*pos);
            ++pos;
            continue;
          }

          paramValue.Append('\\');
          paramValue.mRequiresQuoting = true;
        }

        break;
      }

      // Step 11.8.3
      while (pos < end && *pos != ';') {
        ++pos;
      }

      // Step 11.9
    } else {
      // Step 11.9.1
      const char_type* paramValueStart = pos;
      while (pos < end && *pos != ';') {
        ++pos;
      }

      // Step 11.9.2
      const char_type* paramValueLastChar = pos - 1;
      while (paramValueLastChar >= paramValueStart &&
             NS_IsHTTPWhitespace(*paramValueLastChar)) {
        --paramValueLastChar;
      }

      // Step 11.9.3
      if (paramValueStart > paramValueLastChar) {
        VERBOSE_LOG(("Parse(%s): skipping empty unquoted param %s",
                     LOGSTR(aMimeType), LOGSTR(paramName)));
        continue;
      }

      for (const char_type* c = paramValueStart; c <= paramValueLastChar; ++c) {
        if (!NS_IsHTTPQuotedStringTokenPoint(*c)) {
          paramValueHadInvalidChars = true;
        }
        if (!NS_IsHTTPTokenPoint(*c)) {
          paramValue.mRequiresQuoting = true;
        }
        paramValue.Append(*c);
      }
    }

    // Step 11.10
    if (!paramName.IsEmpty() && !paramNameHadInvalidChars &&
        !paramValueHadInvalidChars) {
      paramValue = mimeType->mParameters.LookupOrInsertWith(paramName, [&] {
        mimeType->mParameterNames.AppendElement(paramName);
        return paramValue;
      });
      VERBOSE_LOG(("Parse(%s): found param %s=%s (requires quoting=%d)",
                   LOGSTR(aMimeType), LOGSTR(paramName), LOGSTR(paramValue),
                   paramValue.mRequiresQuoting));
    } else {
      VERBOSE_LOG(
          ("Parse(%s): skipping param %s (empty=%d, invalid name=%d, invalid "
           "value=%d)",
           LOGSTR(aMimeType), LOGSTR(paramName), paramName.IsEmpty(),
           paramNameHadInvalidChars, paramValueHadInvalidChars));
    }
  }

  // Step 12
  DEBUG_LOG(("Parse(%s): %s", LOGSTR(aMimeType), LOGSTR(mimeType)));
  return mimeType;
}

template <typename char_type>
const char_type* CollectHTTPQuotedString(const char_type* pos,
                                         const char_type* end,
                                         nsTString<char_type>& value,
                                         bool extractValue = false) {
  // See https://fetch.spec.whatwg.org/#collect-an-http-quoted-string

  // Step 1 unnecessary, we just add the quotes too as we go if !extractValue.

  // Step 2 intentionally skipped as we're using an outparam for the result.

  // Step 3
  MOZ_ASSERT(*pos == '"');
  if (!extractValue) {
    value.Append('"');
  }

  // Step 4
  ++pos;

  // Step 5
  while (true) {
    // Step 5.1
    while (pos < end && *pos != '"' && *pos != '\\') {
      value.Append(*pos);
      ++pos;
    }

    // Step 5.2
    if (pos >= end) {
      break;
    }

    // Step 5.3
    bool isBackslash = *pos == '\\';

    // Step 5.4
    ++pos;

    // Step 5.5
    if (isBackslash) {
      // Step 5.5.1
      if (pos >= end) {
        value.Append('\\');
        break;
      }

      // Step 5.5.2
      value.Append(*pos);

      // Step 5.5.3
      ++pos;

    } else {
      // Step 5.6.1
      MOZ_ASSERT(*(pos - 1) == '"');

      if (!extractValue) {
        value.Append('"');
      }

      // Step 5.6.2
      break;
    }
  }

  return pos;
}

template <typename char_type>
void GetDecodeAndSplitHeaderValue(
    const nsTSubstring<char_type>& aHeaderValue,
    const std::function<
        void(const nsTDependentSubstring<char_type>& nextValue)>& aCallback) {
  // See https://fetch.spec.whatwg.org/#header-value-get-decode-and-split

  // Step 1 is already done for us

  // Step 2
  const char_type* pos = aHeaderValue.BeginReading();
  const char_type* end = aHeaderValue.EndReading();

  // Steps 3 skipped; we'll just call aCallback for each value.

  // Step 4
  nsTString<char_type> tempValue;

  // Step 5
  while (pos < end) {
    // Step 5.1
    while (pos < end && *pos != '"' && *pos != ',') {
      tempValue.Append(*pos);
      ++pos;
    }

    // Step 5.2
    if (pos < end) {
      // Step 5.2.1
      if (*pos == '"') {
        // Step 5.2.1.1
        pos = CollectHTTPQuotedString(pos, end, tempValue);

        // Step 5.2.1.2
        if (pos < end) {
          VERBOSE_LOG(
              ("GetDecodeAndSplitHeaderValue saw quoted string up to '%c'",
               *pos));
          continue;
        }

        VERBOSE_LOG(
            ("GetDecodeAndSplitHeaderValue saw quoted string to the end"));

        // Step 5.2.2
      } else {
        // Step 5.2.2.1
        MOZ_ASSERT(*pos == ',');

        // Step 5.2.2.2
        ++pos;
      }
    }

    // Step 5.3
    const char_type* first = tempValue.BeginReading();
    const char_type* last = tempValue.EndReading();
    while (first < last && (*first == ' ' || *first == '\t')) {
      ++first;
    }
    while (last > first && (*last == ' ' || *last == '\t')) {
      --last;
    }

    VERBOSE_LOG(("GetDecodeAndSplitHeaderValue found '%s'",
                 LOGSTR(nsTDependentSubstring<char_type>(first, last))));

    // Step 5.4
    aCallback(nsTDependentSubstring<char_type>(first, last));

    // Step 5.5
    tempValue.Truncate();
  }
}

template <typename char_type>
/* static */ RefPtr<TMimeType<char_type>> TMimeType<char_type>::ExtractMIMEType(
    const nsTSubstring<char_type>& aContentTypeHeader) {
  // See https://fetch.spec.whatwg.org/#content-type-header

  // Step 5 (might as well do this first)
  if (aContentTypeHeader.IsEmpty()) {
    DEBUG_LOG(("ExtractMimeType(empty) failed"));
    return nullptr;
  }

  VERBOSE_LOG(("  ExtractMimeType(%s) called", LOGSTR(aContentTypeHeader)));

  // Step 1-3
  nsTString<char_type> charset, essenceType, essenceSubtype, tempMimeEssence;
  charset.SetIsVoid(true);
  essenceType.SetIsVoid(true);
  essenceSubtype.SetIsVoid(true);

  // Step 3
  RefPtr<TMimeType<char_type>> mimeType;
  RefPtr<TMimeType<char_type>> tempMimeType;

  // Steps 4,6
  GetDecodeAndSplitHeaderValue<char_type>(
      aContentTypeHeader,
      [&](const nsTDependentSubstring<char_type>& nextValue) {
        // Step 6.1
        VERBOSE_LOG(("  ExtractMimeType 6.1 checking %s", LOGSTR(nextValue)));
        tempMimeType = TMimeType<char_type>::Parse(nextValue);

        // Step 6.2
        if (!tempMimeType) {
          VERBOSE_LOG(("  ExtractMimeType 6.2 skipping, parse failed"));
          return;
        }

        if (tempMimeType->mType.Length() == 1 &&
            tempMimeType->mType[0] == '*' &&
            tempMimeType->mSubtype.Length() == 1 &&
            tempMimeType->mSubtype[0] == '*') {
          VERBOSE_LOG(("  ExtractMimeType 6.2 skipping, is */*"));
          return;
        }

        // Step 6.3
        mimeType = std::move(tempMimeType);

        // Step 6.4
        static char_type kCHARSET[] = {'c', 'h', 'a', 'r', 's', 'e', 't'};
        static nsTDependentSubstring<char_type> kCharset(kCHARSET, 7);

        if (!mimeType->mType.Equals(essenceType) ||
            !mimeType->mSubtype.Equals(essenceSubtype)) {
          // Steps 6.4.1 and 6.4.2
          if (mimeType->HasParameter(kCharset)) {
            mimeType->GetParameterValue(kCharset, charset);
          } else {
            charset.SetIsVoid(true);
          }

          // Step 6.4.3
          essenceType = mimeType->mType;
          essenceSubtype = mimeType->mSubtype;

          VERBOSE_LOG(
              ("  ExtractMimeType 6.4 found essence=%s/%s and charset=%s",
               LOGSTR(essenceType), LOGSTR(essenceSubtype), LOGSTR(charset)));

          // Step 6.5
        } else if (!charset.IsVoid() && !mimeType->HasParameter(kCharset)) {
          VERBOSE_LOG(
              ("  ExtractMimeType 6.5 set charset=%s", LOGSTR(charset)));
          mimeType->SetParameterValue(kCharset, charset);
        }
      });

  if (mimeType) {
    DEBUG_LOG(("ExtractMimeType(%s) got %s", LOGSTR(aContentTypeHeader),
               LOGSTR(mimeType)));
  } else {
    DEBUG_LOG(("ExtractMimeType(%s) failed", LOGSTR(aContentTypeHeader)));
  }

  // Steps 7 and 8 (failure is returning null)
  return mimeType;
}

template <typename char_type>
/* static */ nsTArray<nsTDependentSubstring<char_type>>
TMimeType<char_type>::SplitMimetype(const nsTSubstring<char_type>& aMimeType) {
  nsTArray<nsTDependentSubstring<char_type>> mimeTypeParts;
  bool inQuotes = false;
  size_t start = 0;

  for (size_t i = 0; i < aMimeType.Length(); i++) {
    char_type c = aMimeType[i];

    if (c == '\"' && (i == 0 || aMimeType[i - 1] != '\\')) {
      inQuotes = !inQuotes;
    } else if (c == ',' && !inQuotes) {
      mimeTypeParts.AppendElement(Substring(aMimeType, start, i - start));
      start = i + 1;
    }
  }
  if (start < aMimeType.Length()) {
    mimeTypeParts.AppendElement(Substring(aMimeType, start));
  }
  return mimeTypeParts;
}

template <typename char_type>
/* static */ bool TMimeType<char_type>::Parse(
    const nsTSubstring<char_type>& aMimeType,
    nsTSubstring<char_type>& aOutEssence,
    nsTSubstring<char_type>& aOutCharset) {
  static char_type kCHARSET[] = {'c', 'h', 'a', 'r', 's', 'e', 't'};
  static nsTDependentSubstring<char_type> kCharset(kCHARSET, 7);

  RefPtr<TMimeType<char_type>> parsed;
  nsTAutoString<char_type> prevContentType;
  nsTAutoString<char_type> prevCharset;

  prevContentType.Assign(aOutEssence);
  prevCharset.Assign(aOutCharset);

  nsTArray<nsTDependentSubstring<char_type>> mimeTypeParts =
      SplitMimetype(aMimeType);

  for (auto& mimeTypeString : mimeTypeParts) {
    if (mimeTypeString.EqualsLiteral("error")) {
      continue;
    }

    parsed = Parse(mimeTypeString);

    if (!parsed) {
      aOutEssence.Truncate();
      aOutCharset.Truncate();
      return false;
    }

    parsed->GetEssence(aOutEssence);

    if (aOutEssence.EqualsLiteral("*/*")) {
      aOutEssence.Assign(prevContentType);
      continue;
    }

    bool eq = !prevContentType.IsEmpty() && aOutEssence.Equals(prevContentType);

    if (!eq) {
      prevContentType.Assign(aOutEssence);
    }

    bool typeHasCharset = false;
    if (parsed->GetParameterValue(kCharset, aOutCharset, false, false)) {
      typeHasCharset = true;
    } else if (eq) {
      aOutCharset.Assign(prevCharset);
    }

    if ((!eq && !prevCharset.IsEmpty()) || typeHasCharset) {
      prevCharset.Assign(aOutCharset);
    }
  }

  return true;
}

template <typename char_type>
void TMimeType<char_type>::Serialize(nsTSubstring<char_type>& aOutput) const {
  aOutput.Assign(mType);
  aOutput.Append('/');
  aOutput.Append(mSubtype);
  for (uint32_t i = 0; i < mParameterNames.Length(); i++) {
    auto name = mParameterNames[i];
    aOutput.Append(';');
    aOutput.Append(name);
    aOutput.Append('=');
    GetParameterValue(name, aOutput, true);
  }
}

template <typename char_type>
void TMimeType<char_type>::GetEssence(nsTSubstring<char_type>& aOutput) const {
  aOutput.Assign(mType);
  aOutput.Append('/');
  aOutput.Append(mSubtype);
}

template <typename char_type>
bool TMimeType<char_type>::HasParameter(
    const nsTSubstring<char_type>& aName) const {
  return mParameters.Get(aName, nullptr);
}

template <typename char_type>
bool TMimeType<char_type>::GetParameterValue(
    const nsTSubstring<char_type>& aName, nsTSubstring<char_type>& aOutput,
    bool aAppend, bool aWithQuotes) const {
  if (!aAppend) {
    aOutput.Truncate();
  }

  ParameterValue value;
  if (!mParameters.Get(aName, &value)) {
    return false;
  }

  if (aWithQuotes && (value.mRequiresQuoting || value.IsEmpty())) {
    aOutput.Append('"');
    const char_type* vcur = value.BeginReading();
    const char_type* vend = value.EndReading();
    while (vcur < vend) {
      if (*vcur == '"' || *vcur == '\\') {
        aOutput.Append('\\');
      }
      aOutput.Append(*vcur);
      vcur++;
    }
    aOutput.Append('"');
  } else {
    aOutput.Append(value);
  }

  return true;
}

template <typename char_type>
void TMimeType<char_type>::SetParameterValue(
    const nsTSubstring<char_type>& aName,
    const nsTSubstring<char_type>& aValue) {
  mParameters.WithEntryHandle(aName, [&](auto&& entry) {
    if (!entry) {
      mParameterNames.AppendElement(aName);
    }
    ParameterValue value;
    value.Append(aValue);
    entry.InsertOrUpdate(std::move(value));
  });
}

template const char* CollectHTTPQuotedString(const char* start, const char* end,
                                             nsCString& value,
                                             bool extractValue = false);
template const char16_t* CollectHTTPQuotedString(const char16_t* start,
                                                 const char16_t* end,
                                                 nsString& value,
                                                 bool extractValue = false);
template void GetDecodeAndSplitHeaderValue(
    const nsTSubstring<char>& aHeaderValue,
    const std::function<void(const nsTDependentSubstring<char>& nextValue)>&
        aCallback);
template void GetDecodeAndSplitHeaderValue(
    const nsTSubstring<char16_t>& aHeaderValue,
    const std::function<void(const nsTDependentSubstring<char16_t>& nextValue)>&
        aCallback);

template RefPtr<TMimeType<char16_t>> TMimeType<char16_t>::Parse(
    const nsTSubstring<char16_t>& aMimeType);
template RefPtr<TMimeType<char>> TMimeType<char>::Parse(
    const nsTSubstring<char>& aMimeType);
template RefPtr<TMimeType<char>> TMimeType<char>::ExtractMIMEType(
    const nsTSubstring<char>& aContentTypeHeader);
template RefPtr<TMimeType<char16_t>> TMimeType<char16_t>::ExtractMIMEType(
    const nsTSubstring<char16_t>& aContentTypeHeader);
template bool TMimeType<char16_t>::Parse(
    const nsTSubstring<char16_t>& aMimeType,
    nsTSubstring<char16_t>& aOutEssence, nsTSubstring<char16_t>& aOutCharset);
template bool TMimeType<char>::Parse(const nsTSubstring<char>& aMimeType,
                                     nsTSubstring<char>& aOutEssence,
                                     nsTSubstring<char>& aOutCharset);
template nsTArray<nsTDependentSubstring<char>> TMimeType<char>::SplitMimetype(
    const nsTSubstring<char>& aMimeType);
template void TMimeType<char16_t>::Serialize(
    nsTSubstring<char16_t>& aOutput) const;
template void TMimeType<char>::Serialize(nsTSubstring<char>& aOutput) const;
template void TMimeType<char16_t>::GetEssence(
    nsTSubstring<char16_t>& aOutput) const;
template void TMimeType<char>::GetEssence(nsTSubstring<char>& aOutput) const;
template bool TMimeType<char16_t>::HasParameter(
    const nsTSubstring<char16_t>& aName) const;
template bool TMimeType<char>::HasParameter(
    const nsTSubstring<char>& aName) const;
template bool TMimeType<char16_t>::GetParameterValue(
    const nsTSubstring<char16_t>& aName, nsTSubstring<char16_t>& aOutput,
    bool aAppend, bool aWithQuotes) const;
template bool TMimeType<char>::GetParameterValue(
    const nsTSubstring<char>& aName, nsTSubstring<char>& aOutput, bool aAppend,
    bool aWithQuotes) const;
template void TMimeType<char16_t>::SetParameterValue(
    const nsTSubstring<char16_t>& aName, const nsTSubstring<char16_t>& aValue);
template void TMimeType<char>::SetParameterValue(
    const nsTSubstring<char>& aName, const nsTSubstring<char>& aValue);
