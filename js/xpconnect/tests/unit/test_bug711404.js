const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test()
{
  var p = Cc["@mozilla.org/hash-property-bag;1"].
          createInstance(Ci.nsIWritablePropertyBag2);
  p.setPropertyAsInt64("a", -4000);
  do_check_neq(p.getPropertyAsUint64("a"), -4000);
}
