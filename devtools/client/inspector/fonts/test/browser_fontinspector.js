/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* global getURL, getFamilyName */
"use strict";

requestLongerTimeout(2);

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(async function() {
  await pushPref("devtools.inspector.fonteditor.enabled", true);
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  ok(!!view, "Font inspector document is alive.");

  const viewDoc = view.document;

  await testBodyFonts(inspector, viewDoc);
  await testDivFonts(inspector, viewDoc);
});

function isRemote(fontLi) {
  return fontLi.querySelector(".font-origin").classList.contains("remote");
}

async function testBodyFonts(inspector, viewDoc) {
  await selectNode("body", inspector);

  const lis = getUsedFontsEls(viewDoc);
  // test system font
  const localFontName = getName(lis[0]);
  // On Linux test machines, the Arial font doesn't exist.
  // The fallback is "Liberation Sans"
  is(lis.length, 1, "Found 1 font on BODY");
  ok((localFontName == "Arial") || (localFontName == "Liberation Sans"),
     "local font right font name");
  ok(!isRemote(lis[0]), "local font is local");
}

async function testDivFonts(inspector, viewDoc) {
  const FONTS = [{
    selector: "div",
    familyName: "bar",
    name: "Ostrich Sans Medium",
    remote: true,
    url: URL_ROOT + "ostrich-regular.ttf",
  },
  {
    selector: ".normal-text",
    familyName: "barnormal",
    name: "Ostrich Sans Medium",
    remote: true,
    url: URL_ROOT + "ostrich-regular.ttf",
  },
  {
    selector: ".bold-text",
    familyName: "bar",
    name: "Ostrich Sans Black",
    remote: true,
    url: URL_ROOT + "ostrich-black.ttf",
  }, {
    selector: ".black-text",
    familyName: "bar",
    name: "Ostrich Sans Black",
    remote: true,
    url: URL_ROOT + "ostrich-black.ttf",
  }];

  for (let i = 0; i < FONTS.length; i++) {
    await selectNode(FONTS[i].selector, inspector);
    const lis = getUsedFontsEls(viewDoc);
    const li = lis[0];
    const font = FONTS[i];

    is(lis.length, 1, `Found 1 font on ${FONTS[i].selector}`);
    is(getName(li), font.name, "The DIV font has the right name");
    is(getFamilyName(li), font.familyName, `font has the right family name`);
    is(isRemote(li), font.remote, `font remote value correct`);
    is(getURL(li), font.url, `font url correct`);
  }
}
