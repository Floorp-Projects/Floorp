/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that links are shown in attributes when the values (or part of the
// values) are URIs or pointers to IDs.

const TEST_URL = TEST_URL_ROOT + "doc_markup_links.html";

const TEST_DATA = [{
  selector: "link",
  attributes: [{
    attributeName: "href",
    links: [{type: "cssresource", value: "style.css"}]
  }]
}, {
  selector: "link[rel=icon]",
  attributes: [{
    attributeName: "href",
    links: [{type: "uri", value: "/media/img/firefox/favicon-196.223e1bcaf067.png"}]
  }]
}, {
  selector: "form",
  attributes: [{
    attributeName: "action",
    links: [{type: "uri", value: "/post_message"}]
  }]
}, {
  selector: "label[for=name]",
  attributes: [{
    attributeName: "for",
    links: [{type: "idref", value: "name"}]
  }]
}, {
  selector: "label[for=message]",
  attributes: [{
    attributeName: "for",
    links: [{type: "idref", value: "message"}]
  }]
}, {
  selector: "output",
  attributes: [{
    attributeName: "form",
    links: [{type: "idref", value: "message-form"}]
  }, {
    attributeName: "for",
    links: [
      {type: "idref", value: "name"},
      {type: "idref", value: "message"},
      {type: "idref", value: "invalid"}
    ]
  }]
}, {
  selector: "a",
  attributes: [{
    attributeName: "href",
    links: [{type: "uri", value: "/go/somewhere/else"}]
  }, {
    attributeName: "ping",
    links: [
      {type: "uri", value: "/analytics?page=pageA"},
      {type: "uri", value: "/analytics?user=test"}
    ]
  }]
}, {
  selector: "li[contextmenu=menu1]",
  attributes: [{
    attributeName: "contextmenu",
    links: [{type: "idref", value: "menu1"}]
  }]
}, {
  selector: "li[contextmenu=menu2]",
  attributes: [{
    attributeName: "contextmenu",
    links: [{type: "idref", value: "menu2"}]
  }]
}, {
  selector: "li[contextmenu=menu3]",
  attributes: [{
    attributeName: "contextmenu",
    links: [{type: "idref", value: "menu3"}]
  }]
}, {
  selector: "video",
  attributes: [{
    attributeName: "poster",
    links: [{type: "uri", value: "doc_markup_tooltip.png"}]
  }, {
    attributeName: "src",
    links: [{type: "uri", value: "code-rush.mp4"}]
  }]
}, {
  selector: "script",
  attributes: [{
    attributeName: "src",
    links: [{type: "jsresource", value: "lib_jquery_1.0.js"}]
  }]
}];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  for (let {selector, attributes} of TEST_DATA) {
    info("Testing attributes on node " + selector);
    yield selectNode(selector, inspector);
    let {editor} = yield getContainerForSelector(selector, inspector);

    for (let {attributeName, links} of attributes) {
      info("Testing attribute " + attributeName);
      let linkEls = editor.attrElements.get(attributeName).querySelectorAll(".link");

      is(linkEls.length, links.length, "The right number of links were found");

      for (let i = 0; i < links.length; i ++) {
        is(linkEls[i].dataset.type, links[i].type, "Link " + i + " has the right type");
        is(linkEls[i].textContent, links[i].value, "Link " + i + " has the right value");
      }
    }
  }
});
