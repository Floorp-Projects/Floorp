/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);

class CssCompatibilityTooltipHelper {
  constructor() {
    this.addTab = this.addTab.bind(this);
  }

  _createElement(doc, tag, classList = [], attributeList = {}) {
    const XHTML_NS = "http://www.w3.org/1999/xhtml";
    const newElement = doc.createElementNS(XHTML_NS, tag);
    for (const elementClass of classList) {
      newElement.classList.add(elementClass);
    }

    for (const key in attributeList) {
      newElement.setAttribute(key, attributeList[key]);
    }

    return newElement;
  }

  /*
   * Attach the UnsupportedBrowserList component to the
   * "".compatibility-browser-list-wrapper" div to render the
   * unsupported browser list
   */
  _renderUnsupportedBrowserList(container, unsupportedBrowsers) {
    // Mount the ReactDOM only if the unsupported browser
    // list is not empty. Else "compatibility-browser-list-wrapper"
    // is not defined. For example, for property clip,
    // unsupportedBrowsers is an empty array
    if (!unsupportedBrowsers.length) {
      return;
    }

    const { require } = BrowserLoader({
      baseURI: "resource://devtools/client/shared/widgets/tooltip/",
      window: this._currentTooltip.doc.defaultView,
    });
    const {
      createFactory,
      createElement,
    } = require("devtools/client/shared/vendor/react");
    const ReactDOM = require("devtools/client/shared/vendor/react-dom");
    const UnsupportedBrowserList = createFactory(
      require("devtools/client/inspector/compatibility/components/UnsupportedBrowserList")
    );

    const unsupportedBrowserList = createElement(UnsupportedBrowserList, {
      browsers: unsupportedBrowsers,
    });
    ReactDOM.render(
      unsupportedBrowserList,
      container.querySelector(".compatibility-browser-list-wrapper")
    );
  }

  /*
   * Get the first paragraph for the compatibility tooltip
   * Return a subtree similar to:
   *   <p data-l10n-id="css-compatibility-default-message"
   *      data-l10n-args="{&quot;property&quot;:&quot;user-select&quot;}">
   *   </p>
   */
  _getCompatibilityMessage(doc, data) {
    const { msgId, property } = data;
    return this._createElement(doc, "p", [], {
      "data-l10n-id": msgId,
      "data-l10n-args": JSON.stringify({ property }),
    });
  }

  /**
   * Gets the paragraph elements related to the browserList.
   * This returns an array with following subtree:
   *   [
   *     <p data-l10n-id="css-compatibility-browser-list-message"></p>,
   *     <p>
   *      <ul class="compatibility-unsupported-browser-list">
   *        <list-element />
   *      </ul>
   *     </p>
   *   ]
   * The first element is the message and the second element is the
   * unsupported browserList itself.
   * If the unsupportedBrowser is an empty array, we return an empty
   * array back.
   */
  _getBrowserListContainer(doc, unsupportedBrowsers) {
    if (!unsupportedBrowsers.length) {
      return null;
    }

    const browserList = this._createElement(doc, "p");
    const browserListWrapper = this._createElement(doc, "div", [
      "compatibility-browser-list-wrapper",
    ]);
    browserList.appendChild(browserListWrapper);

    return browserList;
  }

  /*
   * This is the learn more message element linking to the MDN documentation
   * for the particular incompatible CSS declaration.
   * The element returned is:
   *   <p data-l10n-id="css-compatibility-learn-more-message"
   *       data-l10n-args="{&quot;property&quot;:&quot;user-select&quot;}">
   *     <span data-l10n-name="link" class="link"></span>
   *   </p>
   */
  _getLearnMoreMessage(doc, { rootProperty }) {
    const learnMoreMessage = this._createElement(doc, "p", [], {
      "data-l10n-id": "css-compatibility-learn-more-message",
      "data-l10n-args": JSON.stringify({ rootProperty }),
    });
    learnMoreMessage.appendChild(
      this._createElement(doc, "span", ["link"], {
        "data-l10n-name": "link",
      })
    );

    return learnMoreMessage;
  }

