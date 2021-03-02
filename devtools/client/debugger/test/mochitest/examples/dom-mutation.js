function changeAttribute() {
  const title = document.body.title === "Goodbye" ? "Hello" : "Goodbye";
  document.body.setAttribute("title", title);
}

function changeStyleAttribute() {
  document.body.style.color = "blue";
}

function changeSubtree() {
  document.body.appendChild(document.createElement("div"));
}
//# sourceMappingURL=dom-mutation.js.map
