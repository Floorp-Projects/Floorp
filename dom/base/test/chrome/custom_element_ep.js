class XFoo extends HTMLElement {
  constructor() {
    super();
    this.magicNumber = 42;
  }

  connectedCallback() {
    finishTest(this.magicNumber === 42);
  }
};
customElements.define("x-foo", XFoo);

document.firstChild.appendChild(document.createElement("x-foo"));
