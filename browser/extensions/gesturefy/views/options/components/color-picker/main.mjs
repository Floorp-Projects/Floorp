import "/views/options/components/popup-box/main.mjs";

// getter for module path
const MODULE_DIR = (() => {
  const urlPath = new URL(import.meta.url).pathname;
  return urlPath.slice(0, urlPath.lastIndexOf("/") + 1);
})();

/**
 * Custom element - <color-picker>
 * Accepts one special attribute (and property):
 * value : Hex value with alpha channel | default value: #00000000
 * Dispatches a "change" event on value changes
 **/
class ColorPicker extends HTMLElement {

  /**
   * Construcor
   * Set default variables
   * Create shadow root and load stylesheet by appending it to the shadow DOM
   * Add all necessary event listeners
   **/
  constructor() {
    super();

    this._rgba = [0, 0, 0, 0];

    this.attachShadow({mode: 'open'}).innerHTML = `
      <link rel="stylesheet" href="${MODULE_DIR}layout.css">

      <div id="content"></div>

      <popup-box id="colorPickerPopup">
        <span slot="title">Color Picker</span>
        <div id="colorPicker" slot="content">
          <div class="cp-main">
            <div class="cp-panel" id="colorFieldPanel">
              <canvas id="colorField" width="256" height="256"></canvas>
              <div id="colorFieldCursor"></div>
            </div>

            <div class="cp-panel" id="colorScalePanel">
              <canvas id="colorScale" width="20" height="256"></canvas>
              <div id="colorScaleCursor"></div>
            </div>

            <div class="cp-panel" id="alphaScalePanel">
              <div id="alphaScale"></div>
              <div id="alphaScaleCursor"></div>
            </div>

            <div class="cp-panel">
              <form id="rgbaValueForm">
                <label>R<input name="R" type="number" step="1" min="0" max="255" required></label>
                <label>G<input name="G" type="number" step="1" min="0" max="255" required></label>
                <label>B<input name="B" type="number" step="1" min="0" max="255" required></label>
                <label>A<input name="A" type="number" step="0.01" min="0" max="1" required></label>
                <div id="colorPreview"></div>
              </form>
            </div>
          </div>
          <div class="cp-footer">
            <form id="hexValueForm">
              <input name="Hex" pattern="#[a-fA-F0-9]{8}" required>
              <button id="saveButton">Save</button>
            </form>
          </div>
        </div>
      </popup-box>
    `;

    const colorField = this.shadowRoot.getElementById("colorField");
          colorField.addEventListener("pointerdown", this._handleColorFieldPointerdown.bind(this));
    const colorFieldCursor = this.shadowRoot.getElementById("colorFieldCursor");
          colorFieldCursor.addEventListener("pointerdown", this._handleColorFieldPointerdown.bind(this));

    const colorScale = this.shadowRoot.getElementById("colorScale");
          colorScale.addEventListener("pointerdown", this._handleColorScalePointerdown.bind(this));
    const colorScaleCursor = this.shadowRoot.getElementById("colorScaleCursor");
          colorScaleCursor.addEventListener("pointerdown", this._handleColorScalePointerdown.bind(this));

    const alphaScale = this.shadowRoot.getElementById("alphaScale");
          alphaScale.addEventListener("pointerdown", this._handleAlphaScalePointerdown.bind(this));
    const alphaScaleCursor = this.shadowRoot.getElementById("alphaScaleCursor");
          alphaScaleCursor.addEventListener("pointerdown", this._handleAlphaScalePointerdown.bind(this));

    const ctx = colorScale.getContext('2d');
    const gradient = ctx.createLinearGradient(0, 0, 0, colorScale.height);
          gradient.addColorStop(0, 'rgb(255,0,0)');
          gradient.addColorStop(0.16, 'rgb(255,0,255)');
          gradient.addColorStop(0.33, 'rgb(0,0,255)');
          gradient.addColorStop(0.49, 'rgb(0,255,255)');
          gradient.addColorStop(0.65, 'rgb(0,255,0)');
          gradient.addColorStop(0.84, 'rgb(255,255,0)');
          gradient.addColorStop(1, 'rgb(255,0,0)');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, colorScale.width, colorScale.height);

    const rgbaValueForm = this.shadowRoot.getElementById("rgbaValueForm");
          rgbaValueForm.addEventListener('input', this._handleRGBAInput.bind(this));

