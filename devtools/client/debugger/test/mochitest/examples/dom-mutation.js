function changeAttribute() {
  const title = document.body.title === "Goodbye" ? "Hello" : "Goodbye";
  document.body.setAttribute("title", title);
}

function changeStyleAttribute() {
  document.body.style.color = "blue";
}

function addDivToBody() {
  let div = document.createElement("div");
  div.id = "dynamic";
  document.body.appendChild(div);
}

function removeDivInBody() {
  document.body.querySelector("#dynamic").remove();
}

function changeAttributeInShadow() {
  document.getElementById("host").shadowRoot.querySelector("div").classList.toggle("red");
}

document.getElementById("host").attachShadow({ mode: "open" }).innerHTML = `<div></div>`;

//# sourceMappingURL=dom-mutation.js.map
