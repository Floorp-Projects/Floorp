/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
let {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
let {OutputParser} = devtools.require("devtools/output-parser");

let parser;
let doc;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(init, content);
    doc = content.document;
  }, true);

  content.location = "data:text/html,<h1>browser_outputParser.js</h1>" +
                     "<div></div>";
}

function init() {
  parser = new OutputParser();
  testParseCssProperty();
}

function testParseCssProperty() {
  let frag = parser.parseCssProperty("border", "1px solid red", {
    colorSwatchClass: "test-colorswatch"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  is(target.innerHTML,
     '1px solid <span style="background-color:red" class="test-colorswatch"></span><span>#F00</span>',
     "CSS property correctly parsed");

  target.innerHTML = "";

  let frag = parser.parseCssProperty("background-image", "linear-gradient(to right, #F60 10%, rgba(0,0,0,1))", {
    colorSwatchClass: "test-colorswatch",
    colorClass: "test-color"
  });
  target.appendChild(frag);
  is(target.innerHTML,
     'linear-gradient(to right, <span style="background-color:#F60" class="test-colorswatch"></span><span class="test-color">#F60</span> 10%, ' +
     '<span style="background-color:rgba(0,0,0,1)" class="test-colorswatch"></span><span class="test-color">#000</span>)',
     "Gradient CSS property correctly parsed");

  target.innerHTML = "";

  testParseHTMLAttribute();
}

function testParseHTMLAttribute() {
  let attrib = "color:red; font-size: 12px; background-image: " +
               "url(chrome://branding/content/about-logo.png)";
  let frag = parser.parseHTMLAttribute(attrib, {
    urlClass: "theme-link",
    colorClass: "theme-color"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  let expected = 'color:<span class="theme-color">#F00</span>; font-size: 12px; ' +
                 'background-image: url(\'<a href="chrome://branding/content/about-logo.png" ' +
                 'class="theme-link" ' +
                 'target="_blank">chrome://branding/content/about-logo.png</a>\')';

  is(target.innerHTML, expected, "HTML Attribute correctly parsed");
  target.innerHTML = "";
  testParseNonCssHTMLAttribute();
}

function testParseNonCssHTMLAttribute() {
  let attrib = "someclass background someotherclass red";
  let frag = parser.parseHTMLAttribute(attrib);

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  let expected = 'someclass background someotherclass red';

  is(target.innerHTML, expected, "Non-CSS HTML Attribute correctly parsed");
  target.innerHTML = "";
  finishUp();
}


function finishUp() {
  Services = Loader = OutputParser = parser = doc = null;
  gBrowser.removeCurrentTab();
  finish();
}
