/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  repeat,
  styleMap,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { escapeRegExp } from "./helpers.mjs";

const NOW_THRESHOLD_MS = 91000;
const FXVIEW_ROW_HEIGHT_PX = 32;
const lazy = {};
let XPCOMUtils;

if (!window.IS_STORYBOOK) {
  XPCOMUtils = ChromeUtils.importESModule(
    "resource://gre/modules/XPCOMUtils.sys.mjs"
  ).XPCOMUtils;
  XPCOMUtils.defineLazyPreferenceGetter(
    lazy,
    "virtualListEnabledPref",
    "browser.firefox-view.virtual-list.enabled"
  );
  ChromeUtils.defineLazyGetter(lazy, "relativeTimeFormat", () => {
    return new Services.intl.RelativeTimeFormat(undefined, {
      style: "narrow",
    });
  });

  ChromeUtils.defineESModuleGetters(lazy, {
    BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  });
}

/**
 * A list of clickable tab items
 *
 * @property {boolean} compactRows - Whether to hide the URL and date/time for each tab.
 * @property {string} dateTimeFormat - Expected format for date and/or time
 * @property {string} hasPopup - The aria-haspopup attribute for the secondary action, if required
 * @property {number} maxTabsLength - The max number of tabs for the list
 * @property {Array} tabItems - Items to show in the tab list
 * @property {string} searchQuery - The query string to highlight, if provided.
 */
export default class FxviewTabList extends MozLitElement {
  constructor() {
    super();
    window.MozXULElement.insertFTLIfNeeded("toolkit/branding/brandings.ftl");
    window.MozXULElement.insertFTLIfNeeded("browser/fxviewTabList.ftl");
    this.activeIndex = 0;
    this.currentActiveElementId = "fxview-tab-row-main";
    this.hasPopup = null;
    this.dateTimeFormat = "relative";
    this.maxTabsLength = 25;
    this.tabItems = [];
    this.compactRows = false;
    this.updatesPaused = true;
    this.#register();
  }

  static properties = {
    activeIndex: { type: Number },
    compactRows: { type: Boolean },
    currentActiveElementId: { type: String },
    dateTimeFormat: { type: String },
    hasPopup: { type: String },
    maxTabsLength: { type: Number },
    tabItems: { type: Array },
    updatesPaused: { type: Boolean },
    searchQuery: { type: String },
  };

  static queries = {
    rowEls: { all: "fxview-tab-row" },
    rootVirtualListEl: "virtual-list",
  };

  willUpdate(changes) {
    this.activeIndex = Math.min(
      Math.max(this.activeIndex, 0),
      this.tabItems.length - 1
    );

    if (changes.has("dateTimeFormat") || changes.has("updatesPaused")) {
      this.clearIntervalTimer();
      if (
        !this.updatesPaused &&
        this.dateTimeFormat == "relative" &&
        !window.IS_STORYBOOK
      ) {
        this.startIntervalTimer();
        this.onIntervalUpdate();
      }
    }

    if (this.maxTabsLength > 0) {
      // Can set maxTabsLength to -1 to have no max
      this.tabItems = this.tabItems.slice(0, this.maxTabsLength);
    }
  }

  startIntervalTimer() {
    this.clearIntervalTimer();
    this.intervalID = setInterval(
      () => this.onIntervalUpdate(),
      this.timeMsPref
    );
  }

  clearIntervalTimer() {
    if (this.intervalID) {
      clearInterval(this.intervalID);
      delete this.intervalID;
    }
  }

