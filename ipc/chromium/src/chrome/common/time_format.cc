// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/time_format.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/time_format.h"
#include "grit/generated_resources.h"
#include "unicode/datefmt.h"
#include "unicode/locid.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "unicode/smpdtfmt.h"

using base::Time;
using base::TimeDelta;

namespace {

static const char kFallbackFormatSuffixShort[] = "}";
static const char kFallbackFormatSuffixLeft[] = " left}";
static const char kFallbackFormatSuffixAgo[] = " ago}";

static const int kInvalidMsgId = -1;

// Contains message IDs for various time units and pluralities.
struct MessageIDs {
  // There are 4 different time units and 6 different pluralities.
  int ids[4][6];
};

// Message IDs for different time formats.
static const MessageIDs kTimeShortMessageIDs = { {
  {
    IDS_TIME_SECS_DEFAULT, IDS_TIME_SEC_SINGULAR, IDS_TIME_SECS_ZERO,
    IDS_TIME_SECS_TWO, IDS_TIME_SECS_FEW, IDS_TIME_SECS_MANY
  },
  {
    IDS_TIME_MINS_DEFAULT, IDS_TIME_MIN_SINGULAR, kInvalidMsgId,
    IDS_TIME_MINS_TWO, IDS_TIME_MINS_FEW, IDS_TIME_MINS_MANY
  },
  {
    IDS_TIME_HOURS_DEFAULT, IDS_TIME_HOUR_SINGULAR, kInvalidMsgId,
    IDS_TIME_HOURS_TWO, IDS_TIME_HOURS_FEW, IDS_TIME_HOURS_MANY
  },
  {
    IDS_TIME_DAYS_DEFAULT, IDS_TIME_DAY_SINGULAR, kInvalidMsgId,
    IDS_TIME_DAYS_TWO, IDS_TIME_DAYS_FEW, IDS_TIME_DAYS_MANY
  }
} };

static const MessageIDs kTimeRemainingMessageIDs = { {
  {
    IDS_TIME_REMAINING_SECS_DEFAULT, IDS_TIME_REMAINING_SEC_SINGULAR,
    IDS_TIME_REMAINING_SECS_ZERO, IDS_TIME_REMAINING_SECS_TWO,
    IDS_TIME_REMAINING_SECS_FEW, IDS_TIME_REMAINING_SECS_MANY
  },
  {
    IDS_TIME_REMAINING_MINS_DEFAULT, IDS_TIME_REMAINING_MIN_SINGULAR,
    kInvalidMsgId, IDS_TIME_REMAINING_MINS_TWO,
    IDS_TIME_REMAINING_MINS_FEW, IDS_TIME_REMAINING_MINS_MANY
  },
  {
    IDS_TIME_REMAINING_HOURS_DEFAULT, IDS_TIME_REMAINING_HOUR_SINGULAR,
    kInvalidMsgId, IDS_TIME_REMAINING_HOURS_TWO,
    IDS_TIME_REMAINING_HOURS_FEW, IDS_TIME_REMAINING_HOURS_MANY
  },
  {
    IDS_TIME_REMAINING_DAYS_DEFAULT, IDS_TIME_REMAINING_DAY_SINGULAR,
    kInvalidMsgId, IDS_TIME_REMAINING_DAYS_TWO,
    IDS_TIME_REMAINING_DAYS_FEW, IDS_TIME_REMAINING_DAYS_MANY
  }
} };

static const MessageIDs kTimeElapsedMessageIDs = { {
  {
    IDS_TIME_ELAPSED_SECS_DEFAULT, IDS_TIME_ELAPSED_SEC_SINGULAR,
    IDS_TIME_ELAPSED_SECS_ZERO, IDS_TIME_ELAPSED_SECS_TWO,
    IDS_TIME_ELAPSED_SECS_FEW, IDS_TIME_ELAPSED_SECS_MANY
  },
  {
    IDS_TIME_ELAPSED_MINS_DEFAULT, IDS_TIME_ELAPSED_MIN_SINGULAR,
    kInvalidMsgId, IDS_TIME_ELAPSED_MINS_TWO,
    IDS_TIME_ELAPSED_MINS_FEW, IDS_TIME_ELAPSED_MINS_MANY
  },
  {
    IDS_TIME_ELAPSED_HOURS_DEFAULT, IDS_TIME_ELAPSED_HOUR_SINGULAR,
    kInvalidMsgId, IDS_TIME_ELAPSED_HOURS_TWO,
    IDS_TIME_ELAPSED_HOURS_FEW, IDS_TIME_ELAPSED_HOURS_MANY
  },
  {
    IDS_TIME_ELAPSED_DAYS_DEFAULT, IDS_TIME_ELAPSED_DAY_SINGULAR,
    kInvalidMsgId, IDS_TIME_ELAPSED_DAYS_TWO,
    IDS_TIME_ELAPSED_DAYS_FEW, IDS_TIME_ELAPSED_DAYS_MANY
  }
} };

// Different format types.
enum FormatType {
  FORMAT_SHORT,
  FORMAT_REMAINING,
  FORMAT_ELAPSED,
};

}  // namespace

