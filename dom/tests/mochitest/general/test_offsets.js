var scrollbarWidth = 17, scrollbarHeight = 17;

function testElements(baseid, callback)
{
  scrollbarWidth = scrollbarHeight = gcs($("scrollbox-test"), "width");

  var elements = $(baseid).getElementsByTagName("*");
  for (var t = 0; t < elements.length; t++) {
    var element = elements[t];
    testElement(element);
  }

  var nonappended = document.createElement("div");
  nonappended.id = "nonappended";
  nonappended.setAttribute("_offsetParent", "null");
  testElement(nonappended);

  checkScrolledElement($("scrollbox"), $("scrollchild"));

  var div = $("noscroll");
  div.scrollLeft = 10;
  div.scrollTop = 10;
  is(element.scrollLeft, 0, element.id + " scrollLeft after nonscroll");
  is(element.scrollTop, 0, element.id + " scrollTop after nonscroll");

  callback();
}

function toNearestAppunit(v)
{
  // 60 appunits per CSS pixel; round result to the nearest appunit
  return Math.round(v*60)/60;
}

function isEqualAppunits(a, b, msg)
{
  is(toNearestAppunit(a), toNearestAppunit(b), msg);
}

function testElement(element)
{
  var offsetParent = element.getAttribute("_offsetParent");
  offsetParent = $(offsetParent == "null" ? null: (offsetParent ? offsetParent : "body"));

  var borderLeft = gcs(element, "borderLeftWidth");
  var borderTop = gcs(element, "borderTopWidth");
  var borderRight = gcs(element, "borderRightWidth");
  var borderBottom = gcs(element, "borderBottomWidth");
  var paddingLeft = gcs(element, "paddingLeft");
  var paddingTop = gcs(element, "paddingTop");
  var paddingRight = gcs(element, "paddingRight");
  var paddingBottom = gcs(element, "paddingBottom");
  var width = gcs(element, "width");
  var height = gcs(element, "height");

  if (element instanceof HTMLElement)
    checkOffsetState(element, -10000, -10000,
                              borderLeft + paddingLeft + width + paddingRight + borderRight,
                              borderTop + paddingTop + height + paddingBottom + borderBottom,
                              offsetParent, element.id);

  var scrollWidth, scrollHeight, clientWidth, clientHeight;
  if (element.id == "scrollbox") {
    var lastchild = $("lastline");
    scrollWidth = lastchild.getBoundingClientRect().width + paddingLeft + paddingRight;
    var top = element.firstChild.getBoundingClientRect().top;
    var bottom = element.lastChild.getBoundingClientRect().bottom;
    var contentsHeight = bottom - top;
    scrollHeight = contentsHeight + paddingTop + paddingBottom;
    clientWidth = paddingLeft + width + paddingRight - scrollbarWidth;
    clientHeight = paddingTop + height + paddingBottom - scrollbarHeight;
  }
  else {
    scrollWidth = paddingLeft + width + paddingRight;
    scrollHeight = paddingTop + height + paddingBottom;
    clientWidth = paddingLeft + width + paddingRight;
    clientHeight = paddingTop + height + paddingBottom;
  }

  if (element instanceof SVGElement)
    checkScrollState(element, 0, 0, 0, 0, element.id);
  else
    checkScrollState(element, 0, 0, scrollWidth, scrollHeight, element.id);

  if (element instanceof SVGElement)
    checkClientState(element, 0, 0, 0, 0, element.id);
  else
    checkClientState(element, borderLeft, borderTop, clientWidth, clientHeight, element.id);

  var boundingrect = element.getBoundingClientRect();
  isEqualAppunits(boundingrect.width, borderLeft + paddingLeft + width + paddingRight + borderRight,
     element.id + " bounding rect width");
  isEqualAppunits(boundingrect.height, borderTop + paddingTop + height + paddingBottom + borderBottom,
     element.id + " bounding rect height");
  isEqualAppunits(boundingrect.right - boundingrect.left, boundingrect.width,
     element.id + " bounding rect right");
  isEqualAppunits(boundingrect.bottom - boundingrect.top, boundingrect.height,
     element.id + " bounding rect bottom");

  var rects = element.getClientRects();
  if (element.id == "input-displaynone" || element.id == "nonappended") {
    is(rects.length, 0, element.id + " getClientRects empty");
  }
  else {
    is(rects[0].left, boundingrect.left, element.id + " getClientRects left");
    is(rects[0].top, boundingrect.top, element.id + " getClientRects top");
    is(rects[0].right, boundingrect.right, element.id + " getClientRects right");
    is(rects[0].bottom, boundingrect.bottom, element.id + " getClientRects bottom");
  }
}

