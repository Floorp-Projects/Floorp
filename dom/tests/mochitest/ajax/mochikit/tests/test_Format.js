if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.Format'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.Format'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_Format = function (t) {
    t.is( truncToFixed(0.1234, 3), "0.123", "truncToFixed truncate" );
    t.is( truncToFixed(0.12, 3), "0.120", "truncToFixed trailing zeros" );
    t.is( truncToFixed(0.15, 1), "0.1", "truncToFixed no round" );
    t.is( truncToFixed(0.15, 0), "0", "truncToFixed zero (edge case)" );

    t.is( roundToFixed(0.1234, 3), "0.123", "roundToFixed truncate" );
    t.is( roundToFixed(0.12, 3), "0.120", "roundToFixed trailing zeros" );
    t.is( roundToFixed(0.15, 1), "0.2", "roundToFixed round" );
    t.is( roundToFixed(0.15, 0), "0", "roundToFixed zero (edge case)" );

    t.is( twoDigitFloat(-0.1234), "-0.12", "twoDigitFloat -0.1234 correct");
    t.is( twoDigitFloat(-0.1), "-0.1", "twoDigitFloat -0.1 correct");
    t.is( twoDigitFloat(-0), "0", "twoDigitFloat -0 correct");
    t.is( twoDigitFloat(0), "0", "twoDigitFloat 0 correct");
    t.is( twoDigitFloat(1), "1", "twoDigitFloat 1 correct");
    t.is( twoDigitFloat(1.0), "1", "twoDigitFloat 1.0 correct");
    t.is( twoDigitFloat(1.2), "1.2", "twoDigitFloat 1.2 correct");
    t.is( twoDigitFloat(1.234), "1.23", "twoDigitFloat 1.234 correct");
    t.is( percentFormat(123), "12300%", "percentFormat 123 correct");
    t.is( percentFormat(1.23), "123%", "percentFormat 123 correct");
    t.is( twoDigitAverage(1, 0), "0", "twoDigitAverage dbz correct");
    t.is( twoDigitAverage(1, 1), "1", "twoDigitAverage 1 correct");
    t.is( twoDigitAverage(1, 10), "0.1", "twoDigitAverage .1 correct");
    function reprIs(a, b) {
        arguments[0] = repr(a);
        arguments[1] = repr(b);
        t.is.apply(this, arguments);
    }
    reprIs( lstrip("\r\t\n foo \n\t\r"), "foo \n\t\r", "lstrip whitespace chars" );
    reprIs( rstrip("\r\t\n foo \n\t\r"), "\r\t\n foo", "rstrip whitespace chars" );
    reprIs( strip("\r\t\n foo \n\t\r"), "foo", "strip whitespace chars" );
    reprIs( lstrip("\r\n\t \r", "\r"), "\n\t \r", "lstrip custom chars" );
    reprIs( rstrip("\r\n\t \r", "\r"), "\r\n\t ", "rstrip custom chars" );
    reprIs( strip("\r\n\t \r", "\r"), "\n\t ", "strip custom chars" );

    var nf = numberFormatter("$###,###.00 footer");
    t.is( nf(1000.1), "$1,000.10 footer", "trailing zeros" );
    t.is( nf(1000000.1), "$1,000,000.10 footer", "two seps" );
    t.is( nf(100), "$100.00 footer", "shorter than sep" );
    t.is( nf(100.555), "$100.56 footer", "rounding" );
    t.is( nf(-100.555), "$-100.56 footer", "default neg" );
    nf = numberFormatter("-$###,###.00");
    t.is( nf(-100.555), "-$100.56", "custom neg" );
    nf = numberFormatter("0000.0000");
    t.is( nf(0), "0000.0000", "leading and trailing" );
    t.is( nf(1.1), "0001.1000", "leading and trailing" );
    t.is( nf(12345.12345), "12345.1235", "no need for leading/trailing" );
    nf = numberFormatter("0000.0000");
    t.is( nf("taco"), "", "default placeholder" );
    nf = numberFormatter("###,###.00", "foo", "de_DE");
    t.is( nf("taco"), "foo", "custom placeholder" );
    t.is( nf(12345.12345), "12.345,12", "de_DE locale" );
    nf = numberFormatter("#%");
    t.is( nf(1), "100%", "trivial percent" );
    t.is( nf(0.55), "55%", "percent" );

    var customLocale = {
        separator: " apples and ",
        decimal: " bagels at ",
        percent: "am for breakfast"};
    var customFormatter = numberFormatter("###,###.0%", "No breakfast", customLocale);
    t.is( customFormatter(23.458), "2 apples and 345 bagels at 8am for breakfast", "custom locale" );

    nf = numberFormatter("###,###");
    t.is( nf(123), "123", "large number format" );
    t.is( nf(1234), "1,234", "large number format" );
    t.is( nf(12345), "12,345", "large number format" );
    t.is( nf(123456), "123,456", "large number format" );
    t.is( nf(1234567), "1,234,567", "large number format" );
    t.is( nf(12345678), "12,345,678", "large number format" );
    t.is( nf(123456789), "123,456,789", "large number format" );
    t.is( nf(1234567890), "1,234,567,890", "large number format" );
    t.is( nf(12345678901), "12,345,678,901", "large number format" );
    t.is( nf(123456789012), "123,456,789,012", "large number format" );
};
