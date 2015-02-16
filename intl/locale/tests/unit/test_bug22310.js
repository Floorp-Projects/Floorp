String.prototype.has = function(s) { return this.indexOf(s) != -1; }

const Cc = Components.classes;
const Ci = Components.interfaces;

const dts = Cc["@mozilla.org/intl/scriptabledateformat;1"]
        .getService(Ci.nsIScriptableDateFormat);

function dt(locale) {
  return dts.FormatDateTime(locale, dts.dateFormatLong,
                            dts.timeFormatSeconds, 2008, 6, 30, 13, 56, 34);
}

var all_passed = true;
const tests = 
[
 [dt("en-US").has("June"), "month name in en-US"],
 [dt("en-US").has("2008"), "year in en-US"],
 [dt("da").has("jun"), "month name in da"],
 [dt("da-DK") == dt("da"), "da same as da-DK"],
 [dt("en-GB").has("30") && dt("en-GB").has("June") &&
   dt("en-GB").indexOf("30") < dt("en-GB").indexOf("June"),
  "day before month in en-GB"],
 [dt("en-US").has("30") && dt("en-US").has("June") &&
   dt("en-US").indexOf("30") > dt("en-US").indexOf("June"),
  "month before day in en-US"],
 [dt("ja-JP").has("\u5E746\u670830\u65E5"), "year month and day in ja-JP"],
 [dt("ja-JP") == dt("ja-JP-mac"), "ja-JP-mac same as ja-JP"],
 [dt("nn-NO").has("juni"), "month name in nn-NO"],
 [dt("nb-NO").has("juni"), "month name in nb-NO"],
 [dt("no-NO").has("30. juni"), "month name in no-NO"],
 [dt("sv-SE").has("30 jun"), "month name in sv-SE"],
 [dt("kok").has("\u091C\u0942\u0928"), "month name in kok"],
 [dt("ta-IN").has("\u0B9C\u0BC2\u0BA9\u0BCD"), "month name in ta-IN"],
 [dt("ab-CD").length > 0, "fallback for ab-CD"]
];

function one_test(testcase, msg)
{
    if (!testcase) {
        all_passed = false;
        dump("Unexpected date format: " + msg + "\n");
    }
}

function run_test()
{
    for (var i = 0; i < tests.length; ++i) {
        one_test(tests[i][0], tests[i][1]);
    }
    do_check_true(all_passed);
}
