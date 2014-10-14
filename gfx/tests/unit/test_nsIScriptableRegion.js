function run_test()
{
  let rgn = Components.classes["@mozilla.org/gfx/region;1"].createInstance(Components.interfaces.nsIScriptableRegion);
  do_check_true (rgn.getRects() === null)
  rgn.unionRect(0,0,80,60);
  do_check_true (rgn.getRects().toString() == "0,0,80,60")
  rgn.unionRect(90,70,1,1);
  do_check_true (rgn.getRects().toString() == "0,0,80,60,90,70,1,1")
}

