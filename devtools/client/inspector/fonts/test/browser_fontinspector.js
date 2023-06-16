/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

const TEST_URI = URL_ROOT + "doc_browser_fontinspector.html";

add_task(async function () {
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  ok(!!view, "Font inspector document is alive.");

  const viewDoc = view.document;

  await testBodyFonts(inspector, viewDoc);
  await testDivFonts(inspector, viewDoc);
});

async function testBodyFonts(inspector, viewDoc) {
  const FONTS = [
    {
      familyName: "bar",
      name: ["Ostrich Sans Medium", "Ostrich Sans Black"],
    },
    {
      familyName: "barnormal",
      name: "Ostrich Sans Medium",
    },
    {
      // On Linux, Arial does not exist. Liberation Sans is used instead.
      familyName: ["Arial", "Liberation Sans"],
      name: ["Arial", "Liberation Sans"],
    },
  ];

  await selectNode("body", inspector);

  const groups = getUsedFontGroupsEls(viewDoc);
  is(groups.length, 3, "Found 3 font families used on BODY");

  for (let i = 0; i < FONTS.length; i++) {
    const groupEL = groups[i];
    const font = FONTS[i];

    const familyName = getFamilyName(groupEL);
    ok(
      font.familyName.includes(familyName),
      `Font families used on BODY include: ${familyName}`
    );

    const fontName = getName(groupEL);
    ok(font.name.includes(fontName), `Fonts used on BODY include: ${fontName}`);
  }
}

async function testDivFonts(inspector, viewDoc) {
  const FONTS = [
    {
      selector: "div",
      familyName: "bar",
      name: "Ostrich Sans Medium",
    },
    {
      selector: ".normal-text",
      familyName: "barnormal",
      name: "Ostrich Sans Medium",
    },
    {
      selector: ".bold-text",
      familyName: "bar",
      name: "Ostrich Sans Black",
    },
    {
      selector: ".black-text",
      familyName: "bar",
      name: "Ostrich Sans Black",
    },
  ];

  for (let i = 0; i < FONTS.length; i++) {
    await selectNode(FONTS[i].selector, inspector);
    const groups = getUsedFontGroupsEls(viewDoc);
    const groupEl = groups[0];
    const font = FONTS[i];

    is(groups.length, 1, `Found 1 font on ${FONTS[i].selector}`);
    is(getName(groupEl), font.name, "The DIV font has the right name");
    is(
      getFamilyName(groupEl),
      font.familyName,
      `font has the right family name`
    );
  }
}
