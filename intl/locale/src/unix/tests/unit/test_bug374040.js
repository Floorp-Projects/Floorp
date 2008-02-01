function test_full() {
    var date = new Date();
    var scriptableDateServ =
    Components.classes["@mozilla.org/intl/scriptabledateformat;1"].createInstance(Components.interfaces.nsIScriptableDateFormat);

    var dateStrXpcom = scriptableDateServ.FormatDateTime("",
    scriptableDateServ.dateFormatLong, scriptableDateServ.timeFormatSeconds,
    date.getFullYear(), date.getMonth()+1, date.getDate(), date.getHours(),
    date.getMinutes(), date.getSeconds());

    var dateStrJs = date.toLocaleString();

    return (dateStrXpcom == dateStrJs);

}

function test_kTimeFormatSeconds() {
    var date = new Date();
    var scriptableDateServ =
    Components.classes["@mozilla.org/intl/scriptabledateformat;1"].createInstance(Components.interfaces.nsIScriptableDateFormat);

    var dateStrXpcom = scriptableDateServ.FormatDateTime("",
    scriptableDateServ.dateFormatLong, scriptableDateServ.timeFormatNone,
    date.getFullYear(), date.getMonth()+1, date.getDate(), date.getHours(),
    date.getMinutes(), date.getSeconds());

    var dateStrJs = date.toLocaleDateString()

    return (dateStrXpcom == dateStrJs);

}

function run_test()
{
    do_check_true(test_full());
    do_check_true(test_kTimeFormatSeconds());
}
