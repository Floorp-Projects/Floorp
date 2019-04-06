#include "MimeType.h"
#include "nsUnicharUtils.h"

namespace {
template <typename Char>
constexpr bool IsHTTPTokenPoint(Char aChar) {
  using UnsignedChar = typename mozilla::detail::MakeUnsignedChar<Char>::Type;
  auto c = static_cast<UnsignedChar>(aChar);
  return c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
         c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
         c == '^' || c == '_' || c == '`' || c == '|' || c == '~' ||
         mozilla::IsAsciiAlphanumeric(c);
}

template <typename Char>
constexpr bool IsHTTPQuotedStringTokenPoint(Char aChar) {
  using UnsignedChar = typename mozilla::detail::MakeUnsignedChar<Char>::Type;
  auto c = static_cast<UnsignedChar>(aChar);
  return c == 0x9 || (c >= ' ' && c <= '~') || mozilla::IsNonAsciiLatin1(c);
}

template <typename Char>
constexpr bool IsHTTPWhitespace(Char aChar) {
  using UnsignedChar = typename mozilla::detail::MakeUnsignedChar<Char>::Type;
  auto c = static_cast<UnsignedChar>(aChar);
  return c == 0x9 || c == 0xA || c == 0xD || c == 0x20;
}
}  // namespace

