/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check that the font editor has a section for "All fonts" which shows all fonts
// used on the page.

const TEST_URI = URL_ROOT_SSL + "doc_browser_fontinspector.html";

add_task(async function () {
  const { view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const allFontsAccordion = getFontsAccordion(viewDoc);
  ok(allFontsAccordion, "There's an accordion in the panel");
  is(
    allFontsAccordion.textContent,
    "All Fonts on Page",
    "It has the right title"
  );

  await expandAccordion(allFontsAccordion);
  const allFontsEls = getAllFontsEls(viewDoc);

  const FONTS = [
    {
      familyName: ["bar"],
      name: ["Ostrich Sans Medium"],
      remote: true,
      url: URL_ROOT_SSL + "ostrich-regular.ttf",
    },
    {
      familyName: ["bar"],
      name: ["Ostrich Sans Black"],
      remote: true,
      url: URL_ROOT_SSL + "ostrich-black.ttf",
    },
    {
      familyName: ["bar"],
      name: ["Ostrich Sans Black"],
      remote: true,
      url: URL_ROOT_SSL + "ostrich-black.ttf",
    },
    {
      familyName: ["barnormal"],
      name: ["Ostrich Sans Medium"],
      remote: true,
      url: URL_ROOT_SSL + "ostrich-regular.ttf",
    },
    {
      // On Linux, Arial does not exist. Liberation Sans is used instead.
      familyName: ["Arial", "Liberation Sans"],
      name: ["Arial", "Liberation Sans"],
      remote: false,
      url: "system",
    },
    {
      // On Linux, Times New Roman does not exist. Liberation Serif is used instead.
      familyName: ["Times New Roman", "Liberation Serif"],
      name: ["Times New Roman", "Liberation Serif"],
      remote: false,
      url: "system",
    },
  ];

  is(allFontsEls.length, FONTS.length, "All fonts used are listed");

  for (let i = 0; i < FONTS.length; i++) {
    const li = allFontsEls[i];
    const font = FONTS[i];

    ok(font.name.includes(getName(li)), "The DIV font has the right name");
    info(getName(li));
    ok(
      font.familyName.includes(getFamilyName(li)),
      `font has the right family name`
    );
    info(getFamilyName(li));
    is(isRemote(li), font.remote, `font remote value correct`);
    is(getURL(li), font.url, `font url correct`);
  }
});
