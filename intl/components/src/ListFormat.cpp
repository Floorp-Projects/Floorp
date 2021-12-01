/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/ListFormat.h"

#include "ScopedICUObject.h"

namespace mozilla::intl {

/*static*/ Result<UniquePtr<ListFormat>, ICUError> ListFormat::TryCreate(
    mozilla::Span<const char> aLocale, const Options& aOptions) {
  UListFormatterType utype = ToUListFormatterType(aOptions.mType);
  UListFormatterWidth uwidth = ToUListFormatterWidth(aOptions.mStyle);

  UErrorCode status = U_ZERO_ERROR;
  UListFormatter* fmt =
      ulistfmt_openForType(IcuLocale(aLocale), utype, uwidth, &status);
  if (U_FAILURE(status)) {
    return Err(ICUError::InternalError);
  }

  return UniquePtr<ListFormat>(new ListFormat(fmt));
}

ListFormat::~ListFormat() {
  if (mListFormatter) {
    ulistfmt_close(mListFormatter.GetMut());
  }
}

/* static */ UListFormatterType ListFormat::ToUListFormatterType(Type type) {
  switch (type) {
    case Type::Conjunction:
      return ULISTFMT_TYPE_AND;
    case Type::Disjunction:
      return ULISTFMT_TYPE_OR;
    case Type::Unit:
      return ULISTFMT_TYPE_UNITS;
  }
  MOZ_ASSERT_UNREACHABLE();
  return ULISTFMT_TYPE_AND;
}

/* static */ UListFormatterWidth ListFormat::ToUListFormatterWidth(
    Style style) {
  switch (style) {
    case Style::Long:
      return ULISTFMT_WIDTH_WIDE;
    case Style::Short:
      return ULISTFMT_WIDTH_SHORT;
    case Style::Narrow:
      return ULISTFMT_WIDTH_NARROW;
  }
  MOZ_ASSERT_UNREACHABLE();
  return ULISTFMT_WIDTH_WIDE;
}

ICUResult ListFormat::FormattedToParts(const UFormattedValue* formattedValue,
                                       size_t formattedSize,
                                       PartVector& parts) {
  size_t lastEndIndex = 0;

  auto AppendPart = [&](PartType type, size_t endIndex) {
    if (!parts.emplaceBack(type, endIndex)) {
      return false;
    }

    lastEndIndex = endIndex;
    return true;
  };

  UErrorCode status = U_ZERO_ERROR;
  UConstrainedFieldPosition* fpos = ucfpos_open(&status);
  if (U_FAILURE(status)) {
    return Err(ICUError::InternalError);
  }
  ScopedICUObject<UConstrainedFieldPosition, ucfpos_close> toCloseFpos(fpos);

  // We're only interested in ULISTFMT_ELEMENT_FIELD fields.
  ucfpos_constrainField(fpos, UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD,
                        &status);
  if (U_FAILURE(status)) {
    return Err(ICUError::InternalError);
  }

  while (true) {
    bool hasMore = ufmtval_nextPosition(formattedValue, fpos, &status);
    if (U_FAILURE(status)) {
      return Err(ICUError::InternalError);
    }
    if (!hasMore) {
      break;
    }

    int32_t beginIndexInt, endIndexInt;
    ucfpos_getIndexes(fpos, &beginIndexInt, &endIndexInt, &status);
    if (U_FAILURE(status)) {
      return Err(ICUError::InternalError);
    }

    MOZ_ASSERT(beginIndexInt <= endIndexInt,
               "field iterator returning invalid range");

    size_t beginIndex = AssertedCast<size_t>(beginIndexInt);
    size_t endIndex = AssertedCast<size_t>(endIndexInt);

    // Indices are guaranteed to be returned in order (from left to right).
    MOZ_ASSERT(lastEndIndex <= beginIndex,
               "field iteration didn't return fields in order start to "
               "finish as expected");

    if (lastEndIndex < beginIndex) {
      if (!AppendPart(PartType::Literal, beginIndex)) {
        return Err(ICUError::InternalError);
      }
    }

    if (!AppendPart(PartType::Element, endIndex)) {
      return Err(ICUError::InternalError);
    }
  }

  // Append any final literal.
  if (lastEndIndex < formattedSize) {
    if (!AppendPart(PartType::Literal, formattedSize)) {
      return Err(ICUError::InternalError);
    }
  }

  return Ok();
}
}  // namespace mozilla::intl
