/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test expected outputs of the output-parser's parseCssProperty function.

// This is more of a unit test than a mochitest-browser test, but can't be
// tested with an xpcshell test as the output-parser requires the DOM to work.

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let {OutputParser} = devtools.require("devtools/output-parser");

const COLOR_CLASS = "color-class";
const URL_CLASS = "url-class";

function test() {
  function countAll(fragment) {
    return fragment.querySelectorAll("*").length;
  }
  function countColors(fragment) {
    return fragment.querySelectorAll("." + COLOR_CLASS).length;
  }
  function countUrls(fragment) {
    return fragment.querySelectorAll("." + URL_CLASS).length;
  }
  function getColor(fragment, index) {
    return fragment.querySelectorAll("." + COLOR_CLASS)[index||0].textContent;
  }
  function getUrl(fragment, index) {
    return fragment.querySelectorAll("." + URL_CLASS)[index||0].textContent;
  }

  let testData = [
    {
      name: "width",
      value: "100%",
      test: fragment => {
        is(countAll(fragment), 0);
        is(fragment.textContent, "100%");
      }
    },
    {
      name: "width",
      value: "blue",
      test: fragment => {
        is(countAll(fragment), 0);
      }
    },
    {
      name: "content",
      value: "'red url(test.png) repeat top left'",
      test: fragment => {
        is(countAll(fragment), 0);
      }
    },
    {
      name: "content",
      value: "\"blue\"",
      test: fragment => {
        is(countAll(fragment), 0);
      }
    },
    {
      name: "margin-left",
      value: "url(something.jpg)",
      test: fragment => {
        is(countAll(fragment), 0);
      }
    },
    {
      name: "background-color",
      value: "transparent",
      test: fragment => {
        is(countAll(fragment), 1);
        is(countColors(fragment), 1);
        is(fragment.textContent, "transparent");
      }
    },
    {
      name: "color",
      value: "red",
      test: fragment => {
        is(countColors(fragment), 1);
        is(fragment.textContent, "red");
      }
    },
    {
      name: "color",
      value: "#F06",
      test: fragment => {
        is(countColors(fragment), 1);
        is(fragment.textContent, "#F06");
      }
    },
    {
      name: "border-top-left-color",
      value: "rgba(14, 255, 20, .5)",
      test: fragment => {
        is(countAll(fragment), 1);
        is(countColors(fragment), 1);
        is(fragment.textContent, "rgba(14, 255, 20, .5)");
      }
    },
    {
      name: "border",
      value: "80em dotted pink",
      test: fragment => {
        is(countAll(fragment), 1);
        is(countColors(fragment), 1);
        is(getColor(fragment), "pink");
      }
    },
    {
      name: "color",
      value: "red !important",
      test: fragment => {
        is(countColors(fragment), 1);
        is(fragment.textContent, "red !important");
      }
    },
    {
      name: "background",
      value: "red url(test.png) repeat top left",
      test: fragment => {
        is(countColors(fragment), 1);
        is(countUrls(fragment), 1);
        is(getColor(fragment), "red");
        is(getUrl(fragment), "test.png");
        is(countAll(fragment), 2);
      }
    },
    {
      name: "background",
      value: "blue url(test.png) repeat top left !important",
      test: fragment => {
        is(countColors(fragment), 1);
        is(countUrls(fragment), 1);
        is(getColor(fragment), "blue");
        is(getUrl(fragment), "test.png");
        is(countAll(fragment), 2);
      }
    },
    {
      name: "list-style-image",
      value: "url(\"images/arrow.gif\")",
      test: fragment => {
        is(countAll(fragment), 1);
        is(getUrl(fragment), "images/arrow.gif");
      }
    },
    {
      name: "list-style-image",
      value: "url(\"images/arrow.gif\")!important",
      test: fragment => {
        is(countAll(fragment), 1);
        is(getUrl(fragment), "images/arrow.gif");
        is(fragment.textContent, "url('images/arrow.gif')!important");
      }
    },
    {
      name: "-moz-binding",
      value: "url(http://somesite.com/path/to/binding.xml#someid)",
      test: fragment => {
        is(countAll(fragment), 1);
        is(countUrls(fragment), 1);
        is(getUrl(fragment), "http://somesite.com/path/to/binding.xml#someid");
      }
    },
    {
      name: "background",
      value: "linear-gradient(to right, rgba(183,222,237,1) 0%, rgba(33,180,226,1) 30%, rgba(31,170,217,.5) 44%, #F06 75%, red 100%)",
      test: fragment => {
        is(countAll(fragment), 5);
        let allSwatches = fragment.querySelectorAll("." + COLOR_CLASS);
        is(allSwatches.length, 5);
        is(allSwatches[0].textContent, "rgba(183,222,237,1)");
        is(allSwatches[1].textContent, "rgba(33,180,226,1)");
        is(allSwatches[2].textContent, "rgba(31,170,217,.5)");
        is(allSwatches[3].textContent, "#F06");
        is(allSwatches[4].textContent, "red");
      }
    },
    {
      name: "background",
      value: "-moz-radial-gradient(center 45deg, circle closest-side, orange 0%, red 100%)",
      test: fragment => {
        is(countAll(fragment), 2);
        let allSwatches = fragment.querySelectorAll("." + COLOR_CLASS);
        is(allSwatches.length, 2);
        is(allSwatches[0].textContent, "orange");
        is(allSwatches[1].textContent, "red");
      }
    },
    {
      name: "background",
      value: "white  url(http://test.com/wow_such_image.png) no-repeat top left",
      test: fragment => {
        is(countAll(fragment), 2);
        is(countUrls(fragment), 1);
        is(countColors(fragment), 1);
      }
    },
    {
      name: "background",
      value: "url(\"http://test.com/wow_such_(oh-noes)image.png?testid=1&color=red#w00t\")",
      test: fragment => {
        is(countAll(fragment), 1);
        is(getUrl(fragment), "http://test.com/wow_such_(oh-noes)image.png?testid=1&color=red#w00t");
      }
    },
    {
      name: "background-image",
      value: "url(this-is-an-incredible-image.jpeg)",
      test: fragment => {
        is(countAll(fragment), 1);
        is(getUrl(fragment), "this-is-an-incredible-image.jpeg");
      }
    },
    {
      name: "background",
      value: "red url(    \"http://wow.com/cool/../../../you're(doingit)wrong\"   ) repeat center",
      test: fragment => {
        is(countAll(fragment), 2);
        is(countColors(fragment), 1);
        is(getUrl(fragment), "http://wow.com/cool/../../../you're(doingit)wrong");
      }
    },
    {
      name: "background-image",
      value: "url(../../../look/at/this/folder/structure/../../red.blue.green.svg   )",
      test: fragment => {
        is(countAll(fragment), 1);
        is(getUrl(fragment), "../../../look/at/this/folder/structure/../../red.blue.green.svg");
      }
    }
  ];

  let parser = new OutputParser();
  for (let i = 0; i < testData.length; i ++) {
    let data = testData[i];
    info("Output-parser test data " + i + ". {" + data.name + " : " + data.value + ";}");
    data.test(parser.parseCssProperty(data.name, data.value, {
      colorClass: COLOR_CLASS,
      urlClass: URL_CLASS,
      defaultColorType: false
    }));
  }

  finish();
}
