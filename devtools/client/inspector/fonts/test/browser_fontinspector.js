/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

const TEST_URI = URL_ROOT + "browser_fontinspector.html";
const FONTS = [{
  name: "Ostrich Sans Medium",
  remote: true,
  url: URL_ROOT + "ostrich-regular.ttf",
  cssName: "bar"
}, {
  name: "Ostrich Sans Black",
  remote: true,
  url: URL_ROOT + "ostrich-black.ttf",
  cssName: "bar"
}, {
  name: "Ostrich Sans Black",
  remote: true,
  url: URL_ROOT + "ostrich-black.ttf",
  cssName: "bar"
}, {
  name: "Ostrich Sans Medium",
  remote: true,
  url: URL_ROOT + "ostrich-regular.ttf",
  cssName: "barnormal"
}];

add_task(async function() {
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  ok(!!view, "Font inspector document is alive.");

  const viewDoc = view.document;

  await testBodyFonts(inspector, viewDoc);
  await testDivFonts(inspector, viewDoc);
});

function isRemote(fontLi) {
  return fontLi.querySelector(".font-origin").classList.contains("remote");
}

function testBodyFonts(inspector, viewDoc) {
  const lis = getUsedFontsEls(viewDoc);
  is(lis.length, 5, "Found 5 fonts");

  for (let i = 0; i < FONTS.length; i++) {
    const li = lis[i];
    const font = FONTS[i];

    is(getName(li), font.name, `font ${i} right font name`);
    is(isRemote(li), font.remote, `font ${i} remote value correct`);
    is(li.querySelector(".font-origin").textContent, font.url, `font ${i} url correct`);
  }

  // test that the bold and regular fonts have different previews
  const regSrc = lis[0].querySelector(".font-preview").src;
  const boldSrc = lis[1].querySelector(".font-preview").src;
  isnot(regSrc, boldSrc, "preview for bold font is different from regular");

  // test system font
  const localFontName = getName(lis[4]);

  // On Linux test machines, the Arial font doesn't exist.
  // The fallback is "Liberation Sans"
  ok((localFontName == "Arial") || (localFontName == "Liberation Sans"),
     "local font right font name");
  ok(!isRemote(lis[4]), "local font is local");
}

async function testDivFonts(inspector, viewDoc) {
  const updated = inspector.once("fontinspector-updated");
  await selectNode("div", inspector);
  await updated;

  const lis = getUsedFontsEls(viewDoc);
  is(lis.length, 1, "Found 1 font on DIV");
  is(getName(lis[0]), "Ostrich Sans Medium", "The DIV font has the right name");
}