class TimeFormatter {
  public:
    const std::vector<PluralFormat*>& formatter(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return short_formatter_;
        case FORMAT_REMAINING:
          return time_left_formatter_;
        case FORMAT_ELAPSED:
          return time_elapsed_formatter_;
        default:
          NOTREACHED();
          return short_formatter_;
      }
    }
  private:
    static const MessageIDs& GetMessageIDs(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kTimeShortMessageIDs;
        case FORMAT_REMAINING:
          return kTimeRemainingMessageIDs;
        case FORMAT_ELAPSED:
          return kTimeElapsedMessageIDs;
        default:
          NOTREACHED();
          return kTimeShortMessageIDs;
      }
    }

    static const char* GetFallbackFormatSuffix(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kFallbackFormatSuffixShort;
        case FORMAT_REMAINING:
          return kFallbackFormatSuffixLeft;
        case FORMAT_ELAPSED:
          return kFallbackFormatSuffixAgo;
        default:
          NOTREACHED();
          return kFallbackFormatSuffixShort;
      }
    }

    TimeFormatter() {
      BuildFormats(FORMAT_SHORT, &short_formatter_);
      BuildFormats(FORMAT_REMAINING, &time_left_formatter_);
      BuildFormats(FORMAT_ELAPSED, &time_elapsed_formatter_);
    }
    ~TimeFormatter() {
      STLDeleteContainerPointers(short_formatter_.begin(),
                                 short_formatter_.end());
      STLDeleteContainerPointers(time_left_formatter_.begin(),
                                 time_left_formatter_.end());
      STLDeleteContainerPointers(time_elapsed_formatter_.begin(),
                                 time_elapsed_formatter_.end());
    }
    friend class Singleton<TimeFormatter>;
    friend struct DefaultSingletonTraits<TimeFormatter>;

    std::vector<PluralFormat*> short_formatter_;
    std::vector<PluralFormat*> time_left_formatter_;
    std::vector<PluralFormat*> time_elapsed_formatter_;
    static void BuildFormats(FormatType format_type,
                             std::vector<PluralFormat*>* time_formats);
    static PluralFormat* createFallbackFormat(const PluralRules& rules,
                                              int index,
                                              FormatType format_type);

    DISALLOW_EVIL_CONSTRUCTORS(TimeFormatter);
};

void TimeFormatter::BuildFormats(FormatType format_type,
                                 std::vector<PluralFormat*>* time_formats) {
  const static UnicodeString kKeywords[] = {
    UNICODE_STRING_SIMPLE("other"), UNICODE_STRING_SIMPLE("one"),
    UNICODE_STRING_SIMPLE("zero"), UNICODE_STRING_SIMPLE("two"),
    UNICODE_STRING_SIMPLE("few"), UNICODE_STRING_SIMPLE("many")
  };
  UErrorCode err = U_ZERO_ERROR;
  scoped_ptr<PluralRules> rules(
      PluralRules::forLocale(Locale::getDefault(), err));
  if (U_FAILURE(err)) {
    err = U_ZERO_ERROR;
    UnicodeString fallback_rules("one: n is 1", -1, US_INV);
    rules.reset(PluralRules::createRules(fallback_rules, err));
    DCHECK(U_SUCCESS(err));
  }

  const MessageIDs& message_ids = GetMessageIDs(format_type);

  for (int i = 0; i < 4; ++i) {
    UnicodeString pattern;
    for (size_t j = 0; j < arraysize(kKeywords); ++j) {
      int msg_id = message_ids.ids[i][j];
      if (msg_id == kInvalidMsgId) continue;
      std::string sub_pattern = WideToUTF8(l10n_util::GetString(msg_id));
      // NA means this keyword is not used in the current locale.
      // Even if a translator translated for this keyword, we do not
      // use it unless it's 'other' (j=0) or it's defined in the rules
      // for the current locale. Special-casing of 'other' will be removed
      // once ICU's isKeyword is fixed to return true for isKeyword('other').
      if (sub_pattern.compare("NA") != 0 &&
          (j == 0 || rules->isKeyword(kKeywords[j]))) {
          pattern += kKeywords[j];
          pattern += UNICODE_STRING_SIMPLE("{");
          pattern += UnicodeString(sub_pattern.c_str(), "UTF-8");
          pattern += UNICODE_STRING_SIMPLE("}");
      }
    }
    PluralFormat* format = new PluralFormat(*rules, pattern, err);
    if (U_SUCCESS(err)) {
      time_formats->push_back(format);
    } else {
      delete format;
      time_formats->push_back(createFallbackFormat(*rules, i, format_type));
      // Reset it so that next ICU call can proceed.
      err = U_ZERO_ERROR;
    }
  }
}

