function addPoint(aSVGElement, aPolylineElement, aX, aY) {
  var pt = aSVGElement.createSVGPoint();
  pt.x = aX;
  pt.y = aY;
  aPolylineElement.points.appendItem(pt);
}

function round(aFloat)
{
  return aFloat >= 0.0 ? Math.floor(aFloat + 0.5) : Math.ceil(aFloat - 0.5);
}

const kDecorationStyleNone   = 0;
const kDecorationStyleSolid  = 1;
const kDecorationStyleDotted = 2;
const kDecorationStyleDashed = 3;
const kDecorationStyleDouble = 4;
const kDecorationStyleWavy   = 5;

const kDotLength  = 1.0;
const kDashLength = 3.0;

const kSVGNS = "http://www.w3.org/2000/svg";

// XXX following functions only support to draw underline now.

function drawDecorationLine(aDocument, aColor, aPt, aLineSize, aAscent, aOffset,
                            aStyle, aDescentLimit)
{
  var rect = getTextDecorationRect(aPt, aLineSize, aAscent, aOffset, aStyle,
                                   aDescentLimit);
  if (rect.width == 0 || rect.height == 0)
    return;

  var root = aDocument.documentElement;
  var container = aDocument.createElementNS(kSVGNS, "svg");
  root.appendChild(container);

  var line1 = aDocument.createElementNS(kSVGNS, "polyline");
  var line2;

  var style = "position: absolute;";
  style += "left: " + rect.x + "px;";
  style += "top: " + rect.y + "px;";
  style += "width: " + rect.width + "px;";
  style += "height: " + rect.height + "px;";
  container.setAttribute("style", style);
  rect.x = rect.y = 0;

  var lineHeight = Math.max(round(aLineSize.height), 1.0);

  switch (aStyle) {
    case kDecorationStyleDouble:
      line2 = aDocument.createElementNS(kSVGNS, "polyline");
      container.appendChild(line2);
    case kDecorationStyleSolid:
      container.appendChild(line1);
      break;
    case kDecorationStyleDashed:
      container.appendChild(line1);
      var dashWidth = lineHeight * kDotLength * kDashLength;
      var dash = "stroke-dasharray: " + dashWidth + ", " + dashWidth + ";";
      var lineCap = "stroke-linecap: butt;"
      line1.setAttribute("style", dash + lineCap);
      rect.width += dashWidth;
      break;
    case kDecorationStyleDotted:
      container.appendChild(line1);
      var dashWidth = lineHeight * kDotLength;
      var dash = "stroke-dasharray: ";
      var lineCap = "";
      if (lineHeight > 2.0) {
        dash += "0.0, " + dashWidth * 2.0 + ";";
        lineCap = "stroke-linecap: round;";
      } else {
        dash += dashWidth + ", " + dashWidth + ";";
      }
      rect.width += dashWidth;
      line1.setAttribute("style", dash + lineCap);
      break;
    case kDecorationStyleWavy:
      container.appendChild(line1);
      if (lineHeight > 2.0) {
        //
      } else {
        line1.setAttribute("shape-rendering", "optimizeSpeed");
      }
      break;
  }

  rect.y += lineHeight / 2;

  line1.setAttribute("fill", "none");
  line1.setAttribute("stroke", aColor);
  line1.setAttribute("stroke-width", lineHeight);
  if (line2) {
    line2.setAttribute("fill", "none");
    line2.setAttribute("stroke", aColor);
    line2.setAttribute("stroke-width", lineHeight);
  }

  switch (aStyle) {
    case kDecorationStyleSolid:
      addPoint(container, line1, rect.x, rect.y);
      addPoint(container, line1, rect.x + rect.width, rect.y);
      break;
    case kDecorationStyleDouble:
      addPoint(container, line1, rect.x, rect.y);
      addPoint(container, line1, rect.x + rect.width, rect.y);
      rect.height -= lineHeight;
      addPoint(container, line2, rect.x, rect.y + rect.height);
      addPoint(container, line2, rect.x + rect.width, rect.y + rect.height);
      break;
    case kDecorationStyleDotted:
    case kDecorationStyleDashed:
      addPoint(container, line1, rect.x, rect.y);
      addPoint(container, line1, rect.x + rect.width, rect.y);
      break;
    case kDecorationStyleWavy:
      rect.x += lineHeight / 2.0;

      var pt = { x: rect.x, y: rect.y };
      var rightMost = pt.x + rect.width + lineHeight;
      var adv = rect.height - lineHeight;
      var flatLengthAtVertex = Math.max((lineHeight - 1.0) * 2.0, 1.0);

      var points = "";

      pt.x -= lineHeight;
      addPoint(container, line1, pt.x, pt.y);

      pt.x = rect.x;
      addPoint(container, line1, pt.x, pt.y);

      var goDown = true;
      while (pt.x < rightMost) {
        pt.x += adv;
        pt.y += goDown ? adv : -adv;

        addPoint(container, line1, pt.x, pt.y);

        pt.x += flatLengthAtVertex;
        addPoint(container, line1, pt.x, pt.y);

        goDown = !goDown;
      }
      break;
  }
}

function getTextDecorationRect(aPt, aLineSize, aAscent, aOffset, aStyle,
                               aDescentLimit)
{
  if (aStyle == kDecorationStyleNone)
    return { x: 0, y: 0, width: 0, height: 0 };

  var liftupUnderline = aDescentLimit >= 0.0;

  var r = {};
  r.x = Math.floor(aPt.x + 0.5);
  r.width = round(aLineSize.width);

  var lineHeight = round(aLineSize.height);
  lineHeight = Math.max(lineHeight, 1.0);

  var ascent = round(aAscent);
  var descentLimit = Math.floor(aDescentLimit);

  var suggestedMaxRectHeight = Math.max(Math.min(ascent, descentLimit), 1.0);
  r.height = lineHeight;
  if (aStyle == kDecorationStyleDouble) {
    var gap = round(lineHeight / 2.0);
    gap = Math.max(gap, 1.0);
    r.height = lineHeight * 2.0 + gap;
    if (liftupUnderline) {
      if (r.height > suggestedMaxRectHeight) {
        r.height = Math.max(suggestedMaxRectHeight, lineHeight * 2.0 + 1.0);
      }
    }
  } else if (aStyle == kDecorationStyleWavy) {
    r.height = lineHeight > 2.0 ? lineHeight * 4.0 : lineHeight * 3.0;
    if (liftupUnderline) {
      if (r.height > suggestedMaxRectHeight) {
        r.height = Math.max(suggestedMaxRectHeight, lineHeight * 2.0);
      }
    }
  }

  var baseline = Math.floor(aPt.y + aAscent + 0.5);
  var offset = 0.0;

  offset = aOffset;
  if (liftupUnderline) {
    if (descentLimit < -offset + r.height) {
      var offsetBottomAligned = -descentLimit + r.height;
      var offsetTopAligned = 0.0;
      offset = Math.min(offsetBottomAligned, offsetTopAligned);
    }
  }

  r.y = baseline - Math.floor(offset + 0.5);

  return r;
}

