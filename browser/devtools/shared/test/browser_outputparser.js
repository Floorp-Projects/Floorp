/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let {OutputParser} = devtools.require("devtools/output-parser");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost("bottom", "data:text/html," +
    "<h1>browser_outputParser.js</h1><div></div>");

  let parser = new OutputParser();
  testParseCssProperty(doc, parser);
  testParseCssVar(doc, parser);
  testParseHTMLAttribute(doc, parser);
  testParseNonCssHTMLAttribute(doc, parser);

  host.destroy();
}

function testParseCssProperty(doc, parser) {
  let frag = parser.parseCssProperty("border", "1px solid red", {
    colorSwatchClass: "test-colorswatch"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  is(target.innerHTML,
     '1px solid <span data-color="#F00"><span style="background-color:red" class="test-colorswatch"></span><span>#F00</span></span>',
     "CSS property correctly parsed");

  target.innerHTML = "";

  frag = parser.parseCssProperty("background-image", "linear-gradient(to right, #F60 10%, rgba(0,0,0,1))", {
    colorSwatchClass: "test-colorswatch",
    colorClass: "test-color"
  });
  target.appendChild(frag);
  is(target.innerHTML,
     'linear-gradient(to right, <span data-color="#F60"><span style="background-color:#F60" class="test-colorswatch"></span><span class="test-color">#F60</span></span> 10%, ' +
     '<span data-color="#000"><span style="background-color:rgba(0,0,0,1)" class="test-colorswatch"></span><span class="test-color">#000</span></span>)',
     "Gradient CSS property correctly parsed");

  target.innerHTML = "";
}

function testParseCssVar(doc, parser) {
  let frag = parser.parseCssProperty("color", "var(--some-kind-of-green)", {
    colorSwatchClass: "test-colorswatch"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  is(target.innerHTML, "var(--some-kind-of-green)", "CSS property correctly parsed");

  target.innerHTML = "";
}

function testParseHTMLAttribute(doc, parser) {
  let attrib = "color:red; font-size: 12px; background-image: " +
               "url(chrome://branding/content/about-logo.png)";
  let frag = parser.parseHTMLAttribute(attrib, {
    urlClass: "theme-link",
    colorClass: "theme-color"
  });

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  let expected = 'color:<span data-color="#F00"><span class="theme-color">#F00</span></span>; font-size: 12px; ' +
                 'background-image: url("<a href="chrome://branding/content/about-logo.png" ' +
                 'class="theme-link" ' +
                 'target="_blank">chrome://branding/content/about-logo.png</a>")';

  is(target.innerHTML, expected, "HTML Attribute correctly parsed");
  target.innerHTML = "";
}

function testParseNonCssHTMLAttribute(doc, parser) {
  let attrib = "someclass background someotherclass red";
  let frag = parser.parseHTMLAttribute(attrib);

  let target = doc.querySelector("div");
  ok(target, "captain, we have the div");
  target.appendChild(frag);

  let expected = 'someclass background someotherclass red';

  is(target.innerHTML, expected, "Non-CSS HTML Attribute correctly parsed");
  target.innerHTML = "";
}
