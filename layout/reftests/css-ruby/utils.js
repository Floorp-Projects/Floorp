function getBlockAxisName(elem) {
  var wm = getComputedStyle(elem).writingMode;
  return (!wm || wm == 'horizontal-tb') ? 'height' : 'width';
}

function getBSize(elem) {
  return elem.getBoundingClientRect()[getBlockAxisName(elem)] + 'px';
}

function setBSize(elem, bsize) {
  elem.style[getBlockAxisName(elem)] = bsize;
  elem.style.lineHeight = bsize;
}

// Ruby annotations are placed based on block-axis size of inline boxes
// instead of line box. Block-axis size of an inline box is the max
// height of the font, while that of line box is line height. Hence we
// sometimes need to explicitly set the block-axis size of an inline
// box to a block to simulate the exact behavior, which is what the
// following two functions do.

function makeBSizeMatchInlineBox(block, inline) {
  setBSize(block, getBSize(inline));
}

function makeBSizeOfParentMatch(elems) {
  // The size change is divided into two phases to avoid
  // triggering reflow for every element.
  for (var elem of elems)
    elem.dataset.bsize = getBSize(elem);
  for (var elem of elems)
    setBSize(elem.parentNode, elem.dataset.bsize);
}
