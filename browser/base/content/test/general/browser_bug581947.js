function check(aElementName, aBarred, aType) {
  let doc = gBrowser.contentDocument;
  let tooltip = document.getElementById("aHTMLTooltip");
  let content = doc.getElementById('content');

  let e = doc.createElement(aElementName);
  content.appendChild(e);
  
  if (aType) {
    e.type = aType;
  }

  ok(!tooltip.fillInPageTooltip(e),
     "No tooltip should be shown when the element is valid");

  e.setCustomValidity('foo');
  if (aBarred) {
    ok(!tooltip.fillInPageTooltip(e),
       "No tooltip should be shown when the element is barred from constraint validation");
  } else {
    ok(tooltip.fillInPageTooltip(e),
       e.tagName + " " +"A tooltip should be shown when the element isn't valid");
  }

  e.setAttribute('title', '');
  ok (!tooltip.fillInPageTooltip(e),
      "No tooltip should be shown if the title attribute is set");

  e.removeAttribute('title');
  content.setAttribute('novalidate', '');
  ok (!tooltip.fillInPageTooltip(e),
      "No tooltip should be shown if the novalidate attribute is set on the form owner");
  content.removeAttribute('novalidate');

  content.removeChild(e);
}

function todo_check(aElementName, aBarred) {
  let doc = gBrowser.contentDocument;
  let tooltip = document.getElementById("aHTMLTooltip");
  let content = doc.getElementById('content');

  let e = doc.createElement(aElementName);
  content.appendChild(e);

  let cought = false;
  try {
    e.setCustomValidity('foo');
  } catch (e) {
    cought = true;
  }

  todo(!cought, "setCustomValidity should exist for " + aElementName);

  content.removeChild(e);
}

function test () {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let testData = [
    /* element name, barred */
      [ 'input',    false,  null],
      [ 'textarea', false,  null],
      [ 'button',   true,  'button'],
      [ 'button',   false, 'submit'],
      [ 'select',   false,  null],
      [ 'output',   true,   null],
      [ 'fieldset', true,   null],
      [ 'object',   true,   null],
    ];

    for each (let data in testData) {
      check(data[0], data[1], data[2]);
    }

    let todo_testData = [
      [ 'keygen', 'false' ],
    ];

    for each(let data in todo_testData) {
      todo_check(data[0], data[1]);
    }

    gBrowser.removeCurrentTab();
    finish();
  }, true);

  content.location = 
    "data:text/html,<!DOCTYPE html><html><body><form id='content'></form></body></html>";
}

