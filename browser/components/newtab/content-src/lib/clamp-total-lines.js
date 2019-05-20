/**
 * Adjusts line-clamps of a node's children to fill a desired number of lines.
 *
 * This is a React callback ref that should be set on a parent node that also
 * has a data-total-lines attribute. Children with "clamp" class name are
 * clamped to allow as many lines to earlier children while making sure every
 * child gets at least one line. Each child can be explicitly clamped to a max
 * lines with a data-clamp attribute.
 */
export function clampTotalLines(parentNode) {
  // Nothing to do if the node is removed or didn't configure how many lines
  if (!parentNode || !parentNode.dataset.totalLines) {
    return;
  }

  // Only handle clamp-able children that are displayed (not hidden)
  const toClamp = Array.from(parentNode.querySelectorAll(".clamp"))
    .filter(child => child.scrollHeight);

  // Start with total allowed lines while reserving 1 line for each child
  let maxLines = parentNode.dataset.totalLines - toClamp.length + 1;
  toClamp.forEach(child => {
    // Clamp to the remaining allowed, explicit limit or the natural line count
    const lines = Math.min(maxLines,
      child.dataset.clamp || Infinity,
      child.scrollHeight / parseInt(global.getComputedStyle(child).lineHeight, 10));
    child.style.webkitLineClamp = `${lines}`;

    // Update the remaining line allowance less the already reserved 1 line
    maxLines -= lines - 1;
  });
}
