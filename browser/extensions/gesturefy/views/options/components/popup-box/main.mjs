// getter for module path
const MODULE_DIR = (() => {
  const urlPath = new URL(import.meta.url).pathname;
  return urlPath.slice(0, urlPath.lastIndexOf("/") + 1);
})();


/**
 * Custom element - <popup-box>
 * Accepts two sepcial attributes (and properties):
 * open : boolean - opens or closes the popup
 * type : confirm, alert, prompt, custom (default) - defines the functionality/purpose of the popup
 * Dispatches 2 custom events:
 * close : detail value depends on the popup type
 * open : detail value is always null
 * Special property:
 * value : contains the same value as the detail value of the event
 **/
class PopupBox extends HTMLElement {

  /**
   * Construcor
   * Create shadow root and load stylesheet by appending it to the shadow DOM
   **/
  constructor() {
    super();

    this.attachShadow({mode: 'open'}).innerHTML = `
      <link id="popupStylesheet" rel="stylesheet" href="${MODULE_DIR}layout.css">
    `;
    // add a promise and resolve it when the stylesheet is loaded
    this._loaded = new Promise ((resolve) => {
      this.shadowRoot.getElementById("popupStylesheet").onload = resolve;
    });
  }

  /**
   * Set observable attributes
   **/
  static get observedAttributes() {
    return ['type', 'open'];
  }

  /**
   * Handles the standard connected callback
   * Opens the popup if the element gets appended to the DOM and the open attribute is set
   **/
  connectedCallback () {
    if (this.open && this.isConnected) this._openPopupBox();
  }


  /**
   * Handles the standard disconnected callback
   * Removes all popup elements from the shadow DOM
   **/
  disconnectedCallback () {
    const popupOverlay = this.shadowRoot.getElementById("popupOverlay");
    const popupWrapper = this.shadowRoot.getElementById("popupWrapper");
    // remove popup elements
    if (popupOverlay) popupOverlay.remove();
    if (popupWrapper) popupWrapper.remove();
  }


  /**
   * Handles the standard attribute change callback
   * Open/close the popup on open attribute change
   * Replace the popup if the popup type changes
   **/
  attributeChangedCallback (name, oldValue, newValue) {
    // do nothing if element is not appended to the DOM
    if (!this.isConnected) return;

    switch (name) {
      case 'open': {
        if (this.hasAttribute('open')) {
          // if previous value was not set open popup
          // == checks for null and undefined
          if (oldValue == null) this._openPopupBox();
        }
        else {
          this._closePopupBox();
        }
      } break;

      case 'type': {
        // remove old popup elements
        const popupOverlay = this.shadowRoot.getElementById("popupOverlay");
        const popupWrapper = this.shadowRoot.getElementById("popupWrapper");
        if (popupOverlay) popupOverlay.remove();
        if (popupWrapper) popupWrapper.remove();
        // create and add new popup
        const popupFragment = this._buildPopupBox();
        this.shadowRoot.append(popupFragment);
      } break;
    }
  }


  get open () {
    return this.hasAttribute('open');
  }

  set open (value) {
    if (value) this.setAttribute('open', '');
    else this.removeAttribute('open');
  }

  get type () {
    return this.getAttribute('type') || 'custom';
  }

  set type (value) {
    this.setAttribute('type', value);
  }


  /**
   * Constructs the main command bar structure and popupOverlay
   * Returns it as a document fragment
   **/
  _buildPopupBox () {
    const template = document.createElement('template');
    template.innerHTML = `
      <div id="popupOverlay"></div>
      <div id="popupWrapper">
        <div id="popupBox">
          <div id="popupBoxHead">
            <div id="popupBoxHeading"><slot name="title">Title..</slot></div>
            <button id="popupBoxCloseButton" type="button"></button>
          </div>
          <div id="popupBoxMain"><slot name="content">Content..</slot></div>
          <div id="popupBoxFooter"></div>
        </div>
      </div>
    `;

    // register event handlers
    const popupOverlay = template.content.getElementById("popupOverlay");
          popupOverlay.addEventListener("click", this._handleCloseButtonClick.bind(this), { once: true });
    const popupBoxCloseButton = template.content.getElementById("popupBoxCloseButton");
          popupBoxCloseButton.addEventListener("click", this._handleCloseButtonClick.bind(this), { once: true });

    const popupBoxFooter = template.content.getElementById("popupBoxFooter");

    // add popup type dependent inputs
    switch (this.type) {
      case "alert": {
        const popupBoxConfirmButton = document.createElement("button");
              popupBoxConfirmButton.id = "popupBoxConfirmButton";
              popupBoxConfirmButton.textContent = browser.i18n.getMessage("buttonConfirm");
              popupBoxConfirmButton.addEventListener("click", this._handleCloseButtonClick.bind(this), { once: true });
        popupBoxFooter.append(popupBoxConfirmButton);
      } break;

      case "confirm": {
        const popupBoxConfirmButton = document.createElement("button");
              popupBoxConfirmButton.id = "popupBoxConfirmButton";
              popupBoxConfirmButton.textContent = browser.i18n.getMessage("buttonConfirm");
              popupBoxConfirmButton.addEventListener("click", this._handleConfirmButtonClick.bind(this), { once: true });
        const popupBoxCancelButton = document.createElement("button");
              popupBoxCancelButton.id = "popupBoxCancelButton";
              popupBoxCancelButton.textContent = browser.i18n.getMessage("buttonCancel");
              popupBoxCancelButton.addEventListener("click", this._handleCancelButtonClick.bind(this), { once: true });
        popupBoxFooter.append(popupBoxCancelButton, popupBoxConfirmButton);
      } break;

      case "prompt": {
        const popupBoxInput = document.createElement("input");
              popupBoxInput.id = "popupBoxInput";
              popupBoxInput.addEventListener("keypress", this._handleInputKeypress.bind(this));
        const popupBoxConfirmButton = document.createElement("button");
              popupBoxConfirmButton.id = "popupBoxConfirmButton";
              popupBoxConfirmButton.textContent = browser.i18n.getMessage("buttonConfirm");
              popupBoxConfirmButton.addEventListener("click", this._handleConfirmButtonClick.bind(this), { once: true });
        popupBoxFooter.append(popupBoxInput, popupBoxConfirmButton);
      } break;
    }

    return template.content;
  }


