/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { svg, polygon } from "react-dom-factories";
import PropTypes from "prop-types";

function shorten(coordinates) {
  // In cases where the token is wider than the preview, the smartGap
  // gets distorted. This shortens the coordinate array so that the smartGap
  // is only touching 2 corners of the token (instead of all 4 corners)
  coordinates.splice(0, 2);
  coordinates.splice(4, 2);
  return coordinates;
}

function getSmartGapCoordinates(
  preview,
  token,
  offset,
  orientation,
  gapHeight,
  coords
) {
  if (orientation === "up") {
    const coordinates = [
      token.left - coords.left + offset,
      token.top + token.height - (coords.top + preview.height) + gapHeight,
      0,
      0,
      preview.width + offset,
      0,
      token.left + token.width - coords.left + offset,
      token.top + token.height - (coords.top + preview.height) + gapHeight,
      token.left + token.width - coords.left + offset,
      token.top - (coords.top + preview.height) + gapHeight,
      token.left - coords.left + offset,
      token.top - (coords.top + preview.height) + gapHeight,
    ];
    return preview.width > token.width ? coordinates : shorten(coordinates);
  }
  if (orientation === "down") {
    const coordinates = [
      token.left + token.width - (coords.left + preview.top) + offset,
      0,
      preview.width + offset,
      coords.top - token.top + gapHeight,
      0,
      coords.top - token.top + gapHeight,
      token.left - (coords.left + preview.top) + offset,
      0,
      token.left - (coords.left + preview.top) + offset,
      token.height,
      token.left + token.width - (coords.left + preview.top) + offset,
      token.height,
    ];
    return preview.width > token.width ? coordinates : shorten(coordinates);
  }
  return [
    0,
    token.top - coords.top,
    gapHeight + token.width,
    0,
    gapHeight + token.width,
    preview.height - gapHeight,
    0,
    token.top + token.height - coords.top,
    token.width,
    token.top + token.height - coords.top,
    token.width,
    token.top - coords.top,
  ];
}

function getSmartGapDimensions(
  previewRect,
  tokenRect,
  offset,
  orientation,
  gapHeight,
  coords
) {
  if (orientation === "up") {
    return {
      height:
        tokenRect.top +
        tokenRect.height -
        coords.top -
        previewRect.height +
        gapHeight,
      width: Math.max(previewRect.width, tokenRect.width) + offset,
    };
  }
  if (orientation === "down") {
    return {
      height: coords.top - tokenRect.top + gapHeight,
      width: Math.max(previewRect.width, tokenRect.width) + offset,
    };
  }
  return {
    height: previewRect.height - gapHeight,
    width: coords.left - tokenRect.left + gapHeight,
  };
}

export default function SmartGap({
  token,
  preview,
  type,
  gapHeight,
  coords,
  offset,
}) {
  const tokenRect = token.getBoundingClientRect();
  const previewRect = preview.getBoundingClientRect();
  const { orientation } = coords;
  let optionalMarginLeft, optionalMarginTop;

  if (orientation === "down") {
    optionalMarginTop = -tokenRect.height;
  } else if (orientation === "right") {
    optionalMarginLeft = -tokenRect.width;
  }

  const { height, width } = getSmartGapDimensions(
    previewRect,
    tokenRect,
    -offset,
    orientation,
    gapHeight,
    coords
  );
  const coordinates = getSmartGapCoordinates(
    previewRect,
    tokenRect,
    -offset,
    orientation,
    gapHeight,
    coords
  );
  return svg(
    {
      version: "1.1",
      xmlns: "http://www.w3.org/2000/svg",
      style: {
        height,
        width,
        position: "absolute",
        marginLeft: optionalMarginLeft,
        marginTop: optionalMarginTop,
      },
    },
    polygon({
      points: coordinates,
      fill: "transparent",
    })
  );
}

SmartGap.propTypes = {
  coords: PropTypes.object.isRequired,
  gapHeight: PropTypes.number.isRequired,
  offset: PropTypes.number.isRequired,
  preview: PropTypes.object.isRequired,
  token: PropTypes.object.isRequired,
  type: PropTypes.oneOf(["popover", "tooltip"]).isRequired,
};
