function makeHeightMatchInlineBox(block, inline) {
  var height = inline.getBoundingClientRect().height + 'px';
  block.style.height = height;
  block.style.lineHeight = height;
}
