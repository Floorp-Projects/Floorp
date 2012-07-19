/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=769273 */

function run_test()
{
  var sb = Components.utils.Sandbox("http://www.blah.com");
  sb.prop = "prop"
  var refToObjFromSb = Components.utils.evalInSandbox("var a = {prop2:'prop2'}; a", sb); 
  Components.utils.nukeSandbox(sb);
  do_check_true(Components.utils.isDeadWrapper(sb), "sb should be dead");
  try{
    sb.prop;
    do_check_true(false);
  } catch (e) {
    do_check_true(e.toString().indexOf("can't access dead object") > -1);
  }
  
  Components.utils.isDeadWrapper(refToObjFromSb, "ref to object from sb should be dead");
  try{
    refToObjFromSb.prop2;
    do_check_true(false);
  } catch (e) {
    do_check_true(e.toString().indexOf("can't access dead object") > -1);
  }
  
}
