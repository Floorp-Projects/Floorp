function getHeight(elem) {
  return elem.getBoundingClientRect().height + 'px';
}

function makeHeightMatchInlineBox(block, inline) {
  var height = getHeight(inline);
  block.style.height = height;
  block.style.lineHeight = height;
}
