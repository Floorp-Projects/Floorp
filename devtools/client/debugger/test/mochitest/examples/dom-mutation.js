function changeAttribute() {
  const title = document.body.title === "Goodbye" ? "Hello" : "Goodbye";
  document.body.setAttribute("title", title);
}

function changeStyleAttribute() {
  document.body.style.color = "blue";
}

function addDivToBody() {
  document.body.appendChild(document.createElement("div"));
}

function removeDivInBody() {
  document.body.querySelector("div").remove();
}

//# sourceMappingURL=dom-mutation.js.map
