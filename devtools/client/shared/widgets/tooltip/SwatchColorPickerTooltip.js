/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { colorUtils } = require("devtools/shared/css/color");
const Spectrum = require("devtools/client/shared/widgets/Spectrum");
const SwatchBasedEditorTooltip = require("devtools/client/shared/widgets/tooltip/SwatchBasedEditorTooltip");
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);
const { openDocLink } = require("devtools/client/shared/link");
const {
  A11Y_CONTRAST_LEARN_MORE_LINK,
} = require("devtools/client/accessibility/constants");
loader.lazyRequireGetter(this, "throttle", "devtools/shared/throttle", true);

loader.lazyRequireGetter(
  this,
  ["getFocusableElements", "wrapMoveFocus"],
  "devtools/client/shared/focus",
  true
);
loader.lazyRequireGetter(
  this,
  "PICKER_TYPES",
  "devtools/shared/picker-constants"
);

const TELEMETRY_PICKER_EYEDROPPER_OPEN_COUNT =
  "DEVTOOLS_PICKER_EYEDROPPER_OPENED_COUNT";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The swatch color picker tooltip class is a specific class meant to be used
 * along with output-parser's generated color swatches.
 * It extends the parent SwatchBasedEditorTooltip class.
 * It just wraps a standard Tooltip and sets its content with an instance of a
 * color picker.
 *
 * The activeSwatch element expected by the tooltip must follow some guidelines
 * to be compatible with this feature:
 * - the background-color of the activeSwatch should be set to the current
 *   color, it will be updated when the color is changed via the color-picker.
 * - the `data-color` attribute should be set either on the activeSwatch or on
 *   a parent node, and should also contain the current color.
 * - finally if the color value should be displayed next to the swatch as text,
 *   the activeSwatch should have a nextSibling. Note that this sibling may
 *   contain more than just text initially, but it will be updated after a color
 *   change and will only contain the text.
 *
 * An example of valid markup (with data-color on a parent and a nextSibling):
 *
 * <span data-color="#FFF"> <!-- activeSwatch.closest("[data-color]") -->
 *   <span
 *     style="background-color: rgb(255, 255, 255);"
 *   ></span> <!-- activeSwatch -->
 *   <span>#FFF</span> <!-- activeSwatch.nextSibling -->
 * </span>
 *
 * Another example with everything on the activeSwatch itself:
 *
 * <span> <!-- container, to illustrate that the swatch has no sibling here. -->
 *   <span
 *      data-color="#FFF"
 *      style="background-color: rgb(255, 255, 255);"
 *   ></span> <!-- activeSwatch & activeSwatch.closest("[data-color]") -->
 * </span>
 *
 * @param {Document} document
 *        The document to attach the SwatchColorPickerTooltip. This is either the toolbox
 *        document if the tooltip is a popup tooltip or the panel's document if it is an
 *        inline editor.
 * @param {InspectorPanel} inspector
 *        The inspector panel, needed for the eyedropper.
 * @param {Function} supportsCssColor4ColorFunction
 *        A function for checking the supporting of css-color-4 color function.
 */

class SwatchColorPickerTooltip extends SwatchBasedEditorTooltip {
  constructor(document, inspector, { supportsCssColor4ColorFunction }) {
    super(document);
    this.inspector = inspector;

    // Creating a spectrum instance. this.spectrum will always be a promise that
    // resolves to the spectrum instance
    this.spectrum = this.setColorPickerContent([0, 0, 0, 1]);
    this._onSpectrumColorChange = this._onSpectrumColorChange.bind(this);
    this._openEyeDropper = this._openEyeDropper.bind(this);
    this._openDocLink = this._openDocLink.bind(this);
    this._onTooltipKeydown = this._onTooltipKeydown.bind(this);
    this.cssColor4 = supportsCssColor4ColorFunction();

    // Selecting color by hovering on the spectrum widget could create a lot
    // of requests. Throttle by 50ms to avoid this. See Bug 1665547.
    this._selectColor = throttle(this._selectColor.bind(this), 50);

    this.tooltip.container.addEventListener("keydown", this._onTooltipKeydown);
  }

