
function doit()
{
    var dbview = Components.classes["@mozilla.org/messenger/msgdbview;1?type=threaded"].createInstance(Components.interfaces.nsIMsgDBView);
    dump("dbview = " + dbview + "\n");
}
