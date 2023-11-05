function GenericPartCreator(type) {
    return str => ({ type, value: str });
}

const DateTimeFormatParts = {
    Weekday: GenericPartCreator("weekday"),
    Era: GenericPartCreator("era"),
    Year: GenericPartCreator("year"),
    YearName: GenericPartCreator("yearName"),
    RelatedYear: GenericPartCreator("relatedYear"),
    Month: GenericPartCreator("month"),
    Day: GenericPartCreator("day"),
    DayPeriod: GenericPartCreator("dayPeriod"),
    Hour: GenericPartCreator("hour"),
    Minute: GenericPartCreator("minute"),
    Second: GenericPartCreator("second"),
    FractionalSecond: GenericPartCreator("fractionalSecond"),
    TimeZoneName: GenericPartCreator("timeZoneName"),
    Unknown: GenericPartCreator("unknown"),
    Literal: GenericPartCreator("literal"),
};

function assertParts(df, x, expected) {
    var parts = df.formatToParts(x);
    assertEq(parts.map(part => part.value).join(""), df.format(x),
             "formatToParts and format must agree");

    var len = parts.length;
    assertEq(len, expected.length, "parts count mismatch");
    for (var i = 0; i < len; i++) {
        assertEq(parts[i].type, expected[i].type, "type mismatch at " + i);
        assertEq(parts[i].value, expected[i].value, "value mismatch at " + i);
    }

    // Formatted parts must be consistent with the resolved options.
    var resolvedOptions = df.resolvedOptions();

    assertEq("dateStyle" in resolvedOptions, false, "dateStyle isn't yet supported here");
    assertEq("timeStyle" in resolvedOptions, false, "timeStyle isn't yet supported here");

    // Every formatted part must be in the resolved options.
    for (var {type} of expected) {
      switch (type) {
        case "weekday":
        case "era":
        case "month":
        case "day":
        case "hour":
        case "minute":
        case "second":
        case "timeZoneName":
          assertEq(type in resolvedOptions, true, JSON.stringify(resolvedOptions));
          break;

        case "year":
        case "yearName":
        case "relatedYear":
          assertEq("year" in resolvedOptions, true);
          break;

        case "dayPeriod":
          assertEq("dayPeriod" in resolvedOptions || resolvedOptions.hour12 === true, true);
          break;

        case "fractionalSecond":
          assertEq("fractionalSecondDigits" in resolvedOptions, true);
          break;

        case "unknown":
        case "literal":
          break;

        default:
          assertEq(true, false, `invalid part: ${type}`);
      }
    }

    function includesType(...types) {
      return parts.some(({type}) => types.includes(type));
    }

    // Every resolved option must be in the formatted parts.
    for (var key of Object.keys(resolvedOptions)) {
      switch (key) {
        case "locale":
        case "calendar":
        case "numberingSystem":
        case "timeZone":
        case "hourCycle":
          // Skip over non-pattern keys.
          break;

        case "hour12":
          if (resolvedOptions.hour12) {
            assertEq(includesType("dayPeriod"), true);
          }
          break;

        case "weekday":
        case "era":
        case "month":
        case "day":
        case "dayPeriod":
        case "hour":
        case "minute":
        case "second":
        case "timeZoneName":
          assertEq(includesType(key), true);
          break;

        case "year":
          assertEq(includesType("year", "yearName", "relatedYear"), true);
          break;

        case "fractionalSecondDigits":
          assertEq(includesType("fractionalSecond"), true);
          break;

        default:
          assertEq(true, false, `invalid key: ${key}`);
      }
    }
}

function assertRangeParts(df, start, end, expected) {
    var parts = df.formatRangeToParts(start, end);
    assertEq(parts.map(part => part.value).join(""), df.formatRange(start, end),
             "formatRangeToParts and formatRange must agree");

    var len = parts.length;
    assertEq(len, expected.length, "parts count mismatch");
    for (var i = 0; i < len; i++) {
        assertEq(parts[i].type, expected[i].type, "type mismatch at " + i);
        assertEq(parts[i].value, expected[i].value, "value mismatch at " + i);
    }
}
