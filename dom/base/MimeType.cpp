#include "MimeType.h"
#include "nsUnicharUtils.h"

namespace {
  static inline bool IsHTTPTokenPoint(const char16_t c) {
    return c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
           c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
           c == '^' || c == '_' || c == '`' || c == '|' || c == '~' ||
           mozilla::IsAsciiAlphanumeric(c);
  }

  static inline bool IsHTTPQuotedStringTokenPoint(const char16_t c) {
    return c == 0x9 || (c >= ' ' && c <= '~') || (c >= 0x80 && c <= 0xFF);
  }
}

/* static */ mozilla::UniquePtr<MimeType>
MimeType::Parse(const nsAString& aMimeType)
{
  // See https://mimesniff.spec.whatwg.org/#parsing-a-mime-type

  // Steps 1-2
  const char16_t* pos = aMimeType.BeginReading();
  const char16_t* end = aMimeType.EndReading();
  while (pos < end && mozilla::IsAsciiWhitespace(*pos)) {
    ++pos;
  }
  if (pos == end) {
    return nullptr;
  }
  while (end > pos && mozilla::IsAsciiWhitespace(*(end - 1))) {
    --end;
  }

  // Steps 3-4
  const char16_t* typeStart = pos;
  while (pos < end && *pos != '/') {
    if (!IsHTTPTokenPoint(*pos)) {
      return nullptr;
    }
    ++pos;
  }
  const char16_t* typeEnd = pos;
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
  const char16_t* subtypeStart = pos;
  const char16_t* subtypeEnd = nullptr;
  while (pos < end && *pos != ';') {
    if (!IsHTTPTokenPoint(*pos)) {
      // If we hit a whitespace, check that the rest of
      // the subtype is whitespace, otherwise fail.
      if (mozilla::IsAsciiWhitespace(*pos)) {
        subtypeEnd = pos;
        ++pos;
        while (pos < end && *pos != ';') {
          if (!mozilla::IsAsciiWhitespace(*pos)) {
            return nullptr;
          }
          ++pos;
        }
        break;
      } else {
        return nullptr;
      }
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
  nsString type;
  nsString subtype;
  for (const char16_t* c = typeStart; c < typeEnd; ++c) {
    type.Append(ToLowerCaseASCII(*c));
  }
  for (const char16_t* c = subtypeStart; c < subtypeEnd; ++c) {
    subtype.Append(ToLowerCaseASCII(*c));
  }
  mozilla::UniquePtr<MimeType> mimeType(mozilla::MakeUnique<MimeType>(type, subtype));

  // Step 11
  while (pos < end) {
    // Step 11.1
    ++pos;

    // Step 11.2
    while (pos < end && mozilla::IsAsciiWhitespace(*pos)) {
      ++pos;
    }

    // Steps 11.3 and 11.4
    nsString paramName;
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
    ParameterValue paramValue;
    bool paramValueHadInvalidChars = false;

    // Step 11.7
    if (pos < end) {

      // Step 11.7.1
      if (*pos == '"') {

        // Step 11.7.1.1
        ++pos;

        // Step 11.7.1.2
        while (true) {

          // Step 11.7.1.2.1
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

          // Step 11.7.1.2.2
          if (pos < end && *pos == '\\') {
            // Step 11.7.1.2.2.1
            ++pos;

            // Step 11.7.1.2.2.2
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

            // Step 11.7.1.2.2.3
            paramValue.Append('\\');
            paramValue.mRequiresQuoting = true;
            break;
          } else {
            // Step 11.7.1.2.3
            break;
          }
        }

        // Step 11.7.1.3
        while (pos < end && *pos != ';') {
          ++pos;
        }

      } else {

        const char16_t* paramValueStart = pos;

        // Step 11.7.2.1
        while (pos < end && *pos != ';') {
          if (!IsHTTPQuotedStringTokenPoint(*pos)) {
            paramValueHadInvalidChars = true;
          }
          if (!IsHTTPTokenPoint(*pos)) {
            paramValue.mRequiresQuoting = true;
          }
          ++pos;
        }

        // Step 11.7.2.2
        const char16_t* paramValueEnd = pos - 1;
        while (paramValueEnd >= paramValueStart &&
               mozilla::IsAsciiWhitespace(*paramValueEnd)) {
          --paramValueEnd;
        }

        for (const char16_t* c = paramValueStart; c <= paramValueEnd; ++c) {
          paramValue.Append(*c);
        }
      }

      // Step 11.8
      if (!paramName.IsEmpty() && !paramValue.IsEmpty() &&
          !paramNameHadInvalidChars && !paramValueHadInvalidChars &&
          !mimeType->mParameters.Get(paramName, &paramValue)) {
        mimeType->mParameters.Put(paramName, paramValue);
        mimeType->mParameterNames.AppendElement(paramName);
      }
    }
  }

  return mimeType;
}

void
MimeType::Serialize(nsAString& aOutput) const
{
  aOutput.Assign(mType);
  aOutput.AppendLiteral("/");
  aOutput.Append(mSubtype);
  for (uint32_t i = 0; i < mParameterNames.Length(); i++) {
    auto name = mParameterNames[i];
    ParameterValue value;
    mParameters.Get(name, &value);
    aOutput.AppendLiteral(";");
    aOutput.Append(name);
    aOutput.AppendLiteral("=");
    if (value.mRequiresQuoting) {
      aOutput.AppendLiteral("\"");
      const char16_t* vcur = value.BeginReading();
      const char16_t* vend = value.EndReading();
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
  }
}