template <typename char_type>
/* static */ mozilla::UniquePtr<TMimeType<char_type>>
TMimeType<char_type>::Parse(const nsTSubstring<char_type>& aMimeType) {
  // See https://mimesniff.spec.whatwg.org/#parsing-a-mime-type

  // Steps 1-2
  const char_type* pos = aMimeType.BeginReading();
  const char_type* end = aMimeType.EndReading();
  while (pos < end && IsHTTPWhitespace(*pos)) {
    ++pos;
  }
  if (pos == end) {
    return nullptr;
  }
  while (end > pos && IsHTTPWhitespace(*(end - 1))) {
    --end;
  }

  // Steps 3-4
  const char_type* typeStart = pos;
  while (pos < end && *pos != '/') {
    if (!IsHTTPTokenPoint(*pos)) {
      return nullptr;
    }
    ++pos;
  }
  const char_type* typeEnd = pos;
  if (typeStart == typeEnd) {
    return nullptr;
  }

  // Step 5
  if (pos == end) {
    return nullptr;
  }

  // Step 6
  ++pos;

  // Step 7-9
  const char_type* subtypeStart = pos;
  const char_type* subtypeEnd = nullptr;
  while (pos < end && *pos != ';') {
    if (!IsHTTPTokenPoint(*pos)) {
      // If we hit a whitespace, check that the rest of
      // the subtype is whitespace, otherwise fail.
      if (IsHTTPWhitespace(*pos)) {
        subtypeEnd = pos;
        ++pos;
        while (pos < end && *pos != ';') {
          if (!IsHTTPWhitespace(*pos)) {
            return nullptr;
          }
          ++pos;
        }
        break;
      }
      return nullptr;
    }
    ++pos;
  }
  if (subtypeEnd == nullptr) {
    subtypeEnd = pos;
  }
  if (subtypeStart == subtypeEnd) {
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
  mozilla::UniquePtr<TMimeType<char_type>> mimeType(
      mozilla::MakeUnique<TMimeType<char_type>>(type, subtype));

  // Step 11
  while (pos < end) {
    // Step 11.1
    ++pos;

    // Step 11.2
    while (pos < end && IsHTTPWhitespace(*pos)) {
      ++pos;
    }

    // Steps 11.3 and 11.4
    nsTString<char_type> paramName;
    bool paramNameHadInvalidChars = false;
    while (pos < end && *pos != ';' && *pos != '=') {
      if (!IsHTTPTokenPoint(*pos)) {
        paramNameHadInvalidChars = true;
      }
      paramName.Append(ToLowerCaseASCII(*pos));
      ++pos;
    }

    // Step 11.5
    if (pos < end) {
      if (*pos == ';') {
        continue;
      }
      ++pos;
    }

    // Step 11.6
    if (pos == end) {
      break;
    }

    // Step 11.7
    ParameterValue paramValue;
    bool paramValueHadInvalidChars = false;

    // Step 11.8
    if (*pos == '"') {
      // Step 11.8.1
      ++pos;

      // Step 11.8.2
      while (true) {
        // Step 11.8.2.1
        while (pos < end && *pos != '"' && *pos != '\\') {
          if (!IsHTTPQuotedStringTokenPoint(*pos)) {
            paramValueHadInvalidChars = true;
          }
          if (!IsHTTPTokenPoint(*pos)) {
            paramValue.mRequiresQuoting = true;
          }
          paramValue.Append(*pos);
          ++pos;
        }

        // Step 11.8.2.2
        if (pos < end && *pos == '\\') {
          // Step 11.8.2.2.1
          ++pos;

          // Step 11.8.2.2.2
          if (pos < end) {
            if (!IsHTTPQuotedStringTokenPoint(*pos)) {
              paramValueHadInvalidChars = true;
            }
            if (!IsHTTPTokenPoint(*pos)) {
              paramValue.mRequiresQuoting = true;
            }
            paramValue.Append(*pos);
            ++pos;
            continue;
          }

          // Step 11.8.2.2.3
          paramValue.Append('\\');
          paramValue.mRequiresQuoting = true;
        }

        // Step 11.8.2.3
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
             IsHTTPWhitespace(*paramValueLastChar)) {
        --paramValueLastChar;
      }

      // Step 11.9.3
      if (paramValueStart > paramValueLastChar) {
        continue;
      }

      for (const char_type* c = paramValueStart; c <= paramValueLastChar; ++c) {
        if (!IsHTTPQuotedStringTokenPoint(*c)) {
          paramValueHadInvalidChars = true;
        }
        if (!IsHTTPTokenPoint(*c)) {
          paramValue.mRequiresQuoting = true;
        }
        paramValue.Append(*c);
      }
    }

    // Step 11.10
    if (!paramName.IsEmpty() && !paramNameHadInvalidChars &&
        !paramValueHadInvalidChars &&
        !mimeType->mParameters.Get(paramName, &paramValue)) {
      mimeType->mParameters.Put(paramName, paramValue);
      mimeType->mParameterNames.AppendElement(paramName);
    }
  }

  // Step 12
  return mimeType;
}

template <typename char_type>
void TMimeType<char_type>::Serialize(nsTSubstring<char_type>& aOutput) const {
  aOutput.Assign(mType);
  aOutput.AppendLiteral("/");
  aOutput.Append(mSubtype);
  for (uint32_t i = 0; i < mParameterNames.Length(); i++) {
    auto name = mParameterNames[i];
    aOutput.AppendLiteral(";");
    aOutput.Append(name);
    aOutput.AppendLiteral("=");
    GetParameterValue(name, aOutput, true);
  }
}

template <typename char_type>
void TMimeType<char_type>::GetFullType(nsTSubstring<char_type>& aOutput) const {
  aOutput.Assign(mType);
  aOutput.AppendLiteral("/");
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
    bool aAppend) const {
  if (!aAppend) {
    aOutput.Truncate();
  }

  ParameterValue value;
  if (!mParameters.Get(aName, &value)) {
    return false;
  }

  if (value.mRequiresQuoting || value.IsEmpty()) {
    aOutput.AppendLiteral("\"");
    const char_type* vcur = value.BeginReading();
    const char_type* vend = value.EndReading();
    while (vcur < vend) {
      if (*vcur == '"' || *vcur == '\\') {
        aOutput.AppendLiteral("\\");
      }
      aOutput.Append(*vcur);
      vcur++;
    }
    aOutput.AppendLiteral("\"");
  } else {
    aOutput.Append(value);
  }

  return true;
}

template <typename char_type>
void TMimeType<char_type>::SetParameterValue(
    const nsTSubstring<char_type>& aName,
    const nsTSubstring<char_type>& aValue) {
  if (!mParameters.Get(aName, nullptr)) {
    mParameterNames.AppendElement(aName);
  }
  ParameterValue value;
  value.Append(aValue);
  mParameters.Put(aName, value);
}

template mozilla::UniquePtr<TMimeType<char16_t>> TMimeType<char16_t>::Parse(
    const nsTSubstring<char16_t>& aMimeType);
template mozilla::UniquePtr<TMimeType<char>> TMimeType<char>::Parse(
    const nsTSubstring<char>& aMimeType);
template void TMimeType<char16_t>::Serialize(
    nsTSubstring<char16_t>& aOutput) const;
template void TMimeType<char>::Serialize(nsTSubstring<char>& aOutput) const;
template void TMimeType<char16_t>::GetFullType(
    nsTSubstring<char16_t>& aOutput) const;
template void TMimeType<char>::GetFullType(nsTSubstring<char>& aOutput) const;
template bool TMimeType<char16_t>::HasParameter(
    const nsTSubstring<char16_t>& aName) const;
template bool TMimeType<char>::HasParameter(
    const nsTSubstring<char>& aName) const;
template bool TMimeType<char16_t>::GetParameterValue(
    const nsTSubstring<char16_t>& aName, nsTSubstring<char16_t>& aOutput,
    bool aAppend) const;
template bool TMimeType<char>::GetParameterValue(
    const nsTSubstring<char>& aName, nsTSubstring<char>& aOutput,
    bool aAppend) const;
template void TMimeType<char16_t>::SetParameterValue(
    const nsTSubstring<char16_t>& aName, const nsTSubstring<char16_t>& aValue);
template void TMimeType<char>::SetParameterValue(
    const nsTSubstring<char>& aName, const nsTSubstring<char>& aValue);
