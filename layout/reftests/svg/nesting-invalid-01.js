// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

// These are all of the SVG 1.1 and Filter Effects element names, except for container and deprecated elements,
// with the addition of an "UNKNOWN" element in the SVG namespace.
var es = 'animate animateMotion animateTransform circle color-profile cursor desc ellipse feBlend feColorMatrix feComponentTransfer feComposite feConvolveMatrix feDiffuseLighting feDisplacementMap feDistantLight feDropShadow feFlood feFuncA feFuncB feFuncG feFuncR feGaussianBlur feImage feMerge feMergeNode feMorphology feOffset fePointLight feSpecularLighting feSpotLight feTile feTurbulence filter image line linearGradient metadata mpath path polygon polyline radialGradient rect script set stop style text textPath title tspan use view UNKNOWN'.split(' ');
var colwidth = 200;
var size = 40;
var rows = 14;

function makeElement(localName, attrs, children) {
  var e = document.createElementNS('http://www.w3.org/2000/svg', localName);
  for (var an in attrs) {
    e.setAttribute(an, attrs[an]);
  }
  if (children) {
    for (var i = 0; i < children.length; i++) {
      if (typeof children[i] == 'object') {
        e.appendChild(children[i]);
      } else {
        e.appendChild(document.createTextNode(children[i]));
      }
    }
  }
  return e;
}

function makeGroup(i, failing, text) {
  var x = colwidth * Math.floor(i / rows),
      y = size * (i % rows);
  return makeElement('g', { 'fill-opacity': failing ? '1' : '0.25' },
           [makeElement('rect', { x: x, y: y, width: colwidth, height: size, fill: 'white' }),
            makeElement('rect', { x: x, y: y, width: size, height: size, fill: failing ? 'red' : 'green' }),
            makeElement('text', { x: x + size + 10, y: y + size / 2 + 6 }, [text])]);
}
