/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let doc;
let win;
let stylePanel;

function createDocument()
{
  doc.body.innerHTML = '<style type="text/css"> ' +
    'html { color: #000000; } ' +
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
    '<span style="font-style: italic">some text</span></p>\n' +
    '<p id="closing">more text</p>\n' +
    '<p>even more text</p>' +
    '</div>';
  doc.title = "Rule view style editor link test";

  let span = doc.querySelector("span");
  ok(span, "captain, we have the span");

  Services.obs.addObserver(testInlineStyle, "StyleInspector-populated", false);
  stylePanel = new ComputedViewPanel(window);
  stylePanel.createPanel(span);
}

function testInlineStyle()
{
  Services.obs.removeObserver(testInlineStyle, "StyleInspector-populated", false);

  info("expanding property");
  expandProperty(0, function propertyExpanded() {
    Services.ww.registerNotification(function onWindow(aSubject, aTopic) {
      if (aTopic != "domwindowopened") {
        return;
      }
      info("window opened");
      win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function windowLoad() {
        win.removeEventListener("load", windowLoad);
        info("window load completed");
        let windowType = win.document.documentElement.getAttribute("windowtype");
        is(windowType, "navigator:view-source", "view source window is open");
        info("closing window");
        win.close();
        Services.ww.unregisterNotification(onWindow);
        testInlineStyleSheet();
      });
    });
    let link = getLinkByIndex(0);
    link.click();
  });
}

function testInlineStyleSheet()
{
  info("clicking an inline stylesheet");

  Services.ww.registerNotification(function onWindow(aSubject, aTopic) {
    if (aTopic != "domwindowopened") {
      return;
    }
    info("window opened");
    win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function windowLoad() {
      win.removeEventListener("load", windowLoad);
      info("window load completed");
      let windowType = win.document.documentElement.getAttribute("windowtype");
      is(windowType, "Tools:StyleEditor", "style editor window is open");

      win.styleEditorChrome.addChromeListener({
        onEditorAdded: function checkEditor(aChrome, aEditor) {
          if (!aEditor.sourceEditor) {
            aEditor.addActionListener({
              onAttach: function (aEditor) {
                aEditor.removeActionListener(this);
                validateStyleEditorSheet(aEditor);
              }
            });
          } else {
            validateStyleEditorSheet(aEditor);
          }
        }
      });
      Services.ww.unregisterNotification(onWindow);
    });
  });
  let link = getLinkByIndex(1);
  link.click();
}

function validateStyleEditorSheet(aEditor)
{
  info("validating style editor stylesheet");

  let sheet = doc.styleSheets[0];
  is(aEditor.styleSheet, sheet, "loaded stylesheet matches document stylesheet");
  info("closing window");
  win.close();

  stylePanel.destroy();
  finishUp();
}

function expandProperty(aIndex, aCallback)
{
  let iframe = stylePanel.iframe;
  let contentDoc = iframe.contentDocument;
  let contentWindow = iframe.contentWindow;
  let expando = contentDoc.querySelectorAll(".expandable")[aIndex];
  expando.click();

  // We use executeSoon to give the property time to expand.
  executeSoon(aCallback);
}

function getLinkByIndex(aIndex)
{
  let contentDoc = stylePanel.iframe.contentDocument;
  let links = contentDoc.querySelectorAll(".rule-link .link");
  return links[aIndex];
}

function finishUp()
{
  doc = win = stylePanel = null;
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,<p>Computed view style editor link test</p>";
}
