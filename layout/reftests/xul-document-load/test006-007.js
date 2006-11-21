function checkDOM(target, data) {
  // assuming whitespace and the XML declaration are not in the DOM.
  var piNode = document.firstChild;
  if (!piNode || piNode.nodeType != Node.PROCESSING_INSTRUCTION_NODE ||
      piNode.target != target || piNode.data != data) {
    document.documentElement.style.backgroundColor = "red";
  }
}
