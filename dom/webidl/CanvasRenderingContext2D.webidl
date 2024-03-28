/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

enum CanvasWindingRule { "nonzero", "evenodd" };

enum CanvasLineCap { "butt", "round", "square" };
enum CanvasLineJoin { "round", "bevel", "miter" };
enum CanvasTextAlign { "start", "end", "left", "right", "center" };
enum CanvasTextBaseline { "top", "hanging", "middle", "alphabetic", "ideographic", "bottom" };
enum CanvasDirection { "ltr", "rtl", "inherit" };
enum CanvasFontKerning { "auto", "normal", "none" };
enum CanvasFontStretch { "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "normal", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded" };
enum CanvasFontVariantCaps { "normal", "small-caps", "all-small-caps", "petite-caps", "all-petite-caps", "unicase", "titling-caps" };
enum CanvasTextRendering { "auto", "optimizeSpeed", "optimizeLegibility", "geometricPrecision" };

[GenerateInit]
dictionary CanvasRenderingContext2DSettings {
  // signal if the canvas contains an alpha channel
  boolean alpha = true;

  boolean desynchronized = false;

  PredefinedColorSpace colorSpace = "srgb";

  // whether or not we're planning to do a lot of readback operations
  boolean willReadFrequently = false;
};

dictionary HitRegionOptions {
  Path2D? path = null;
  DOMString id = "";
  Element? control = null;
};

typedef (HTMLImageElement or
         SVGImageElement) HTMLOrSVGImageElement;

typedef (HTMLOrSVGImageElement or
         HTMLCanvasElement or
         HTMLVideoElement or
         OffscreenCanvas or
         ImageBitmap or
         VideoFrame) CanvasImageSource;

[Exposed=Window]
interface CanvasRenderingContext2D {

  // back-reference to the canvas.  Might be null if we're not
  // associated with a canvas.
  readonly attribute HTMLCanvasElement? canvas;

  CanvasRenderingContext2DSettings getContextAttributes();

  // Show the caret if appropriate when drawing
  [Func="CanvasUtils::HasDrawWindowPrivilege"]
  const unsigned long DRAWWINDOW_DRAW_CARET   = 0x01;
  // Don't flush pending layout notifications that could otherwise
  // be batched up
  [Func="CanvasUtils::HasDrawWindowPrivilege"]
  const unsigned long DRAWWINDOW_DO_NOT_FLUSH = 0x02;
  // Draw scrollbars and scroll the viewport if they are present
  [Func="CanvasUtils::HasDrawWindowPrivilege"]
  const unsigned long DRAWWINDOW_DRAW_VIEW    = 0x04;
  // Use the widget layer manager if available. This means hardware
  // acceleration may be used, but it might actually be slower or
  // lower quality than normal. It will however more accurately reflect
  // the pixels rendered to the screen.
  [Func="CanvasUtils::HasDrawWindowPrivilege"]
  const unsigned long DRAWWINDOW_USE_WIDGET_LAYERS = 0x08;
  // Don't synchronously decode images - draw what we have
  [Func="CanvasUtils::HasDrawWindowPrivilege"]
  const unsigned long DRAWWINDOW_ASYNC_DECODE_IMAGES = 0x10;

  /**
   * Renders a region of a window into the canvas.  The contents of
   * the window's viewport are rendered, ignoring viewport clipping
   * and scrolling.
   *
   * @param x
   * @param y
   * @param w
   * @param h specify the area of the window to render, in CSS
   * pixels.
   *
   * @param backgroundColor the canvas is filled with this color
   * before we render the window into it. This color may be
   * transparent/translucent. It is given as a CSS color string
   * (e.g., rgb() or rgba()).
   *
   * @param flags Used to better control the drawWindow call.
   * Flags can be ORed together.
   *
   * Of course, the rendering obeys the current scale, transform and
   * globalAlpha values.
   *
   * Hints:
   * -- If 'rgba(0,0,0,0)' is used for the background color, the
   * drawing will be transparent wherever the window is transparent.
   * -- Top-level browsed documents are usually not transparent
   * because the user's background-color preference is applied,
   * but IFRAMEs are transparent if the page doesn't set a background.
   * -- If an opaque color is used for the background color, rendering
   * will be faster because we won't have to compute the window's
   * transparency.
   *
   * This API cannot currently be used by Web content. It is chrome
   * and Web Extensions (with a permission) only.
   */
  [Throws, NeedsSubjectPrincipal, Func="CanvasUtils::HasDrawWindowPrivilege"]
  undefined drawWindow(Window window, double x, double y, double w, double h,
                       UTF8String bgColor, optional unsigned long flags = 0);

