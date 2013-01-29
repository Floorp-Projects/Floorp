/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the style inspector works properly

let doc;
let inspector;
let computedView;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    'span { font-variant: small-caps; color: #000000; } ' +
    '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="font-size: 12pt">hi.</p>\n' +
    '<p id="body" style="font-size: 12pt">I am a test-case. This text exists ' +
    'solely to provide some things to <span style="color: yellow">' +
    'highlight</span> and <span style="font-weight: bold">count</span> ' +
    'style list-items in the box at right. If you are reading this, ' +
    'you should go do something else instead. Maybe read a book. Or better ' +
    'yet, write some test-cases for another bit of code. ' +
    '<span style="font-style: italic">Maybe more inspector test-cases!</span></p>\n' +
    '<p id="closing">end transmission</p>\n' +
    '<p>Inspect using inspectstyle(document.querySelectorAll("span")[0])</p>' +
    '</div>';
  doc.title = "Style Inspector Test";

  openInspector(openComputedView);
}

function openComputedView(aInspector)
{
  inspector = aInspector;

  inspector.sidebar.once("computedview-ready", function() {
    computedView = getComputedView(inspector);

    inspector.sidebar.select("computedview");
    runStyleInspectorTests();
  });
}

function runStyleInspectorTests()
{
  var spans = doc.querySelectorAll("span");
  ok(spans, "captain, we have the spans");

  for (var i = 0, numSpans = spans.length; i < numSpans; i++) {
    inspector.selection.setNode(spans[i]);

    is(spans[i], computedView.viewedElement,
      "style inspector node matches the selected node");
    is(computedView.viewedElement, computedView.cssLogic.viewedElement,
       "cssLogic node matches the cssHtmlTree node");
  }

  SI_CheckProperty();
  finishUp();
}

function SI_CheckProperty()
{
  let cssLogic = computedView.cssLogic;
  let propertyInfo = cssLogic.getPropertyInfo("color");
  ok(propertyInfo.matchedRuleCount > 0, "color property has matching rules");
}

function finishUp()
{
  doc = computedView = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,basic style inspector tests";
}
