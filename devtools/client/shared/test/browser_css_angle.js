/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */
"use strict";

const TEST_URI = "data:text/html;charset=utf-8,browser_css_angle.js";
var {angleUtils} = require("devtools/client/shared/css-angle");

add_task(function* () {
  yield addTab("about:blank");
  let [host] = yield createHost("bottom", TEST_URI);

  info("Starting the test");
  testAngleUtils();

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testAngleUtils() {
  let data = getTestData();

  for (let {authored, deg, rad, grad, turn} of data) {
    let angle = new angleUtils.CssAngle(authored);

    // Check all values.
    info("Checking values for " + authored);
    is(angle.deg, deg, "color.deg === deg");
    is(angle.rad, rad, "color.rad === rad");
    is(angle.grad, grad, "color.grad === grad");
    is(angle.turn, turn, "color.turn === turn");

    testToString(angle, deg, rad, grad, turn);
  }
}

function testToString(angle, deg, rad, grad, turn) {
  angle.angleUnit = angleUtils.CssAngle.ANGLEUNIT.deg;
  is(angle.toString(), deg, "toString() with deg type");

  angle.angleUnit = angleUtils.CssAngle.ANGLEUNIT.rad;
  is(angle.toString(), rad, "toString() with rad type");

  angle.angleUnit = angleUtils.CssAngle.ANGLEUNIT.grad;
  is(angle.toString(), grad, "toString() with grad type");

  angle.angleUnit = angleUtils.CssAngle.ANGLEUNIT.turn;
  is(angle.toString(), turn, "toString() with turn type");
}

function getTestData() {
  return [{
    authored: "0deg",
    deg: "0deg",
    rad: "0rad",
    grad: "0grad",
    turn: "0turn"
  }, {
    authored: "180deg",
    deg: "180deg",
    rad: `${Math.round(Math.PI * 10000) / 10000}rad`,
    grad: "200grad",
    turn: "0.5turn"
  }, {
    authored: "180DEG",
    deg: "180DEG",
    rad: `${Math.round(Math.PI * 10000) / 10000}RAD`,
    grad: "200GRAD",
    turn: "0.5TURN"
  }, {
    authored: `-${Math.PI}rad`,
    deg: "-180deg",
    rad: `-${Math.PI}rad`,
    grad: "-200grad",
    turn: "-0.5turn"
  }, {
    authored: `-${Math.PI}RAD`,
    deg: "-180DEG",
    rad: `-${Math.PI}RAD`,
    grad: "-200GRAD",
    turn: "-0.5TURN"
  }, {
    authored: "100grad",
    deg: "90deg",
    rad: `${Math.round(Math.PI / 2 * 10000) / 10000}rad`,
    grad: "100grad",
    turn: "0.25turn"
  }, {
    authored: "100GRAD",
    deg: "90DEG",
    rad: `${Math.round(Math.PI / 2 * 10000) / 10000}RAD`,
    grad: "100GRAD",
    turn: "0.25TURN"
  }, {
    authored: "-1turn",
    deg: "-360deg",
    rad: `${-1 * Math.round(Math.PI * 2 * 10000) / 10000}rad`,
    grad: "-400grad",
    turn: "-1turn"
  }, {
    authored: "-10TURN",
    deg: "-3600DEG",
    rad: `${-1 * Math.round(Math.PI * 2 * 10 * 10000) / 10000}RAD`,
    grad: "-4000GRAD",
    turn: "-10TURN"
  }, {
    authored: "inherit",
    deg: "inherit",
    rad: "inherit",
    grad: "inherit",
    turn: "inherit"
  }, {
    authored: "initial",
    deg: "initial",
    rad: "initial",
    grad: "initial",
    turn: "initial"
  }, {
    authored: "unset",
    deg: "unset",
    rad: "unset",
    grad: "unset",
    turn: "unset"
  }];
}