  /**
   * This causes a context that is currently using a hardware-accelerated
   * backend to fallback to a software one. All state should be preserved.
   */
  [ChromeOnly]
  undefined demote();
};

CanvasRenderingContext2D includes CanvasState;
CanvasRenderingContext2D includes CanvasTransform;
CanvasRenderingContext2D includes CanvasCompositing;
CanvasRenderingContext2D includes CanvasImageSmoothing;
CanvasRenderingContext2D includes CanvasFillStrokeStyles;
CanvasRenderingContext2D includes CanvasShadowStyles;
CanvasRenderingContext2D includes CanvasFilters;
CanvasRenderingContext2D includes CanvasRect;
CanvasRenderingContext2D includes CanvasDrawPath;
CanvasRenderingContext2D includes CanvasUserInterface;
CanvasRenderingContext2D includes CanvasText;
CanvasRenderingContext2D includes CanvasDrawImage;
CanvasRenderingContext2D includes CanvasImageData;
CanvasRenderingContext2D includes CanvasPathDrawingStyles;
CanvasRenderingContext2D includes CanvasTextDrawingStyles;
CanvasRenderingContext2D includes CanvasPathMethods;


interface mixin CanvasState {
  // state
  undefined save(); // push state on state stack
  undefined restore(); // pop state stack and restore state
  undefined reset(); // reset the rendering context to its default state
};

interface mixin CanvasTransform {
  // transformations (default transform is the identity matrix)
  [Throws, LenientFloat]
  undefined scale(double x, double y);
  [Throws, LenientFloat]
  undefined rotate(double angle);
  [Throws, LenientFloat]
  undefined translate(double x, double y);
  [Throws, LenientFloat]
  undefined transform(double a, double b, double c, double d, double e, double f);

  [NewObject, Throws] DOMMatrix getTransform();
  [Throws, LenientFloat]
  undefined setTransform(double a, double b, double c, double d, double e, double f);
  [Throws]
  undefined setTransform(optional DOMMatrix2DInit transform = {});
  [Throws]
  undefined resetTransform();
};

interface mixin CanvasCompositing {
  attribute unrestricted double globalAlpha; // (default 1.0)
  [Throws]
  attribute DOMString globalCompositeOperation; // (default source-over)
};

interface mixin CanvasImageSmoothing {
  // drawing images
  attribute boolean imageSmoothingEnabled;
};

interface mixin CanvasFillStrokeStyles {
  // colors and styles (see also the CanvasPathDrawingStyles interface)
  attribute (UTF8String or CanvasGradient or CanvasPattern) strokeStyle; // (default black)
  attribute (UTF8String or CanvasGradient or CanvasPattern) fillStyle; // (default black)
  [NewObject]
  CanvasGradient createLinearGradient(double x0, double y0, double x1, double y1);
  [NewObject, Throws]
  CanvasGradient createRadialGradient(double x0, double y0, double r0, double x1, double y1, double r1);
  [NewObject]
  CanvasGradient createConicGradient(double angle, double cx, double cy);
  [NewObject, Throws]
  CanvasPattern? createPattern(CanvasImageSource image, [LegacyNullToEmptyString] DOMString repetition);
};

interface mixin CanvasShadowStyles {
  [LenientFloat]
  attribute double shadowOffsetX; // (default 0)
  [LenientFloat]
  attribute double shadowOffsetY; // (default 0)
  [LenientFloat]
  attribute double shadowBlur; // (default 0)
  attribute UTF8String shadowColor; // (default transparent black)
};

interface mixin CanvasFilters {
  [SetterThrows]
  attribute UTF8String filter; // (default empty string = no filter)
};

interface mixin CanvasRect {
  [LenientFloat]
  undefined clearRect(double x, double y, double w, double h);
  [LenientFloat]
  undefined fillRect(double x, double y, double w, double h);
  [LenientFloat]
  undefined strokeRect(double x, double y, double w, double h);
};

