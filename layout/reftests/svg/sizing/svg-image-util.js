/**
 * Reusable SVG data uri prefix for generated SVGs.
 */
const SVG_DATA_URI_PREFIX = 'data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg"';
/**
 * Reusable SVG data uri suffix for generated SVGs.
 */
const SVG_DATA_URI_SUFFIX = '><rect fill="brown" x="-2000" y="-2000" width="4000" height="4000"></rect><rect fill="green" height="50" width="50"></rect><rect fill="yellow" x="10" y="10" height="30" width="30"></rect></svg>';

/**
 * Generates full data URI for an SVG document, with the given parameters
 * on the SVG element.
 *
 * @param aWidth    The width attribute, or null for no width.
 * @param aHeight   The height attribute, or null for no height.
 * @param aViewbox  The viewBox attribute, or null for no viewBox.
 */
function generateSVGDataURI(aWidth, aHeight, aViewBox) {
  let datauri = SVG_DATA_URI_PREFIX;

  if (aWidth) {
    datauri += " width=\"" + aWidth + "\""
  }

  if (aHeight) {
    datauri += " height=\"" + aHeight + "\"";
  }

  if (aViewBox) {
    datauri += " viewBox=\"" + aViewBox + "\"";
  }

  datauri += SVG_DATA_URI_SUFFIX;

  return datauri;
}

/**
 * Generates a canvas, with the given width and height parameters, and uses
 * CanvasRenderingContext2D.drawImage() to draw a SVG image with the given
 * width and height attributes.
 *
 * @param aCanvasWidth      The width attribute of the canvas.
 * @param aCanvasHeight     The height attribute of the canvas.
 * @param aSVGWidth         The width attribute of the svg, or null for no width.
 * @param aSVGHeight        The height attribute of the svg, or null for no height.
 * @param aSVGViewBox       The viewBox attribute sf the svg, or null for no viewBox.
 *
 * @returns A promise that resolves when the SVG image has been drawn to the
 *          created canvas
 */
async function generateCanvasDrawImageSVG(aCanvasWidth, aCanvasHeight, aSVGWidth,
                                          aSVGHeight, aSVGViewBox) {
    let canvas = document.createElement("canvas");
    let ctx = canvas.getContext("2d");
    document.body.appendChild(canvas);

    canvas.setAttribute("width", aCanvasWidth);
    canvas.setAttribute("height", aCanvasHeight);

    let img = document.createElement("img");

    let promise = new Promise(resolve => {
      img.onload = () => {
        ctx.drawImage(img, 0, 0);
        resolve();
      };
    });

    let uri = generateSVGDataURI(aSVGWidth, aSVGHeight, aSVGViewBox);
    img.src = uri;

    return promise;
}