function checkScrolledElement(element, child)
{
  var elemrect = element.getBoundingClientRect();
  var childrect = child.getBoundingClientRect();

  var topdiff = childrect.top - elemrect.top;

  element.scrollTop = 20;
  is(element.scrollLeft, 0, element.id + " scrollLeft after vertical scroll");
  is(element.scrollTop, 20, element.id + " scrollTop after vertical scroll");
  // If the viewport has been transformed, then we might have scrolled to a subpixel value
  // that's slightly different from what we requested. After rounding, however, it should
  // be the same.
  is(Math.round(childrect.top - child.getBoundingClientRect().top), 20, "child position after vertical scroll");

  element.scrollTop = 0;
  is(element.scrollLeft, 0, element.id + " scrollLeft after vertical scroll reset");
  is(element.scrollTop, 0, element.id + " scrollTop after vertical scroll reset");
  // Scrolling back to the top should work precisely.
  is(child.getBoundingClientRect().top, childrect.top, "child position after vertical scroll reset");

  element.scrollTop = 10;
  element.scrollTop = -30;
  is(element.scrollLeft, 0, element.id + " scrollLeft after vertical scroll negative");
  is(element.scrollTop, 0, element.id + " scrollTop after vertical scroll negative");
  is(child.getBoundingClientRect().top, childrect.top, "child position after vertical scroll negative");

  element.scrollLeft = 18;
  is(element.scrollLeft, 18, element.id + " scrollLeft after horizontal scroll");
  is(element.scrollTop, 0, element.id + " scrollTop after horizontal scroll");
  is(Math.round(childrect.left - child.getBoundingClientRect().left), 18, "child position after horizontal scroll");

  element.scrollLeft = -30;
  is(element.scrollLeft, 0, element.id + " scrollLeft after horizontal scroll reset");
  is(element.scrollTop, 0, element.id + " scrollTop after horizontal scroll reset");
  is(child.getBoundingClientRect().left, childrect.left, "child position after horizontal scroll reset");
}

function checkOffsetState(element, left, top, width, height, parent, testname)
{
  checkCoords(element, "offset", left, top, width, height, testname);
  is(element.offsetParent, parent, testname + " offsetParent");
}

function checkScrollState(element, left, top, width, height, testname)
{
  checkCoords(element, "scroll", left, top, width, height, testname);
}

function checkClientState(element, left, top, width, height, testname)
{
  checkCoords(element, "client", left, top, width, height, testname);
}

function checkCoord(element, type, val, testname)
{
  if (val != -10000)
    is(element[type], Math.round(val), testname + " " + type);
}

function checkCoords(element, type, left, top, width, height, testname)
{
  checkCoord(element, type + "Left", left, testname);
  checkCoord(element, type + "Top", top, testname);
  checkCoord(element, type + "Width", width, testname);
  checkCoord(element, type + "Height", height, testname);

  if (element instanceof SVGElement)
    return;

  if (element.id == "outerpopup" && !element.parentNode.open) // closed popup
    return;

  if (element.id == "input-displaynone" || element.id == "nonappended") // hidden elements
    ok(element[type + "Width"] == 0 && element[type + "Height"] == 0,
       element.id + " has zero " + type + " width and height");
  else if (element.id != "input-nosize") // for some reason, this element has a width of 2
    ok(element[type + "Width"] > 0 && element[type + "Height"] > 0,
       element.id + " has non-zero " + type + " width and height");
}

function gcs(element, prop)
{
  var propVal = (element instanceof SVGElement && (prop == "width" || prop == "height")) ?
                   element.getAttribute(prop) : getComputedStyle(element, "")[prop];
  if (propVal == "auto")
    return 0;
  return parseFloat(propVal);
}
