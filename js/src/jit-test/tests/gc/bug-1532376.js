"use strict";
function __f_276() {
    this.getNameReallyHard = () => eval("eval('(() => this.name)()')")
}
for (var __v_1377 = 0; __v_1377 < 10000; __v_1377++) {
  var __v_1378 = new __f_276();
  try {
    __v_1376[__getRandomProperty()];
  } catch (e) {}
__v_1378.getNameReallyHard()}