  /**
   * Builds and opens the popup
   * Dispatches the open event listener
   **/
  _openPopupBox () {
    this._loaded.then(() => {
      // delete old value if any
      delete this.value;

      const popupFragment = this._buildPopupBox();

      const popupOverlay = popupFragment.getElementById("popupOverlay");
      const popupBox = popupFragment.getElementById("popupBox");

      popupOverlay.classList.add("po-hide");
      popupBox.classList.add("pb-hide");

      // append to shadow dom
      this.shadowRoot.append(popupFragment);

      // trigger reflow
      popupBox.offsetHeight;

      // cleanup classes after animation
      popupOverlay.addEventListener("transitionend", function removePopupOverlay(event) {
        // prevent the event from firing for child transitions
        if (event.currentTarget === event.target) {
          popupOverlay.removeEventListener("transitionend", removePopupOverlay);
          popupOverlay.classList.remove("po-show");
        }
      });
      popupBox.addEventListener("animationend", function removePopupBox(event) {
        // prevent the event from firing for child transitions
        if (event.currentTarget === event.target) {
          popupBox.removeEventListener("transitionend", removePopupBox);
          popupBox.classList.remove("pb-show");
        }
      });

      // start show animation
      popupOverlay.classList.replace("po-hide", "po-show");
      popupBox.classList.replace("pb-hide", "pb-show");

      // dispatch custom open event
      this.dispatchEvent(new CustomEvent("open"));
    });
  }


  /**
   * Closes the popup and removes it from the shadow dom
   * Dispatches the close event listener with the given value as detail
   **/
  _closePopupBox () {
    const popupOverlay = this.shadowRoot.getElementById("popupOverlay");
    const popupWrapper = this.shadowRoot.getElementById("popupWrapper");
    const popupBox = this.shadowRoot.getElementById("popupBox");

    popupOverlay.addEventListener("transitionend", function removePopupOverlay(event) {
      // prevent the event from firing for child transitions
      if (event.currentTarget === event.target) {
        popupOverlay.removeEventListener("transitionend", removePopupOverlay);
        popupOverlay.remove();
      }
    });
    popupBox.addEventListener("transitionend", function removePopupBox(event) {
      // prevent the event from firing for child transitions
      if (event.currentTarget === event.target) {
        popupBox.removeEventListener("transitionend", removePopupBox);
        popupWrapper.remove();
      }
    });

    // start hide animation
    popupOverlay.classList.add("po-hide");
    popupBox.classList.add("pb-hide");

    // dispatch custom close event
    this.dispatchEvent(new CustomEvent("close", {
      detail: this.value
    }));
  }


  /**
   * Listenes for the close button click, closes the popup and passes null as the detail value
   **/
  _handleCloseButtonClick (event) {
    // remove open attribute and close popup
    this.open = false;
  }


  /**
   * Listenes for the confirm button click, closes the popup and passes the input value or true as the detail value
   **/
  _handleConfirmButtonClick (event) {
    const popupBoxInput = this.shadowRoot.getElementById("popupBoxInput");
    // set value either to true or to the input value if type "prompt" was used
    this.value = popupBoxInput ? popupBoxInput.value : true;
    // remove open attribute and close popup
    this.open = false;
  }


  /**
   * Listenes for the cancel button click, closes the popup and passes false as the detail value
   **/
  _handleCancelButtonClick (event) {
    // set value to false
    this.value = false;
    // remove open attribute and close popup
    this.open = false;
  }


  /**
   * Listenes for the enter key press and runs the cofirm button function
   **/
  _handleInputKeypress (event) {
     if (event.key === "Enter") this._handleConfirmButtonClick();
  }
}

// define custom element
window.customElements.define('popup-box', PopupBox);