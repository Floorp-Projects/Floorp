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
}