  #register() {
    if (!window.IS_STORYBOOK) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "timeMsPref",
        "browser.tabs.firefox-view.updateTimeMs",
        NOW_THRESHOLD_MS,
        (prefName, oldVal, newVal) => {
          this.clearIntervalTimer();
          if (!this.isConnected) {
            return;
          }
          this.startIntervalTimer();
          this.requestUpdate();
        }
      );
    }
  }

  connectedCallback() {
    super.connectedCallback();
    if (
      !this.updatesPaused &&
      this.dateTimeFormat === "relative" &&
      !window.IS_STORYBOOK
    ) {
      this.startIntervalTimer();
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.clearIntervalTimer();
  }

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(Array.from(this.rowEls).map(item => item.updateComplete));
  }

  onIntervalUpdate() {
    this.requestUpdate();
    Array.from(this.rowEls).forEach(fxviewTabRow =>
      fxviewTabRow.requestUpdate()
    );
  }

  /**
   * Focuses the expected element (either the link or button) within fxview-tab-row
   * The currently focused/active element ID within a row is stored in this.currentActiveElementId
   */
  handleFocusElementInRow(e) {
    let fxviewTabRow = e.target;
    if (e.code == "ArrowUp") {
      // Focus either the link or button of the previous row based on this.currentActiveElementId
      e.preventDefault();
      this.focusPrevRow();
    } else if (e.code == "ArrowDown") {
      // Focus either the link or button of the next row based on this.currentActiveElementId
      e.preventDefault();
      this.focusNextRow();
    } else if (e.code == "ArrowRight") {
      // Focus either the link or the button in the current row and
      // set this.currentActiveElementId to that element's ID
      e.preventDefault();
      if (document.dir == "rtl") {
        this.currentActiveElementId = fxviewTabRow.focusLink();
      } else {
        this.currentActiveElementId = fxviewTabRow.focusButton();
      }
    } else if (e.code == "ArrowLeft") {
      // Focus either the link or the button in the current row and
      // set this.currentActiveElementId to that element's ID
      e.preventDefault();
      if (document.dir == "rtl") {
        this.currentActiveElementId = fxviewTabRow.focusButton();
      } else {
        this.currentActiveElementId = fxviewTabRow.focusLink();
      }
    }
  }

  focusPrevRow() {
    this.focusIndex(this.activeIndex - 1);
  }

  focusNextRow() {
    this.focusIndex(this.activeIndex + 1);
  }

  async focusIndex(index) {
    // Focus link or button of item
    if (lazy.virtualListEnabledPref) {
      let row = this.rootVirtualListEl.getItem(index);
      if (!row) {
        return;
      }
      let subList = this.rootVirtualListEl.getSubListForItem(index);
      if (!subList) {
        return;
      }
      this.activeIndex = index;

      // In Bug 1866845, these manual updates to the sublists should be removed
      // and scrollIntoView() should also be iterated on so that we aren't constantly
      // moving the focused item to the center of the viewport
      for (const sublist of Array.from(this.rootVirtualListEl.children)) {
        await sublist.requestUpdate();
        await sublist.updateComplete;
      }
      row.scrollIntoView({ block: "center" });
      row.focus();
    } else if (index >= 0 && index < this.rowEls?.length) {
      this.rowEls[index].focus();
      this.activeIndex = index;
    }
  }

  shouldUpdate(changes) {
    if (changes.has("updatesPaused")) {
      if (this.updatesPaused) {
        this.clearIntervalTimer();
      }
    }
    return !this.updatesPaused;
  }

  itemTemplate = (tabItem, i) => {
    let time;
    if (tabItem.time || tabItem.closedAt) {
      let stringTime = (tabItem.time || tabItem.closedAt).toString();
      // Different APIs return time in different units, so we use
      // the length to decide if it's milliseconds or nanoseconds.
      if (stringTime.length === 16) {
        time = (tabItem.time || tabItem.closedAt) / 1000;
      } else {
        time = tabItem.time || tabItem.closedAt;
      }
    }
    return html`
      <fxview-tab-row
        exportparts="secondary-button"
        ?active=${i == this.activeIndex}
        ?compact=${this.compactRows}
        .hasPopup=${this.hasPopup}
        .currentActiveElementId=${this.currentActiveElementId}
        .dateTimeFormat=${this.dateTimeFormat}
        .favicon=${tabItem.icon}
        .primaryL10nId=${tabItem.primaryL10nId}
        .primaryL10nArgs=${ifDefined(tabItem.primaryL10nArgs)}
        role="listitem"
        .secondaryL10nId=${tabItem.secondaryL10nId}
        .secondaryL10nArgs=${ifDefined(tabItem.secondaryL10nArgs)}
        .sourceClosedId=${ifDefined(tabItem.sourceClosedId)}
        .sourceWindowId=${ifDefined(tabItem.sourceWindowId)}
        .closedId=${ifDefined(tabItem.closedId || tabItem.closedId)}
        .searchQuery=${ifDefined(this.searchQuery)}
        .tabElement=${ifDefined(tabItem.tabElement)}
        .time=${ifDefined(time)}
        .timeMsPref=${ifDefined(this.timeMsPref)}
        .title=${tabItem.title}
        .url=${tabItem.url}
      ></fxview-tab-row>
    `;
  };

  render() {
    if (this.searchQuery && this.tabItems.length === 0) {
      return this.#emptySearchResultsTemplate();
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/fxview-tab-list.css"
      />
      <div
        id="fxview-tab-list"
        class="fxview-tab-list"
        role="list"
        @keydown=${this.handleFocusElementInRow}
      >
        ${when(
          lazy.virtualListEnabledPref,
          () => html`
            <virtual-list
              .activeIndex=${this.activeIndex}
              .items=${this.tabItems}
              .template=${this.itemTemplate}
            ></virtual-list>
          `
        )}
        ${when(
          !lazy.virtualListEnabledPref,
          () => html`
            ${this.tabItems.map((tabItem, i) => this.itemTemplate(tabItem, i))}
          `
        )}
      </div>
      <slot name="menu"></slot>
    `;
  }

  #emptySearchResultsTemplate() {
    return html` <fxview-empty-state
      class="search-results"
      headerLabel="firefoxview-search-results-empty"
      .headerArgs=${{ query: this.searchQuery }}
      isInnerCard
    >
    </fxview-empty-state>`;
  }
}
customElements.define("fxview-tab-list", FxviewTabList);

/**
 * A tab item that displays favicon, title, url, and time of last access
 *
 * @property {boolean} active - Should current item have focus on keydown
 * @property {boolean} compact - Whether to hide the URL and date/time for this tab.
 * @property {string} currentActiveElementId - ID of currently focused element within each tab item
 * @property {string} dateTimeFormat - Expected format for date and/or time
 * @property {string} hasPopup - The aria-haspopup attribute for the secondary action, if required
 * @property {number} closedId - The tab ID for when the tab item was closed.
 * @property {number} sourceClosedId - The closedId of the closed window its from if applicable
 * @property {number} sourceWindowId - The sessionstore id of the window its from if applicable
 * @property {string} favicon - The favicon for the tab item.
 * @property {string} primaryL10nId - The l10n id used for the primary action element
 * @property {string} primaryL10nArgs - The l10n args used for the primary action element
 * @property {string} secondaryL10nId - The l10n id used for the secondary action button
 * @property {string} secondaryL10nArgs - The l10n args used for the secondary action element
 * @property {object} tabElement - The MozTabbrowserTab element for the tab item.
 * @property {number} time - The timestamp for when the tab was last accessed.
 * @property {string} title - The title for the tab item.
 * @property {string} url - The url for the tab item.
 * @property {number} timeMsPref - The frequency in milliseconds of updates to relative time
 * @property {string} searchQuery - The query string to highlight, if provided.
 */
export class FxviewTabRow extends MozLitElement {
  constructor() {
    super();
    this.active = false;
    this.currentActiveElementId = "fxview-tab-row-main";
  }

  static properties = {
    active: { type: Boolean },
    compact: { type: Boolean },
    currentActiveElementId: { type: String },
    dateTimeFormat: { type: String },
    favicon: { type: String },
    hasPopup: { type: String },
    primaryL10nId: { type: String },
    primaryL10nArgs: { type: String },
    secondaryL10nId: { type: String },
    secondaryL10nArgs: { type: String },
    closedId: { type: Number },
    sourceClosedId: { type: Number },
    sourceWindowId: { type: String },
    tabElement: { type: Object },
    time: { type: Number },
    title: { type: String },
    timeMsPref: { type: Number },
    url: { type: String },
    searchQuery: { type: String },
  };

  static queries = {
    mainEl: ".fxview-tab-row-main",
    buttonEl: ".fxview-tab-row-button:not([hidden])",
  };

  get currentFocusable() {
    return this.renderRoot.getElementById(this.currentActiveElementId);
  }

  focus() {
    this.currentFocusable.focus();
  }

  focusButton() {
    this.buttonEl.focus();
    return this.buttonEl.id;
  }

  focusLink() {
    this.mainEl.focus();
    return this.mainEl.id;
  }

  dateFluentArgs(timestamp, dateTimeFormat) {
    if (dateTimeFormat === "date" || dateTimeFormat === "dateTime") {
      return JSON.stringify({ date: timestamp });
    }
    return null;
  }

  dateFluentId(timestamp, dateTimeFormat, _nowThresholdMs = NOW_THRESHOLD_MS) {
    if (!timestamp) {
      return null;
    }
    if (dateTimeFormat === "relative") {
      const elapsed = Date.now() - timestamp;
      if (elapsed <= _nowThresholdMs || !lazy.relativeTimeFormat) {
        // Use a different string for very recent timestamps
        return "fxviewtabrow-just-now-timestamp";
      }
      return null;
    } else if (dateTimeFormat === "date" || dateTimeFormat === "dateTime") {
      return "fxviewtabrow-date";
    }
    return null;
  }

  relativeTime(timestamp, dateTimeFormat, _nowThresholdMs = NOW_THRESHOLD_MS) {
    if (dateTimeFormat === "relative") {
      const elapsed = Date.now() - timestamp;
      if (elapsed > _nowThresholdMs && lazy.relativeTimeFormat) {
        return lazy.relativeTimeFormat.formatBestUnit(new Date(timestamp));
      }
    }
    return null;
  }

  timeFluentId(dateTimeFormat) {
    if (dateTimeFormat === "time" || dateTimeFormat === "dateTime") {
      return "fxviewtabrow-time";
    }
    return null;
  }

  formatURIForDisplay(uriString) {
    return !window.IS_STORYBOOK
      ? lazy.BrowserUtils.formatURIStringForDisplay(uriString)
      : uriString;
  }

  getImageUrl(icon, targetURI) {
    if (window.IS_STORYBOOK) {
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    if (!icon) {
      if (targetURI?.startsWith("moz-extension")) {
        return "chrome://mozapps/skin/extensions/extension.svg";
      }
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    // If the icon is not for website (doesn't begin with http), we
    // display it directly. Otherwise we go through the page-icon
    // protocol to try to get a cached version. We don't load
    // favicons directly.
    if (icon.startsWith("http")) {
      return `page-icon:${targetURI}`;
    }
    return icon;
  }

  primaryActionHandler(event) {
    if (
      (event.type == "click" && !event.altKey) ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      event.preventDefault();
      if (!window.IS_STORYBOOK) {
        this.dispatchEvent(
          new CustomEvent("fxview-tab-list-primary-action", {
            bubbles: true,
            composed: true,
            detail: { originalEvent: event, item: this },
          })
        );
      }
    }
  }

  secondaryActionHandler(event) {
    if (
      (event.type == "click" && event.detail && !event.altKey) ||
      // detail=0 is from keyboard
      (event.type == "click" && !event.detail)
    ) {
      event.preventDefault();
      this.dispatchEvent(
        new CustomEvent("fxview-tab-list-secondary-action", {
          bubbles: true,
          composed: true,
          detail: { originalEvent: event, item: this },
        })
      );
    }
  }

  render() {
    const title = this.title;
    const relativeString = this.relativeTime(
      this.time,
      this.dateTimeFormat,
      !window.IS_STORYBOOK ? this.timeMsPref : NOW_THRESHOLD_MS
    );
    const dateString = this.dateFluentId(
      this.time,
      this.dateTimeFormat,
      !window.IS_STORYBOOK ? this.timeMsPref : NOW_THRESHOLD_MS
    );
    const dateArgs = this.dateFluentArgs(this.time, this.dateTimeFormat);
    const timeString = this.timeFluentId(this.dateTimeFormat);
    const time = this.time;
    const timeArgs = JSON.stringify({ time });
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/fxview-tab-row.css"
      />
      <a
        href=${ifDefined(this.url)}
        class="fxview-tab-row-main"
        id="fxview-tab-row-main"
        tabindex=${this.active &&
        this.currentActiveElementId === "fxview-tab-row-main"
          ? "0"
          : "-1"}
        data-l10n-id=${ifDefined(this.primaryL10nId)}
        data-l10n-args=${ifDefined(this.primaryL10nArgs)}
        @click=${this.primaryActionHandler}
        @keydown=${this.primaryActionHandler}
      >
        <span
          class="fxview-tab-row-favicon icon"
          id="fxview-tab-row-favicon"
          style=${styleMap({
            backgroundImage: `url(${this.getImageUrl(this.favicon, this.url)})`,
          })}
        ></span>
        <span
          class="fxview-tab-row-title text-truncated-ellipsis"
          id="fxview-tab-row-title"
          dir="auto"
        >
          ${when(
            this.searchQuery,
            () => this.#highlightSearchMatches(this.searchQuery, title),
            () => title
          )}
        </span>
        <span
          class="fxview-tab-row-url text-truncated-ellipsis"
          id="fxview-tab-row-url"
          ?hidden=${this.compact}
        >
          ${when(
            this.searchQuery,
            () =>
              this.#highlightSearchMatches(
                this.searchQuery,
                this.formatURIForDisplay(this.url)
              ),
            () => this.formatURIForDisplay(this.url)
          )}
        </span>
        <span
          class="fxview-tab-row-date"
          id="fxview-tab-row-date"
          ?hidden=${this.compact}
        >
          <span
            ?hidden=${relativeString || !dateString}
            data-l10n-id=${ifDefined(dateString)}
            data-l10n-args=${ifDefined(dateArgs)}
          ></span>
          <span ?hidden=${!relativeString}>${relativeString}</span>
        </span>
        <span
          class="fxview-tab-row-time"
          id="fxview-tab-row-time"
          ?hidden=${this.compact || !timeString}
          data-timestamp=${ifDefined(this.time)}
          data-l10n-id=${ifDefined(timeString)}
          data-l10n-args=${ifDefined(timeArgs)}
        >
        </span>
      </a>
      ${when(
        this.secondaryL10nId && this.secondaryActionHandler,
        () => html`<button
          class="fxview-tab-row-button ghost-button icon-button semi-transparent"
          id="fxview-tab-row-secondary-button"
          part="secondary-button"
          data-l10n-id=${this.secondaryL10nId}
          data-l10n-args=${ifDefined(this.secondaryL10nArgs)}
          aria-haspopup=${ifDefined(this.hasPopup)}
          @click=${this.secondaryActionHandler}
          tabindex="${this.active &&
          this.currentActiveElementId === "fxview-tab-row-secondary-button"
            ? "0"
            : "-1"}"
        ></button>`
      )}
    `;
  }

  /**
   * Find all matches of query within the given string, and compute the result
   * to be rendered.
   *
   * @param {string} query
   * @param {string} string
   */
  #highlightSearchMatches(query, string) {
    const fragments = [];
    const regex = RegExp(escapeRegExp(query), "dgi");
    let prevIndexEnd = 0;
    let result;
    while ((result = regex.exec(string)) !== null) {
      const [indexStart, indexEnd] = result.indices[0];
      fragments.push(string.substring(prevIndexEnd, indexStart));
      fragments.push(
        html`<strong>${string.substring(indexStart, indexEnd)}</strong>`
      );
      prevIndexEnd = regex.lastIndex;
    }
    fragments.push(string.substring(prevIndexEnd));
    return fragments;
  }
}