// Create a hard-coded fallback plural format. This will never be called
// unless translators make a mistake.
PluralFormat* TimeFormatter::createFallbackFormat(const PluralRules& rules,
                                                  int index,
                                                  FormatType format_type) {
  const static UnicodeString kUnits[4][2] = {
    { UNICODE_STRING_SIMPLE("sec"), UNICODE_STRING_SIMPLE("secs") },
    { UNICODE_STRING_SIMPLE("min"), UNICODE_STRING_SIMPLE("mins") },
    { UNICODE_STRING_SIMPLE("hour"), UNICODE_STRING_SIMPLE("hours") },
    { UNICODE_STRING_SIMPLE("day"), UNICODE_STRING_SIMPLE("days") }
  };
  UnicodeString suffix(GetFallbackFormatSuffix(format_type), -1, US_INV);
  UnicodeString pattern;
  if (rules.isKeyword(UNICODE_STRING_SIMPLE("one"))) {
    pattern += UNICODE_STRING_SIMPLE("one{# ") + kUnits[index][0] + suffix;
  }
  pattern += UNICODE_STRING_SIMPLE(" other{# ") + kUnits[index][1] + suffix;
  UErrorCode err = U_ZERO_ERROR;
  PluralFormat* format = new PluralFormat(rules, pattern, err);
  DCHECK(U_SUCCESS(err));
  return format;
}

Singleton<TimeFormatter> time_formatter;

static std::wstring FormatTimeImpl(const TimeDelta& delta,
                                   FormatType format_type) {
  if (delta.ToInternalValue() < 0) {
    NOTREACHED() << "Negative duration";
    return std::wstring();
  }

  int number;

  const std::vector<PluralFormat*>& formatters =
    time_formatter->formatter(format_type);

  UErrorCode error = U_ZERO_ERROR;
  UnicodeString time_string;
  // Less than a minute gets "X seconds left"
  if (delta.ToInternalValue() < Time::kMicrosecondsPerMinute) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerSecond);
    time_string = formatters[0]->format(number, error);

  // Less than 1 hour gets "X minutes left".
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerHour) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerMinute);
    time_string = formatters[1]->format(number, error);

  // Less than 1 day remaining gets "X hours left"
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerDay) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerHour);
    time_string = formatters[2]->format(number, error);

  // Anything bigger gets "X days left"
  } else {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerDay);
    time_string = formatters[3]->format(number, error);
  }

  // With the fallback added, this should never fail.
  DCHECK(U_SUCCESS(error));
  int capacity = time_string.length() + 1;
  string16 result_utf16;
  time_string.extract(static_cast<UChar*>(
                      WriteInto(&result_utf16, capacity)),
                      capacity, error);
  DCHECK(U_SUCCESS(error));
  return UTF16ToWide(result_utf16);
}

// static
std::wstring TimeFormat::TimeElapsed(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_ELAPSED);
}

// static
std::wstring TimeFormat::TimeRemaining(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_REMAINING);
}

// static
std::wstring TimeFormat::TimeRemainingShort(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_SHORT);
}

// static
std::wstring TimeFormat::RelativeDate(
    const Time& time,
    const Time* optional_midnight_today) {
  Time midnight_today = optional_midnight_today ? *optional_midnight_today :
      Time::Now().LocalMidnight();

  // Filter out "today" and "yesterday"
  if (time >= midnight_today)
    return l10n_util::GetString(IDS_PAST_TIME_TODAY);
  else if (time >= midnight_today -
                   TimeDelta::FromMicroseconds(Time::kMicrosecondsPerDay))
    return l10n_util::GetString(IDS_PAST_TIME_YESTERDAY);

  return std::wstring();
}