    const hexValueForm = this.shadowRoot.getElementById("hexValueForm");
          hexValueForm.addEventListener('input', this._handleHexInput.bind(this));
          hexValueForm.addEventListener('submit', this._handleHexSubmit.bind(this));

    const saveButton = this.shadowRoot.getElementById("saveButton");
          saveButton.textContent = browser.i18n.getMessage('buttonSave');

    this.addEventListener('click', this._handleHostElementClick.bind(this));
  }


  /**
   * Add attribute observer
   **/
  static get observedAttributes() {
    return ['value'];
  }


  /**
   * Attribute change handler
   * Update the color picker values, colors and the input field color on "value" attribute change
   **/
  attributeChangedCallback (name, oldValue, newValue) {
    if (name === "value") {
      if (newValue) {
        this._rgba = HexAToRGBA(newValue);
      }
      else {
        this._rgba = [0, 0, 0, 0];
      }
      this._updateRGBAInputs(...this._rgba);
      this._handleRGBAInput();

      const content = this.shadowRoot.getElementById("content");
      content.style.setProperty("--color", `rgba(${this._rgba})`);
    }
  }


  /**
   * Getter for the "value" attribute
   **/
  get value () {
    return this.getAttribute('value');
  }


  /**
   * Setter for the "value" attribute
   **/
  set value (value) {
    this.setAttribute('value', value);
  }


  /**
   * Opens the color picker popup
   **/
  _openColorPicker () {
    const colorPickerPopup = this.shadowRoot.getElementById("colorPickerPopup");
    colorPickerPopup.open = true;
  }


  /**
   * Opens the color picker popup
   **/
  _closeColorPicker () {
    const colorPickerPopup = this.shadowRoot.getElementById("colorPickerPopup");
    colorPickerPopup.open = false;
  }


  /**
   * Get the color data of the color field (saturation & value field) at the given x and y position
   **/
  _getColorFieldData (x, y) {
    const colorField  = this.shadowRoot.getElementById("colorField");

    x = clamp(x, 0, colorField.width - 1);
    y = clamp(y, 0, colorField.height - 1);

    return colorField.getContext('2d').getImageData(x, y, 1, 1).data;
  }


  /**
   * Get the color data of the color scale (hue field) at the given y position
   **/
  _getColorScaleData (y) {
    const colorScale = this.shadowRoot.getElementById("colorScale");

    y = clamp(y, 0, colorScale.height - 1);

    return colorScale.getContext('2d').getImageData(0, y, 1, 1).data;
  }


  /**
   * Get the alpha value of the alpha scale at the given y position
   **/
  _getAlphaScaleData (y) {
    const alphaScale = this.shadowRoot.getElementById("alphaScale");
    const alphaHeight = alphaScale.offsetHeight;

    y = clamp(y, 0, alphaHeight - 1);

    // calculate alpha value
    const alpha = 1 - (1 / alphaHeight * y);
    // round alpha value by 2 decimal places
    return Math.round(alpha * 100) / 100;
  }


  /**
   * Update the position of the color field (saturation & value field) cursor
   * By the given x and y coordinates
   **/
  _updateColorFieldCursor (x, y) {
    const colorField = this.shadowRoot.getElementById("colorField");
    const colorPicker = this.shadowRoot.getElementById("colorPicker");

    x = clamp(x, 0, colorField.width - 1);
    y = clamp(y, 0, colorField.height - 1);

    colorPicker.style.setProperty("--colorFieldX", x);
    colorPicker.style.setProperty("--colorFieldY", y);
  }


  /**
   * Update the position of the color scale (hue field) cursor
   * By the given y coordinates
   **/
  _updateColorScaleCursor (y) {
    const colorScale = this.shadowRoot.getElementById("colorScale");
    const colorPicker = this.shadowRoot.getElementById("colorPicker");

    y = clamp(y, 0, colorScale.height - 1);

    colorPicker.style.setProperty("--colorScaleY", y);
  }


  /**
   * Update the position of the alpha scale cursor
   * By the given y coordinates
   **/
  _updateAlphaScaleCursor (y) {
    const alphaScale = this.shadowRoot.getElementById("alphaScale");
    const colorPicker = this.shadowRoot.getElementById("colorPicker");

    y = clamp(y, 0, (alphaScale.offsetHeight || 256) - 1);

    colorPicker.style.setProperty("--alphaScaleY", y);
  }


  /**
   * Update the color of the color field (saturation & value field)
   * By the given rgb value (hue)
   **/
  _updateColorField (...rgb) {
    const colorField = this.shadowRoot.getElementById("colorField");

    let gradient;

    const ctx = colorField.getContext('2d');
    ctx.fillStyle = `rgb(${ rgb.join(',') })`;
    ctx.fillRect(0, 0, colorField.width, colorField.height);
        gradient = ctx.createLinearGradient(0, 0, colorField.width, 0);
        gradient.addColorStop(0, 'rgba(255,255,255,1)');
        gradient.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, colorField.width, colorField.height);
        gradient = ctx.createLinearGradient(0, 0, 0, 255);
        gradient.addColorStop(0, 'rgba(0,0,0,0)');
        gradient.addColorStop(1, 'rgba(0,0,0,1)');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, colorField.width, colorField.height);
  }


  /**
   * Update the current color picker color and alpha by the given rgba value
   **/
  _updateColor (...rgba) {
    const colorPicker = this.shadowRoot.getElementById("colorPicker");
    colorPicker.style.setProperty("--color", `${rgba[0]}, ${rgba[1]}, ${rgba[2]}`);
    colorPicker.style.setProperty("--alpha", rgba[3]);
    // update internal variable
    this._rgba = rgba;
  }


  /**
   * Update the color picker rgba inputs by the given rgba value
   **/
  _updateRGBAInputs (...rgba) {
    const rgbaValueForm = this.shadowRoot.getElementById("rgbaValueForm");
    rgbaValueForm.R.value = rgba[0];
    rgbaValueForm.G.value = rgba[1];
    rgbaValueForm.B.value = rgba[2];
    rgbaValueForm.A.value = rgba[3];
  }


  /**
   * Update the color picker hex input by the given rgba value
   **/
  _updateHexInput (...rgba) {
    const hexValueForm = this.shadowRoot.getElementById("hexValueForm");
    hexValueForm.Hex.value = RGBAToHexA(...rgba);
  }


  /**
   * Handles the rgba input event
   * Updates all other color picker elements to match the new value
   **/
  _handleRGBAInput () {
    const rgbaValueForm = this.shadowRoot.getElementById("rgbaValueForm");
    if (rgbaValueForm.reportValidity()) {
      const rgba = [
        rgbaValueForm.R.valueAsNumber,
        rgbaValueForm.G.valueAsNumber,
        rgbaValueForm.B.valueAsNumber,
        rgbaValueForm.A.valueAsNumber
      ];
      const hsv = rgbToHSV(...rgba);

      const colorField = this.shadowRoot.getElementById("colorField");
      const colorFieldX = Math.round((hsv[1]) * (colorField.width - 1));
      const colorFieldY = Math.round((1 - hsv[2]) * (colorField.height - 1));

      const colorScale = this.shadowRoot.getElementById("colorScale");
      const colorScaleY = Math.round((1 - hsv[0]) * (colorScale.height - 1));

      const alphaScale = this.shadowRoot.getElementById("alphaScale");
      // offsetHeight can be undefined / 0 when the element is hidden
      const alphaScaleY = Math.round((1 - rgba[3]) * ((alphaScale.offsetHeight || 256) - 1));

      this._updateColorFieldCursor(colorFieldX, colorFieldY);
      this._updateColorScaleCursor(colorScaleY);
      this._updateAlphaScaleCursor(alphaScaleY);

      const colorScaleData = this._getColorScaleData(colorScaleY);

      this._updateColorField(colorScaleData[0], colorScaleData[1], colorScaleData[2]);
      this._updateHexInput(...rgba);
      this._updateColor(...rgba);
    }
  }


  /**
   * Handles the hex input event
   * Updates all other color picker elements to match the new value
   **/
  _handleHexInput () {
    const hexValueForm = this.shadowRoot.getElementById("hexValueForm");
    if (hexValueForm.reportValidity()) {
      const hex = hexValueForm.Hex.value;
      const rgba = HexAToRGBA(hex);
      const hsv = rgbToHSV(...rgba);

      const colorField = this.shadowRoot.getElementById("colorField");
      const colorFieldX = Math.round((hsv[1]) * (colorField.width - 1));
      const colorFieldY = Math.round((1 - hsv[2]) * (colorField.height - 1));

      const colorScale = this.shadowRoot.getElementById("colorScale");
      const colorScaleY = Math.round((1 - hsv[0]) * (colorScale.height - 1));

      const alphaScale = this.shadowRoot.getElementById("alphaScale");
      // offsetHeight can be undefined / 0 when the element is hidden
      const alphaScaleY = Math.round((1 - rgba[3]) * ((alphaScale.offsetHeight || 256) - 1));

      this._updateColorFieldCursor(colorFieldX, colorFieldY);
      this._updateColorScaleCursor(colorScaleY);
      this._updateAlphaScaleCursor(alphaScaleY);

      const colorScaleData = this._getColorScaleData(colorScaleY);

      this._updateColorField(colorScaleData[0], colorScaleData[1], colorScaleData[2]);
      this._updateRGBAInputs(...rgba);
      this._updateColor(...rgba);
    }
  }


  /**
   * Handles the color field pointerdown event
   * Enables the pointermove event listener and calls it once
   **/
  _handleColorFieldPointerdown (event) {
    if (event.buttons === 1) {
      const callbackReference = this._handleColorFieldPointerMove.bind(this);
      // call pointer move listener once with the current event
      // in order to update the cursor position
      callbackReference(event);

      this.shadowRoot.addEventListener("pointermove", callbackReference);

      this.shadowRoot.addEventListener("pointerup", (event) => {
        // remove the pointer move listener
        this.shadowRoot.removeEventListener("pointermove", callbackReference);
      }, { once: true });
    }
  }


  /**
   * Handles the color scale pointerdown event
   * Enables the pointermove event listener and calls it once
   **/
  _handleColorScalePointerdown (event) {
    if (event.buttons === 1) {
      const callbackReference = this._handleColorScalePointerMove.bind(this);
      // call pointer move listener once with the current event
      // in order to update the cursor position
      callbackReference(event);

      this.shadowRoot.addEventListener("pointermove", callbackReference);

      this.shadowRoot.addEventListener("pointerup", (event) => {
        // remove the pointer move listener
        this.shadowRoot.removeEventListener("pointermove", callbackReference);
      }, { once: true });
    }
  }


  /**
   * Handles the alpha scale pointerdown event
   * Enables the pointermove event listener and calls it once
   **/
  _handleAlphaScalePointerdown (event) {
    if (event.buttons === 1) {
      const callbackReference = this._handleAlphaScalePointerMove.bind(this);
      // call pointer move listener once with the current event
      // in order to update the cursor position
      callbackReference(event);

      this.shadowRoot.addEventListener("pointermove", callbackReference);

      this.shadowRoot.addEventListener("pointerup", (event) => {
        // remove the pointer move listener
        this.shadowRoot.removeEventListener("pointermove", callbackReference);
      }, { once: true });
    }
  }


  /**
   * Handles the color field pointermove event
   * Updates the color field cursor position and retrieves the new value
   * Updates all other color picker elements to match the new value
   **/
  _handleColorFieldPointerMove (event) {
    if (event.buttons === 1) {
      const colorField = this.shadowRoot.getElementById("colorField");
      const clientRect = colorField.getBoundingClientRect();

      const x = Math.round(event.clientX - clientRect.left),
            y = Math.round(event.clientY - clientRect.top);

      const colorFieldData = this._getColorFieldData(x, y);

      this._updateColorFieldCursor(x, y);

      this._updateColor(colorFieldData[0], colorFieldData[1], colorFieldData[2], this._rgba[3]);

      this._updateRGBAInputs(colorFieldData[0], colorFieldData[1], colorFieldData[2], this._rgba[3]);
      this._updateHexInput(colorFieldData[0], colorFieldData[1], colorFieldData[2], this._rgba[3]);

      event.preventDefault();
    }
  }


  /**
   * Handles the color scale pointermove event
   * Updates the color scale cursor position and retrieves the new value
   * Updates all other color picker elements to match the new value
   **/
  _handleColorScalePointerMove (event) {
    if (event.buttons === 1) {
      const colorScale = this.shadowRoot.getElementById("colorScale");
      const clientRect = colorScale.getBoundingClientRect();

      const y = Math.round(event.clientY - clientRect.top);

      const colorScaleData = this._getColorScaleData(y);
      // set max and min colors to red
      if (y <= 0) colorScaleData[2] = 0;
      if (y >= 255) colorScaleData[1] = 0;

      this._updateColorScaleCursor(y);

      this._updateColorField(colorScaleData[0], colorScaleData[1], colorScaleData[2]);

      const colorPicker = this.shadowRoot.getElementById("colorPicker");
      const colorFieldCursorX = colorPicker.style.getPropertyValue("--colorFieldX");
      const colorFieldCursorY = colorPicker.style.getPropertyValue("--colorFieldY");
      const colorData = this._getColorFieldData(colorFieldCursorX, colorFieldCursorY);

      this._updateColor(colorData[0], colorData[1], colorData[2], this._rgba[3]);

      this._updateRGBAInputs(colorData[0], colorData[1], colorData[2], this._rgba[3]);
      this._updateHexInput(colorData[0], colorData[1], colorData[2], this._rgba[3]);

      event.preventDefault();
    }
  }


  /**
   * Handles the alpha scale pointermove event
   * Updates the alpha scale cursor position and retrieves the new value
   * Updates all other color picker elements to match the new value
   **/
  _handleAlphaScalePointerMove (event) {
    if (event.buttons === 1) {
      const alphaScale = this.shadowRoot.getElementById("alphaScale");
      const clientRect = alphaScale.getBoundingClientRect();

      const y = Math.round(event.clientY - clientRect.top);

      const alphaScaleData = this._getAlphaScaleData(y);

      this._updateAlphaScaleCursor(y);

      this._updateColor(this._rgba[0], this._rgba[1], this._rgba[2], alphaScaleData);

      this._updateRGBAInputs(this._rgba[0], this._rgba[1], this._rgba[2], alphaScaleData);
      this._updateHexInput(this._rgba[0], this._rgba[1], this._rgba[2], alphaScaleData);

      event.preventDefault();
    }
  }


  /**
   * Handles the color picker form submit
   * Sets the temporary color picker value to the new value
   * Dispatches an input change event
   **/
  _handleHexSubmit (event) {
    this.value = RGBAToHexA(...this._rgba);
    this.dispatchEvent( new InputEvent('change') );

    this._closeColorPicker();
    event.preventDefault();
  }


  /**
   * Handles the color field input click
   * Opens the color picker popup
   **/
  _handleHostElementClick (event) {
    // ignore click events triggered by popup/color picker
    if (event.composedPath()[0] === this) {

      // reset color picker to current value
      this._rgba = this.value ? HexAToRGBA(this.value) : [0, 0, 0, 0];
      this._updateRGBAInputs(...this._rgba);
      this._handleRGBAInput();

      this._openColorPicker();
    }
  }
}

