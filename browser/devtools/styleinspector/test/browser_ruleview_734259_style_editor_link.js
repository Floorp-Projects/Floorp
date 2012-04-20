/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let win;
let doc;
let contentWindow;

let tempScope = {};
Cu.import("resource:///modules/Services.jsm", tempScope);
let Services = tempScope.Services;

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

  openInspector();
}

function openInspector()
{
  ok(window.InspectorUI, "InspectorUI variable exists");
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(InspectorUI.store.isEmpty(), "Inspector.store is empty");

  Services.obs.addObserver(inspectorUIOpen,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function inspectorUIOpen()
{
  Services.obs.removeObserver(inspectorUIOpen,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Make sure the inspector is open.
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(!InspectorUI.isSidebarOpen, "Inspector Sidebar is not open");
  ok(!InspectorUI.store.isEmpty(), "InspectorUI.store is not empty");
  is(InspectorUI.store.length, 1, "Inspector.store.length = 1");

  // Highlight a node.
  let div = content.document.getElementsByTagName("div")[0];
  InspectorUI.inspectNode(div);
  InspectorUI.stopInspecting();
  is(InspectorUI.selection, div, "selection matches the div element");

  InspectorUI.currentInspector.once("sidebaractivated-ruleview", testInlineStyle);

  InspectorUI.sidebar.show();
  InspectorUI.sidebar.activatePanel("ruleview");
}

function testInlineStyle()
{
  executeSoon(function() {
    info("clicking an inline style");

    Services.ww.registerNotification(function onWindow(aSubject, aTopic) {
      if (aTopic != "domwindowopened") {
        return;
      }

      win = aSubject.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function windowLoad() {
        win.removeEventListener("load", windowLoad);
        let windowType = win.document.documentElement.getAttribute("windowtype");
        is(windowType, "navigator:view-source", "view source window is open");
        win.close();
        Services.ww.unregisterNotification(onWindow);
        testInlineStyleSheet();
      });
    });
    EventUtils.synthesizeMouseAtCenter(getLinkByIndex(0), { }, contentWindow);
  });
}

function testInlineStyleSheet()
{
  info("clicking an inline stylesheet");

  Services.ww.registerNotification(function onWindow(aSubject, aTopic) {
    if (aTopic != "domwindowopened") {
      return;
    }

    win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function windowLoad() {
      win.removeEventListener("load", windowLoad);

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

  EventUtils.synthesizeMouse(getLinkByIndex(1), 5, 5, { }, contentWindow);
}

function validateStyleEditorSheet(aEditor)
{
  info("validating style editor stylesheet");

  let sheet = doc.styleSheets[0];

  is(aEditor.styleSheet, sheet, "loaded stylesheet matches document stylesheet");
  win.close();

  finishup();
}

function getLinkByIndex(aIndex)
{
  let contentDoc = ruleView().doc;
  contentWindow = contentDoc.defaultView;
  let links = contentDoc.querySelectorAll(".ruleview-rule-source");
  return links[aIndex];
}

function finishup()
{
  InspectorUI.sidebar.hide();
  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  doc = contentWindow = win = null;
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

  content.location = "data:text/html,<p>Rule view style editor link test</p>";
}
