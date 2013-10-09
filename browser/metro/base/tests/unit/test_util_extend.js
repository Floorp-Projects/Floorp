"use strict";

Components.utils.import("resource:///modules/ContentUtil.jsm");
let Util = ContentUtil;

function run_test() {
  do_print("Testing Util.extend");

  do_print("Check if function is defined");
  do_check_true(!!Util.extend);

  do_print("No parameter or null should return a new object");
  let noArgRes = Util.extend();
  do_check_true(noArgRes && typeof noArgRes == "object");

  let nullRes = Util.extend(null);
  do_check_true(nullRes && typeof nullRes == "object");

  do_print("Simple extend");
  let simpleExtend = {a: 1, b: 2};
  let simpleExtendResult = Util.extend(simpleExtend, {b: 3, c: 4});

  do_check_true(simpleExtend.a === 1 && simpleExtend.b === 3 && simpleExtend.c === 4);
  do_check_true(simpleExtendResult === simpleExtend);

  do_print("Cloning");
  let cloneExtend = {a: 1, b: 2};
  let cloned = Util.extend({}, cloneExtend);

  do_check_true(cloneExtend.a === cloned.a && cloneExtend.b === cloned.b);
  do_check_true(cloneExtend !== cloned);

  do_print("Multiple extend");
  let multiExtend1 = {a: 1, b: 2};
  let multiExtend2 = {c: 3, d: 4};
  let multiExtend3 = {b: 5, e: 6};
  let multiExtend4 = {e: 7, a: 8};
  let multiResult = Util.extend({}, multiExtend1, multiExtend2, multiExtend3, multiExtend4);

  do_check_true(multiResult.a === 8 && multiResult.b === 5 && multiResult.c === 3 && multiResult.d === 4 && multiResult.e === 7);
}
