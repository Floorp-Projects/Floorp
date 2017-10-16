/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  setImageTooltip,
  getImageDimensions,
} = require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper");
const { formDataURI } = require("./utils/request-utils");

const REQUESTS_TOOLTIP_IMAGE_MAX_DIM = 400; // px

async function setTooltipImageContent(connector, tooltip, itemEl, requestItem) {
  let { mimeType, text, encoding } = requestItem.responseContent.content;

  if (!mimeType || !mimeType.includes("image/")) {
    return false;
  }

  let string = await connector.getLongString(text);
  let src = formDataURI(mimeType, encoding, string);
  let maxDim = REQUESTS_TOOLTIP_IMAGE_MAX_DIM;
  let { naturalWidth, naturalHeight } = await getImageDimensions(tooltip.doc, src);
  let options = { maxDim, naturalWidth, naturalHeight };
  setImageTooltip(tooltip, tooltip.doc, src, options);

  return itemEl.querySelector(".requests-list-icon");
}

module.exports = {
  setTooltipImageContent,
};