interface mixin CanvasDrawPath {
  // path API (see also CanvasPathMethods)
  undefined beginPath();
  undefined fill(optional CanvasWindingRule winding = "nonzero");
  undefined fill(Path2D path, optional CanvasWindingRule winding = "nonzero");
  undefined stroke();
  undefined stroke(Path2D path);
  undefined clip(optional CanvasWindingRule winding = "nonzero");
  undefined clip(Path2D path, optional CanvasWindingRule winding = "nonzero");
// NOT IMPLEMENTED  undefined resetClip();
  [NeedsSubjectPrincipal]
  boolean isPointInPath(unrestricted double x, unrestricted double y, optional CanvasWindingRule winding = "nonzero");
  [NeedsSubjectPrincipal] // Only required because overloads can't have different extended attributes.
  boolean isPointInPath(Path2D path, unrestricted double x, unrestricted double y, optional CanvasWindingRule winding = "nonzero");
  [NeedsSubjectPrincipal]
  boolean isPointInStroke(double x, double y);
  [NeedsSubjectPrincipal] // Only required because overloads can't have different extended attributes.
  boolean isPointInStroke(Path2D path, unrestricted double x, unrestricted double y);
};

interface mixin CanvasUserInterface {
  [Throws] undefined drawFocusIfNeeded(Element element);
// NOT IMPLEMENTED  undefined scrollPathIntoView();
// NOT IMPLEMENTED  undefined scrollPathIntoView(Path path);
};

interface mixin CanvasText {
  // text (see also the CanvasPathDrawingStyles interface)
  [Throws, LenientFloat]
  undefined fillText(DOMString text, double x, double y, optional double maxWidth);
  [Throws, LenientFloat]
  undefined strokeText(DOMString text, double x, double y, optional double maxWidth);
  [NewObject, Throws]
  TextMetrics measureText(DOMString text);
};

interface mixin CanvasDrawImage {
  [Throws, LenientFloat]
  undefined drawImage(CanvasImageSource image, double dx, double dy);
  [Throws, LenientFloat]
  undefined drawImage(CanvasImageSource image, double dx, double dy, double dw, double dh);
  [Throws, LenientFloat]
  undefined drawImage(CanvasImageSource image, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh);
};

// See https://github.com/whatwg/html/issues/6262 for [EnforceRange] usage.
interface mixin CanvasImageData {
  // pixel manipulation
  [NewObject, Throws]
  ImageData createImageData([EnforceRange] long sw, [EnforceRange] long sh);
  [NewObject, Throws]
  ImageData createImageData(ImageData imagedata);
  [NewObject, Throws, NeedsSubjectPrincipal]
  ImageData getImageData([EnforceRange] long sx, [EnforceRange] long sy, [EnforceRange] long sw, [EnforceRange] long sh);
  [Throws]
  undefined putImageData(ImageData imagedata, [EnforceRange] long dx, [EnforceRange] long dy);
  [Throws]
  undefined putImageData(ImageData imagedata, [EnforceRange] long dx, [EnforceRange] long dy, [EnforceRange] long dirtyX, [EnforceRange] long dirtyY, [EnforceRange] long dirtyWidth, [EnforceRange] long dirtyHeight);
};

interface mixin CanvasPathDrawingStyles {
  // line caps/joins
  [LenientFloat]
  attribute double lineWidth; // (default 1)
  attribute CanvasLineCap lineCap; // (default "butt")
  attribute CanvasLineJoin lineJoin; // (default "miter")
  [LenientFloat]
  attribute double miterLimit; // (default 10)

  // dashed lines
  [LenientFloat, Throws] undefined setLineDash(sequence<double> segments); // default empty
  sequence<double> getLineDash();
  [LenientFloat] attribute double lineDashOffset;
};

interface mixin CanvasTextDrawingStyles {
  // text
  [SetterThrows]
  attribute UTF8String font; // (default 10px sans-serif)
  attribute CanvasTextAlign textAlign; // (default: "start")
  attribute CanvasTextBaseline textBaseline; // (default: "alphabetic")
  attribute CanvasDirection direction; // (default: "inherit")
  attribute UTF8String letterSpacing; // default: "0px"
  attribute CanvasFontKerning fontKerning; // (default: "auto")
  attribute CanvasFontStretch fontStretch; // (default: "normal")
  attribute CanvasFontVariantCaps fontVariantCaps; // (default: "normal")
  attribute CanvasTextRendering textRendering; // (default: "auto")
  attribute UTF8String wordSpacing; // default: "0px"
};