// define custom element <color-picker></color-picker>
window.customElements.define('color-picker', ColorPicker);



/**
 * Function to limit a given number between 2 given numbers
 **/
function clamp (number, min, max) {
  return Math.min(Math.max(number, min), max);
};


/**
 * Function to convert rgb to hsv color space
 * Returns an array of floating point values in the range of 0 - 1
 **/
function rgbToHSV(r, g, b) {
  r /= 255, g /= 255, b /= 255;

  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const d = max - min;

  let h, s, v;

  if (max == min) {
    h = 0;
  }
  else {
    switch (max) {
      case r: h = (g - b) / d + (g < b ? 6 : 0); break;
      case g: h = (b - r) / d + 2; break;
      case b: h = (r - g) / d + 4; break;
    }

    h /= 6;
  }

  s = max == 0 ? 0 : d / max;

  v = max;

  return [ h, s, v ];
}


/**
 * Function to convert rgba format to the hex format with alpha channel
 * Returns a string including a leading hash (#)
 **/
function RGBAToHexA(...rgba) {
  // map alpha to 255
  rgba[3] = Math.round(rgba[3] * 255);

  for (let i = 0; i < rgba.length; i++) {
    // convert to hex system
    rgba[i] = rgba[i].toString(16);
    // append leading 0 if 0
    if (rgba[i].length === 1) rgba[i] = "0" + rgba[i];
  }

  return "#" + rgba.join("");
}


/**
 * Function to convert hex format with alpha channel to the rgba format
 * Returns an array with 4 values
 **/
function HexAToRGBA(hex) {
  if (hex[0] === "#") hex = hex.slice(1);
  const bigint = parseInt(hex, 16);
  return [
    (bigint >> 24) & 255,
    (bigint >> 16) & 255,
    (bigint >> 8) & 255,
    Math.round((bigint & 255) / 255 * 100) / 100
  ];
}