  /**
   * Fill the tooltip with a new instance of the spectrum color picker widget
   * initialized with the given color, and return the instance of spectrum
   */

  setColorPickerContent(color) {
    const { doc } = this.tooltip;
    this.tooltip.panel.innerHTML = "";

    const container = doc.createElementNS(XHTML_NS, "div");
    container.id = "spectrum-tooltip";

    const node = doc.createElementNS(XHTML_NS, "div");
    node.id = "spectrum";
    container.appendChild(node);

    const widget = new Spectrum(node, color);
    this.tooltip.panel.appendChild(container);
    this.tooltip.setContentSize({ width: 215 });

    widget.inspector = this.inspector;

    // Wait for the tooltip to be shown before calling widget.show
    // as it expect to be visible in order to compute DOM element sizes.
    this.tooltip.once("shown", () => {
      widget.show();
    });

    return widget;
  }

  /**
   * Overriding the SwatchBasedEditorTooltip.show function to set spectrum's
   * color.
   */
  async show() {
    // set contrast enabled for the spectrum
    const name = this.activeSwatch.dataset.propertyName;

    // Only enable contrast if the type of property is color.
    this.spectrum.contrastEnabled = name === "color";
    if (this.spectrum.contrastEnabled) {
      const { nodeFront } = this.inspector.selection;
      const { pageStyle } = nodeFront.inspectorFront;
      this.spectrum.textProps = await pageStyle.getComputed(nodeFront, {
        filterProperties: ["font-size", "font-weight", "opacity"],
      });
      this.spectrum.backgroundColorData = await nodeFront.getBackgroundColor();
    }

    // Then set spectrum's color and listen to color changes to preview them
    if (this.activeSwatch) {
      this._originalColor = this._getSwatchColorContainer().dataset.color;
      const color = this.activeSwatch.style.backgroundColor;

      this.spectrum.off("changed", this._onSpectrumColorChange);
      this.spectrum.rgb = this._colorToRgba(color);
      this.spectrum.on("changed", this._onSpectrumColorChange);
      this.spectrum.updateUI();
    }

    // Call then parent class' show function
    await super.show();

    const eyeButton = this.tooltip.container.querySelector(
      "#eyedropper-button"
    );
    const canShowEyeDropper = await this.inspector.supportsEyeDropper();
    if (canShowEyeDropper) {
      eyeButton.disabled = false;
      eyeButton.removeAttribute("title");
      eyeButton.addEventListener("click", this._openEyeDropper);
    } else {
      eyeButton.disabled = true;
      eyeButton.title = L10N.getStr("eyedropper.disabled.title");
    }

    const learnMoreButton = this.tooltip.container.querySelector(
      "#learn-more-button"
    );
    if (learnMoreButton) {
      learnMoreButton.addEventListener("click", this._openDocLink);
      learnMoreButton.addEventListener("keydown", e => e.stopPropagation());
    }

    // Add focus to the first focusable element in the tooltip and attach keydown
    // event listener to tooltip
    this.focusableElements[0].focus();
    this.tooltip.container.addEventListener(
      "keydown",
      this._onTooltipKeydown,
      true
    );

    this.emit("ready");
  }

  _onTooltipKeydown(event) {
    const { target, key, shiftKey } = event;

    if (key !== "Tab") {
      return;
    }

    const focusMoved = !!wrapMoveFocus(
      this.focusableElements,
      target,
      shiftKey
    );
    if (focusMoved) {
      // Focus was moved to the begining/end of the tooltip, so we need to prevent the
      // default focus change that would happen here.
      event.preventDefault();
    }

    event.stopPropagation();
  }

