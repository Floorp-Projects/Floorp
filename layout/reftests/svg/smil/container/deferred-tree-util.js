function makeDiv()
{
  const xhtmlns="http://www.w3.org/1999/xhtml";
  return document.createElementNS(xhtmlns, 'div');
}

function makeSvg()
{
  const svgns="http://www.w3.org/2000/svg";
  var svg = document.createElementNS(svgns, 'svg');
  svg.setAttribute('xmlns', svgns);
  svg.setAttribute('width', '200px');
  svg.setAttribute('height', '200px');
  var rect = document.createElementNS(svgns, 'rect');
  rect.setAttribute('x', '0');
  rect.setAttribute('y', '0');
  rect.setAttribute('width', '199');
  rect.setAttribute('height', '199');
  rect.setAttribute('style', 'fill: none; stroke: black');
  var ellipse = document.createElementNS(svgns, 'ellipse');
  ellipse.setAttribute('stroke-width', '1');
  ellipse.setAttribute('stroke', 'black');
  ellipse.setAttribute('fill', 'yellow');
  ellipse.setAttribute('cx', '100');
  ellipse.setAttribute('cy', '20');
  ellipse.setAttribute('rx', '40');
  ellipse.setAttribute('ry', '20');
  var anim = document.createElementNS(svgns, 'animate');
  anim.setAttribute('attributeName', 'cy');
  anim.setAttribute('attributeType', 'XML');
  anim.setAttribute('begin', '0s');
  anim.setAttribute('from', '20');
  anim.setAttribute('to', '170');
  anim.setAttribute('dur', '2s');
  ellipse.appendChild(anim);
  svg.appendChild(rect);
  svg.appendChild(ellipse);
  return svg;
}
