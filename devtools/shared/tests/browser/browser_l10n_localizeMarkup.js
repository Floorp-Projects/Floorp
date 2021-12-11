/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the markup localization works properly.

const { localizeMarkup, LocalizationHelper } = require("devtools/shared/l10n");
const HTML_NS = "http://www.w3.org/1999/xhtml";

add_task(async function() {
  info("Check that the strings used for this test are still valid");
  const STARTUP_L10N = new LocalizationHelper(
    "devtools/client/locales/startup.properties"
  );
  const TOOLBOX_L10N = new LocalizationHelper(
    "devtools/client/locales/toolbox.properties"
  );
  const str1 = STARTUP_L10N.getStr("inspector.label");
  const str2 = STARTUP_L10N.getStr("inspector.accesskey");
  const str3 = TOOLBOX_L10N.getStr("toolbox.defaultTitle");
  ok(
    str1 && str2 && str3,
    "If this failed, strings should be updated in the test"
  );

  info("Create the test markup");
  const div = document.createElementNS(HTML_NS, "div");
  div.setAttribute(
    "data-localization-bundle",
    "devtools/client/locales/startup.properties"
  );
  const div0 = document.createElementNS(HTML_NS, "div");
  div0.setAttribute("id", "d0");
  div0.setAttribute("data-localization", "content=inspector.someInvalidKey");
  div.appendChild(div0);
  const div1 = document.createElementNS(HTML_NS, "div");
  div1.setAttribute("id", "d1");
  div1.setAttribute("data-localization", "content=inspector.label");
  div.appendChild(div1);
  div1.append("Text will disappear");
  const div2 = document.createElementNS(HTML_NS, "div");
  div2.setAttribute("id", "d2");
  div2.setAttribute(
    "data-localization",
    "content=inspector.label;title=inspector.accesskey"
  );
  div.appendChild(div2);
  const div3 = document.createElementNS(HTML_NS, "div");
  div3.setAttribute("id", "d3");
  div3.setAttribute(
    "data-localization",
    "content=inspector.label;title=inspector.accesskey"
  );
  div.appendChild(div3);
  const div4 = document.createElementNS(HTML_NS, "div");
  div4.setAttribute("id", "d4");
  div4.setAttribute("data-localization", "aria-label=inspector.label");
  div.appendChild(div4);
  div4.append("Some content");
  const toolboxDiv = document.createElementNS(HTML_NS, "div");
  toolboxDiv.setAttribute(
    "data-localization-bundle",
    "devtools/client/locales/toolbox.properties"
  );
  div.appendChild(toolboxDiv);
  const div5 = document.createElementNS(HTML_NS, "div");
  div5.setAttribute("id", "d5");
  div5.setAttribute("data-localization", "content=toolbox.defaultTitle");
  toolboxDiv.appendChild(div5);

  info("Use localization helper to localize the test markup");
  localizeMarkup(div);

  is(div1.innerHTML, str1, "The content of #d1 is localized");
  is(div2.innerHTML, str1, "The content of #d2 is localized");
  is(div2.getAttribute("title"), str2, "The title of #d2 is localized");
  is(div3.innerHTML, str1, "The content of #d3 is localized");
  is(div3.getAttribute("title"), str2, "The title of #d3 is localized");
  is(div4.innerHTML, "Some content", "The content of #d4 is not replaced");
  is(
    div4.getAttribute("aria-label"),
    str1,
    "The aria-label of #d4 is localized"
  );
  is(
    div5.innerHTML,
    str3,
    "The content of #d5 is localized with another bundle"
  );
});
