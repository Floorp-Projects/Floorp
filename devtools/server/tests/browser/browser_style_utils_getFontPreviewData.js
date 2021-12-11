/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that getFontPreviewData of the style utils generates font previews.

const TEST_URI = "data:text/html,<title>Test getFontPreviewData</title>";

add_task(async function() {
  await addTab(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const {
      getFontPreviewData,
    } = require("devtools/server/actors/utils/style-utils");

    const font = Services.appinfo.OS === "WINNT" ? "Arial" : "Liberation Sans";
    let fontPreviewData = getFontPreviewData(font, content.document);
    ok(
      fontPreviewData?.dataURL,
      "Returned a font preview with a valid dataURL"
    );

    // Create <img> element and load the generated preview into it
    // to check whether the image is valid and get its dimensions
    const image = content.document.createElement("img");
    let imageLoaded = new Promise(loaded =>
      image.addEventListener("load", loaded, { once: true })
    );
    image.src = fontPreviewData.dataURL;
    await imageLoaded;

    const { naturalWidth: widthImage1, naturalHeight: heightImage1 } = image;

    ok(widthImage1 > 0, "Preview width is greater than 0");
    ok(heightImage1 > 0, "Preview height is greater than 0");

    // Create a preview with different text and compare
    // its dimensions with the first one
    fontPreviewData = getFontPreviewData(font, content.document, {
      previewText: "Abcdef",
    });

    ok(
      fontPreviewData?.dataURL,
      "Returned a font preview with a valid dataURL"
    );

    imageLoaded = new Promise(loaded =>
      image.addEventListener("load", loaded, { once: true })
    );
    image.src = fontPreviewData.dataURL;
    await imageLoaded;

    const { naturalWidth: widthImage2, naturalHeight: heightImage2 } = image;

    // Check whether the width is greater than with the default parameters
    // and that the height is the same
    ok(
      widthImage2 > widthImage1,
      "Preview width is greater than with default parameters"
    );
    ok(
      heightImage2 === heightImage1,
      "Preview height is the same as with default parameters"
    );

    // Create a preview with smaller font size and compare
    // its dimensions with the first one
    fontPreviewData = getFontPreviewData(font, content.document, {
      previewFontSize: 20,
    });

    ok(
      fontPreviewData?.dataURL,
      "Returned a font preview with a valid dataURL"
    );

    imageLoaded = new Promise(loaded =>
      image.addEventListener("load", loaded, { once: true })
    );
    image.src = fontPreviewData.dataURL;
    await imageLoaded;

    const { naturalWidth: widthImage3, naturalHeight: heightImage3 } = image;

    // Check whether the width and height are smaller than with the default parameters
    ok(
      widthImage3 < widthImage1,
      "Preview width is smaller than with default parameters"
    );
    ok(
      heightImage3 < heightImage1,
      "Preview height is smaller than with default parameters"
    );

    // Create a preview with multiple lines and compare
    // its dimensions with the first one
    fontPreviewData = getFontPreviewData(font, content.document, {
      previewText: "Abc\ndef",
    });

    ok(
      fontPreviewData?.dataURL,
      "Returned a font preview with a valid dataURL"
    );

    imageLoaded = new Promise(loaded =>
      image.addEventListener("load", loaded, { once: true })
    );
    image.src = fontPreviewData.dataURL;
    await imageLoaded;

    const { naturalWidth: widthImage4, naturalHeight: heightImage4 } = image;

    // Check whether the width is the same as with the default parameters
    // and that the height is greater
    ok(
      widthImage4 === widthImage1,
      "Preview width is the same as with default parameters"
    );
    ok(
      heightImage4 > heightImage1,
      "Preview height is greater than with default parameters"
    );
  });
});