  _getSwatchColorContainer() {
    // Depending on the UI, the data-color attribute might be set on the
    // swatch itself, or a parent node.
    // This data attribute is also used for the "Copy color" feature, so it
    // can be useful to set it on a container rather than on the swatch.
    return this.activeSwatch.closest("[data-color]");
  }

  _onSpectrumColorChange(rgba, cssColor) {
    this._selectColor(cssColor);
  }

  _selectColor(color) {
    if (this.activeSwatch) {
      this.activeSwatch.style.backgroundColor = color;

      color = this._toDefaultType(color);

      this._getSwatchColorContainer().dataset.color = color;
      if (this.activeSwatch.nextSibling) {
        this.activeSwatch.nextSibling.textContent = color;
      }
      this.preview(color);

      if (this.eyedropperOpen) {
        this.commit();
      }
    }
  }

  /**
   * Override the implementation from SwatchBasedEditorTooltip.
   */
  onTooltipHidden() {
    // If the tooltip is hidden while the eyedropper is being used, we should not commit
    // the changes.
    if (this.eyedropperOpen) {
      return;
    }

    super.onTooltipHidden();
    this.tooltip.container.removeEventListener(
      "keydown",
      this._onTooltipKeydown
    );
  }

  _openEyeDropper() {
    const { inspectorFront, toolbox, telemetry } = this.inspector;

    telemetry
      .getHistogramById(TELEMETRY_PICKER_EYEDROPPER_OPEN_COUNT)
      .add(true);

    // cancelling picker(if it is already selected) on opening eye-dropper
    toolbox.nodePicker.cancel();

    // disable simulating touch events if RDM is active
    toolbox.tellRDMAboutPickerState(true, PICKER_TYPES.EYEDROPPER);

    // pickColorFromPage will focus the content document. If the devtools are in a
    // separate window, the colorpicker tooltip will be closed before pickColorFromPage
    // resolves. Flip the flag early to avoid issues with onTooltipHidden().
    this.eyedropperOpen = true;

    inspectorFront.pickColorFromPage({ copyOnSelect: false }).then(() => {
      // close the colorpicker tooltip so that only the eyedropper is open.
      this.hide();

      this.tooltip.emit("eyedropper-opened");
    }, console.error);

    inspectorFront.once("color-picked", color => {
      toolbox.win.focus();
      this._selectColor(color);
      this._onEyeDropperDone();
    });

    inspectorFront.once("color-pick-canceled", () => {
      this._onEyeDropperDone();
    });
  }

  _openDocLink() {
    openDocLink(A11Y_CONTRAST_LEARN_MORE_LINK);
    this.hide();
  }

  _onEyeDropperDone() {
    // enable simulating touch events if RDM is active
    this.inspector.toolbox.tellRDMAboutPickerState(
      false,
      PICKER_TYPES.EYEDROPPER
    );

    this.eyedropperOpen = false;
    this.activeSwatch = null;
  }

  _colorToRgba(color) {
    color = new colorUtils.CssColor(color, this.cssColor4);
    const rgba = color.getRGBATuple();
    return [rgba.r, rgba.g, rgba.b, rgba.a];
  }

  _toDefaultType(color) {
    const colorObj = new colorUtils.CssColor(color);
    colorObj.setAuthoredUnitFromColor(this._originalColor, this.cssColor4);
    return colorObj.toString();
  }

  /**
   * Overriding the SwatchBasedEditorTooltip.isEditing function to consider the
   * eyedropper.
   */
  isEditing() {
    return this.tooltip.isVisible() || this.eyedropperOpen;
  }

  get focusableElements() {
    return getFocusableElements(this.tooltip.container).filter(
      el => !!el.offsetParent
    );
  }

  destroy() {
    super.destroy();
    this.inspector = null;
    this.spectrum.off("changed", this._onSpectrumColorChange);
    this.spectrum.destroy();
  }
}

module.exports = SwatchColorPickerTooltip;
