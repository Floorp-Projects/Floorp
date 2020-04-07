
"use strict";

const SVG_IMG =
  `<svg width="200" height="200" viewBox="0 0 150 150" xmlns="http://www.w3.org/2000/svg">
   <style>
     circle {
       fill: orange;
       stroke: black;
       stroke-width: 10px;
     }
   </style>
   <circle cx="50" cy="50" r="40" />
   </svg>`;

const SVG_IMG_NO_INLINE_STYLE =
  `<svg width="200" height="200" viewBox="0 0 150 150" xmlns="http://www.w3.org/2000/svg">
   <circle cx="50" cy="50" r="40" />
   </svg>`;

function handleRequest(request, response) {
  const query = request.queryString;

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "image/svg+xml", false);

  if (query === "svg_inline_style_csp") {
    response.setHeader("Content-Security-Policy", "default-src 'none'", false);
    response.write(SVG_IMG);
    return;
  }

  if (query === "svg_inline_style") {
    response.write(SVG_IMG);
    return;
  }

  if (query === "svg_no_inline_style") {
    response.write(SVG_IMG_NO_INLINE_STYLE);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