  /**
   * Fill the tooltip with inactive CSS information.
   *
   * @param {Object} data
   *        An object in the following format: {
   *          // Type of compatibility issue
   *          type: <string>,
   *          // The CSS declaration that has compatibility issues
   *          // The raw CSS declaration name that has compatibility issues
   *          declaration: <string>,
   *          property: <string>,
   *          // Alias to the given CSS property
   *          alias: <Array>,
   *          // Link to MDN documentation for the particular CSS rule
   *          url: <string>,
   *          deprecated: <boolean>,
   *          experimental: <boolean>,
   *          // An array of all the browsers that don't support the given CSS rule
   *          unsupportedBrowsers: <Array>,
   *        }
   * @param {HTMLTooltip} tooltip
   *        The tooltip we are targetting.
   */
  async setContent(data, tooltip) {
    const fragment = this.getTemplate(data, tooltip);
    const { doc } = tooltip;

    tooltip.panel.innerHTML = "";

    tooltip.panel.addEventListener("click", this.addTab);
    tooltip.once("hidden", () => {
      tooltip.panel.removeEventListener("click", this.addTab);
    });

    // Because Fluent is async we need to manually translate the fragment and
    // then insert it into the tooltip. This is needed in order for the tooltip
    // to size to the contents properly and for tests.
    await doc.l10n.translateFragment(fragment);
    doc.l10n.pauseObserving();
    tooltip.panel.appendChild(fragment);
    doc.l10n.resumeObserving();

    // Size the content.
    tooltip.setContentSize({ width: 267, height: Infinity });
  }

  /**
   * Get the template that the Fluent string will be merged with. This template
   * looks like this:
   *
   * <div class="devtools-tooltip-css-compatibility">
   *   <p data-l10n-id="css-compatibility-default-message"
   *      data-l10n-args="{&quot;property&quot;:&quot;user-select&quot;}">
   *     <strong></strong>
   *   </p>
   *   <browser-list />
   *   <p data-l10n-id="css-compatibility-learn-more-message"
   *       data-l10n-args="{&quot;property&quot;:&quot;user-select&quot;}">
   *     <span data-l10n-name="link" class="link"></span>
   *     <strong></strong>
   *   </p>
   * </div>
   *
   * @param {Object} data
   *        An object in the following format: {
   *          // Type of compatibility issue
   *          type: <string>,
   *          // The CSS declaration that has compatibility issues
   *          // The raw CSS declaration name that has compatibility issues
   *          declaration: <string>,
   *          property: <string>,
   *          // Alias to the given CSS property
   *          alias: <Array>,
   *          // Link to MDN documentation for the particular CSS rule
   *          url: <string>,
   *          deprecated: <boolean>,
   *          experimental: <boolean>,
   *          // An array of all the browsers that don't support the given CSS rule
   *          unsupportedBrowsers: <Array>,
   *        }
   * @param {HTMLTooltip} tooltip
   *        The tooltip we are targetting.
   */
  getTemplate(data, tooltip) {
    const { doc } = tooltip;
    const { url, unsupportedBrowsers } = data;

    this._currentTooltip = tooltip;
    this._currentUrl = `${url}?utm_source=devtools&utm_medium=inspector-css-compatibility&utm_campaign=default`;
    const templateNode = this._createElement(doc, "template");

    const tooltipContainer = this._createElement(doc, "div", [
      "devtools-tooltip-css-compatibility",
    ]);

    tooltipContainer.appendChild(this._getCompatibilityMessage(doc, data));
    const browserListContainer = this._getBrowserListContainer(
      doc,
      unsupportedBrowsers
    );
    if (browserListContainer) {
      tooltipContainer.appendChild(browserListContainer);
    }

    tooltipContainer.appendChild(this._getLearnMoreMessage(doc, data));
    templateNode.content.appendChild(tooltipContainer);

    this._renderUnsupportedBrowserList(tooltipContainer, unsupportedBrowsers);
    return doc.importNode(templateNode.content, true);
  }

  /**
   * Hide the tooltip, open `this._currentUrl` in a new tab and focus it.
   *
   * @param {DOMEvent} event
   *        The click event originating from the tooltip.
   *
   */
  addTab(event) {
    // The XUL panel swallows click events so handlers can't be added directly
    // to the link span. As a workaround we listen to all click events in the
    // panel and if a link span is clicked we proceed.
    if (event.target.className !== "link") {
      return;
    }

    const tooltip = this._currentTooltip;
    tooltip.hide();
    openDocLink(this._currentUrl);
  }

  destroy() {
    this._currentTooltip = null;
    this._currentUrl = null;
  }
}

module.exports = CssCompatibilityTooltipHelper;
