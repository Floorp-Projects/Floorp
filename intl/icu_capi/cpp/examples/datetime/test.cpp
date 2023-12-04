// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XGregorianDateFormatter.hpp"
#include "../../include/ICU4XGregorianDateTimeFormatter.hpp"
#include "../../include/ICU4XDateTimeFormatter.hpp"
#include "../../include/ICU4XTimeFormatter.hpp"
#include "../../include/ICU4XDataStruct.hpp"
#include "../../include/ICU4XLogger.hpp"
#include "../../include/ICU4XCustomTimeZone.hpp"
#include "../../include/ICU4XIanaToBcp47Mapper.hpp"
#include "../../include/ICU4XBcp47ToIanaMapper.hpp"
#include "../../include/ICU4XGregorianZonedDateTimeFormatter.hpp"
#include "../../include/ICU4XZonedDateTimeFormatter.hpp"

#include <atomic>
#include <iostream>
#include <array>

int main() {
    ICU4XLogger::init_simple_logger();
    ICU4XLocale locale = ICU4XLocale::create_from_string("es").ok().value();
    std::cout << "Running test for locale " << locale.to_string().ok().value() << std::endl;
    ICU4XDataProvider dp = ICU4XDataProvider::create_compiled();

    ICU4XIsoDateTime date = ICU4XIsoDateTime::create(2022, 07, 11, 13, 06, 42, 0).ok().value();

    ICU4XTimeFormatter tf = ICU4XTimeFormatter::create_with_length(dp, locale, ICU4XTimeLength::Short).ok().value();
    std::string out = tf.format_iso_datetime(date).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "13:06") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    ICU4XGregorianDateFormatter df = ICU4XGregorianDateFormatter::create_with_length(dp, locale, ICU4XDateLength::Full).ok().value();
    out = df.format_iso_datetime(date).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "lunes, 11 de julio de 2022") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    ICU4XGregorianDateTimeFormatter dtf = ICU4XGregorianDateTimeFormatter::create_with_lengths(dp, locale, ICU4XDateLength::Medium, ICU4XTimeLength::Short).ok().value();
    out = dtf.format_iso_datetime(date).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "11 jul 2022, 13:06") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    locale = ICU4XLocale::create_from_string("en-u-ca-japanese").ok().value();
    ICU4XCalendar cal = ICU4XCalendar::create_for_locale(dp, locale).ok().value();
    ICU4XDateTime any_date = ICU4XDateTime::create_from_iso_in_calendar(2020, 10, 5, 13, 33, 15, 0, cal).ok().value();
    ICU4XDateTimeFormatter any_dtf = ICU4XDateTimeFormatter::create_with_lengths(dp, locale, ICU4XDateLength::Medium, ICU4XTimeLength::Short).ok().value();
    out = any_dtf.format_datetime(any_date).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "Oct 5, 2 Reiwa, 1:33\u202fPM") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    ICU4XCustomTimeZone time_zone = ICU4XCustomTimeZone::create_from_string("-06:00").ok().value();
    int32_t offset = time_zone.gmt_offset_seconds().ok().value();
    if (offset != -21600) {
        std::cout << "GMT offset doesn't parse" << std::endl;
        return 1;
    }
    ICU4XMetazoneCalculator mzcalc = ICU4XMetazoneCalculator::create(dp).ok().value();
    ICU4XIanaToBcp47Mapper mapper = ICU4XIanaToBcp47Mapper::create(dp).ok().value();
    time_zone.try_set_iana_time_zone_id(mapper, "america/chicago").ok().value();
    std::string time_zone_id_return = time_zone.time_zone_id().ok().value();
    if (time_zone_id_return != "uschi") {
        std::cout << "Time zone ID does not roundtrip: " << time_zone_id_return << std::endl;
        return 1;
    }
    ICU4XBcp47ToIanaMapper reverse_mapper = ICU4XBcp47ToIanaMapper::create(dp).ok().value();
    std::string recovered_iana_id = reverse_mapper.get("uschi").ok().value();
    if (recovered_iana_id != "America/Chicago") {
        std::cout << "Time zone ID does not canonicalize to IANA: " << recovered_iana_id << std::endl;
        return 1;
    }
    ICU4XIsoDateTime local_datetime = ICU4XIsoDateTime::create(2022, 8, 25, 0, 0, 0, 0).ok().value();
    time_zone.maybe_calculate_metazone(mzcalc, local_datetime);
    std::string metazone_id_return = time_zone.metazone_id().ok().value();
    if (metazone_id_return != "amce") {
        std::cout << "Metazone ID not calculated correctly; got " << metazone_id_return << std::endl;
        return 1;
    }
    // Note: The daylight time switch should normally come from TZDB calculations.
    time_zone.set_daylight_time();
    std::string zone_variant_return = time_zone.zone_variant().ok().value();
    if (zone_variant_return != "dt") {
        std::cout << "Zone variant not calculated correctly; got " << zone_variant_return << std::endl;
        return 1;
    }

    ICU4XGregorianZonedDateTimeFormatter gzdtf = ICU4XGregorianZonedDateTimeFormatter::create_with_lengths(dp, locale, ICU4XDateLength::Full, ICU4XTimeLength::Full).ok().value();
    out = gzdtf.format_iso_datetime_with_custom_time_zone(date, time_zone).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "Monday, July 11, 2022, 1:06:42\u202fPM Central Daylight Time") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    ICU4XZonedDateTimeFormatter zdtf = ICU4XZonedDateTimeFormatter::create_with_lengths(dp, locale, ICU4XDateLength::Full, ICU4XTimeLength::Full).ok().value();
    out = zdtf.format_datetime_with_custom_time_zone(any_date, time_zone).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "Monday, October 5, 2 Reiwa, 1:33:15\u202fPM Central Daylight Time") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    return 0;
}