interface mixin CanvasPathMethods {
  // shared path API methods
  undefined closePath();
  [LenientFloat]
  undefined moveTo(double x, double y);
  [LenientFloat]
  undefined lineTo(double x, double y);
  [LenientFloat]
  undefined quadraticCurveTo(double cpx, double cpy, double x, double y);

  [LenientFloat]
  undefined bezierCurveTo(double cp1x, double cp1y, double cp2x, double cp2y, double x, double y);

  [Throws, LenientFloat]
  undefined arcTo(double x1, double y1, double x2, double y2, double radius);
// NOT IMPLEMENTED  [LenientFloat] undefined arcTo(double x1, double y1, double x2, double y2, double radiusX, double radiusY, double rotation);

  [LenientFloat]
  undefined rect(double x, double y, double w, double h);

  [Throws]
  undefined roundRect(unrestricted double x, unrestricted double y, unrestricted double w, unrestricted double h, optional (unrestricted double or DOMPointInit or sequence<(unrestricted double or DOMPointInit)>) radii = 0);

  [Throws, LenientFloat]
  undefined arc(double x, double y, double radius, double startAngle, double endAngle, optional boolean anticlockwise = false);

  [Throws, LenientFloat]
  undefined ellipse(double x, double y, double radiusX, double radiusY, double rotation, double startAngle, double endAngle, optional boolean anticlockwise = false);
};

[Exposed=(Window,Worker),
 Func="mozilla::dom::OffscreenCanvas::PrefEnabledOnWorkerThread"]
interface CanvasGradient {
  // opaque object
  [Throws]
  // addColorStop should take a double
  undefined addColorStop(float offset, UTF8String color);
};

[Exposed=(Window,Worker),
 Func="mozilla::dom::OffscreenCanvas::PrefEnabledOnWorkerThread"]
interface CanvasPattern {
  // opaque object
  // [Throws, LenientFloat] - could not do this overload because of bug 1020975
  // undefined setTransform(double a, double b, double c, double d, double e, double f);

  [Throws]
  undefined setTransform(optional DOMMatrix2DInit matrix = {});
};

[Exposed=(Window,Worker)]
interface TextMetrics {

  // x-direction
  readonly attribute double width; // advance width

  // [experimental] actualBoundingBox* attributes
  [Pref="dom.textMetrics.actualBoundingBox.enabled"]
  readonly attribute double actualBoundingBoxLeft;
  [Pref="dom.textMetrics.actualBoundingBox.enabled"]
  readonly attribute double actualBoundingBoxRight;

  // y-direction
  // [experimental] fontBoundingBox* attributes
  [Pref="dom.textMetrics.fontBoundingBox.enabled"]
  readonly attribute double fontBoundingBoxAscent;
  [Pref="dom.textMetrics.fontBoundingBox.enabled"]
  readonly attribute double fontBoundingBoxDescent;

  // [experimental] actualBoundingBox* attributes
  [Pref="dom.textMetrics.actualBoundingBox.enabled"]
  readonly attribute double actualBoundingBoxAscent;
  [Pref="dom.textMetrics.actualBoundingBox.enabled"]
  readonly attribute double actualBoundingBoxDescent;

  // [experimental] emHeight* attributes
  [Pref="dom.textMetrics.emHeight.enabled"]
  readonly attribute double emHeightAscent;
  [Pref="dom.textMetrics.emHeight.enabled"]
  readonly attribute double emHeightDescent;

  // [experimental] *Baseline attributes
  [Pref="dom.textMetrics.baselines.enabled"]
  readonly attribute double hangingBaseline;
  [Pref="dom.textMetrics.baselines.enabled"]
  readonly attribute double alphabeticBaseline;
  [Pref="dom.textMetrics.baselines.enabled"]
  readonly attribute double ideographicBaseline;
};

[Exposed=(Window,Worker)]
interface Path2D
{
  constructor();
  constructor(Path2D other);
  constructor(DOMString pathString);

  [Throws] undefined addPath(Path2D path, optional DOMMatrix2DInit transform = {});
};
Path2D includes CanvasPathMethods;