customElements.define("fxview-tab-row", FxviewTabRow);

export class VirtualList extends MozLitElement {
  static properties = {
    items: { type: Array },
    template: { type: Function },
    activeIndex: { type: Number },
    itemOffset: { type: Number },
    maxRenderCountEstimate: { type: Number, state: true },
    itemHeightEstimate: { type: Number, state: true },
    isAlwaysVisible: { type: Boolean },
    isVisible: { type: Boolean, state: true },
    isSubList: { type: Boolean },
  };

  createRenderRoot() {
    return this;
  }

  constructor() {
    super();
    this.activeIndex = 0;
    this.itemOffset = 0;
    this.items = [];
    this.subListItems = [];
    this.itemHeightEstimate = FXVIEW_ROW_HEIGHT_PX;
    this.maxRenderCountEstimate = Math.max(
      40,
      2 * Math.ceil(window.innerHeight / this.itemHeightEstimate)
    );
    this.isSubList = false;
    this.isVisible = false;
    this.intersectionObserver = new IntersectionObserver(
      ([entry]) => (this.isVisible = entry.isIntersecting),
      { root: this.ownerDocument }
    );
    this.resizeObserver = new ResizeObserver(([entry]) => {
      if (entry.contentRect?.height > 0) {
        // Update properties on top-level virtual-list
        this.parentElement.itemHeightEstimate = entry.contentRect.height;
        this.parentElement.maxRenderCountEstimate = Math.max(
          40,
          2 * Math.ceil(window.innerHeight / this.itemHeightEstimate)
        );
      }
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObserver.disconnect();
    this.resizeObserver.disconnect();
  }

  triggerIntersectionObserver() {
    this.intersectionObserver.unobserve(this);
    this.intersectionObserver.observe(this);
  }

  getSubListForItem(index) {
    if (this.isSubList) {
      throw new Error("Cannot get sublist for item");
    }
    return this.children[parseInt(index / this.maxRenderCountEstimate, 10)];
  }

  getItem(index) {
    if (!this.isSubList) {
      return this.getSubListForItem(index)?.getItem(
        index % this.maxRenderCountEstimate
      );
    }
    return this.children[index];
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("items") && !this.isSubList) {
      this.subListItems = [];
      for (let i = 0; i < this.items.length; i += this.maxRenderCountEstimate) {
        this.subListItems.push(
          this.items.slice(i, i + this.maxRenderCountEstimate)
        );
      }
      this.triggerIntersectionObserver();
    }
  }

  recalculateAfterWindowResize() {
    this.maxRenderCountEstimate = Math.max(
      40,
      2 * Math.ceil(window.innerHeight / this.itemHeightEstimate)
    );
  }

  firstUpdated() {
    this.intersectionObserver.observe(this);
    if (this.isSubList && this.children[0]) {
      this.resizeObserver.observe(this.children[0]);
    }
  }

  updated(changedProperties) {
    this.updateListHeight(changedProperties);
  }

  updateListHeight(changedProperties) {
    if (
      changedProperties.has("isAlwaysVisible") ||
      changedProperties.has("isVisible")
    ) {
      this.style.height =
        this.isAlwaysVisible || this.isVisible
          ? "auto"
          : `${this.items.length * this.itemHeightEstimate}px`;
    }
  }

  get renderItems() {
    return this.isSubList ? this.items : this.subListItems;
  }

  subListTemplate = (data, i) => {
    return html`<virtual-list
      .template=${this.template}
      .items=${data}
      .itemHeightEstimate=${this.itemHeightEstimate}
      .itemOffset=${i * this.maxRenderCountEstimate}
      .isAlwaysVisible=${i ==
      parseInt(this.activeIndex / this.maxRenderCountEstimate, 10)}
      isSubList
    ></virtual-list>`;
  };

  itemTemplate = (data, i) => this.template(data, this.itemOffset + i);

  render() {
    if (this.isAlwaysVisible || this.isVisible) {
      return html`
        ${repeat(
          this.renderItems,
          (data, i) => i,
          this.isSubList ? this.itemTemplate : this.subListTemplate
        )}
      `;
    }
    return "";
  }
}

customElements.define("virtual-list", VirtualList);
