/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let {Rect} = Components.utils.import("resource:///modules/tabview/utils.jsm", {});
  
  let referenceRect = new Rect(50,50,150,150);
  let rect = new Rect(100,100,100,100);
  
  ok(referenceRect.contains(referenceRect), "A rect contains itself");
  ok(referenceRect.contains(rect), "[50,50,150,150] contains [100,100,100,100]");
  rect.inset(-1,-1);
  ok(!referenceRect.contains(rect), "Now it grew and [50,50,150,150] doesn't contain [99,99,102,102]");
}
