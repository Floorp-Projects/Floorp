/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the markup localization works properly.

const { localizeMarkup, LocalizationHelper } = require("devtools/shared/l10n");

add_task(function* () {
  info("Check that the strings used for this test are still valid");
  let STARTUP_L10N = new LocalizationHelper("devtools/client/locales/startup.properties");
  let TOOLBOX_L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");
  let str1 = STARTUP_L10N.getStr("inspector.label");
  let str2 = STARTUP_L10N.getStr("inspector.commandkey");
  let str3 = TOOLBOX_L10N.getStr("toolbox.defaultTitle");
  ok(str1 && str2 && str3, "If this failed, strings should be updated in the test");

  info("Create the test markup");
  let div = document.createElement("div");
  div.innerHTML =
  `<div data-localization-bundle="devtools/client/locales/startup.properties">
     <div id="d0" data-localization="content=inspector.someInvalidKey"></div>
     <div id="d1" data-localization="content=inspector.label">Text will disappear</div>
     <div id="d2" data-localization="content=inspector.label;title=inspector.commandkey">
     </div>
     <!-- keep the following data-localization on two separate lines -->
     <div id="d3" data-localization="content=inspector.label;
                                     title=inspector.commandkey"></div>
     <div id="d4" data-localization="aria-label=inspector.label">Some content</div>
     <div data-localization-bundle="devtools/client/locales/toolbox.properties">
       <div id="d5" data-localization="content=toolbox.defaultTitle"></div>
     </div>
   </div>
  `;

  info("Use localization helper to localize the test markup");
  localizeMarkup(div);

  let div1 = div.querySelector("#d1");
  let div2 = div.querySelector("#d2");
  let div3 = div.querySelector("#d3");
  let div4 = div.querySelector("#d4");
  let div5 = div.querySelector("#d5");

  is(div1.innerHTML, str1, "The content of #d1 is localized");
  is(div2.innerHTML, str1, "The content of #d2 is localized");
  is(div2.getAttribute("title"), str2, "The title of #d2 is localized");
  is(div3.innerHTML, str1, "The content of #d3 is localized");
  is(div3.getAttribute("title"), str2, "The title of #d3 is localized");
  is(div4.innerHTML, "Some content", "The content of #d4 is not replaced");
  is(div4.getAttribute("aria-label"), str1, "The aria-label of #d4 is localized");
  is(div5.innerHTML, str3, "The content of #d5 is localized with another bundle");
});
