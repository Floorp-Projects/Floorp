/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

import {
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import Tab from '/common/Tab.js';

import TabFavIconHelper from '/extlib/TabFavIconHelper.js';

import { kTAB_TWISTY_ELEMENT_NAME } from './TabTwistyElement.js';
import { kTAB_FAVICON_ELEMENT_NAME } from './TabFaviconElement.js';
import { kTAB_LABEL_ELEMENT_NAME } from './TabLabelElement.js';
import { kTAB_COUNTER_ELEMENT_NAME } from './TabCounterElement.js';
import { kTAB_SOUND_BUTTON_ELEMENT_NAME } from './TabSoundButtonElement.js';
import { kTAB_CLOSE_BOX_ELEMENT_NAME } from './TabCloseBoxElement.js';

export const kTAB_ELEMENT_NAME = 'tab-item';
export const kTAB_SUBSTANCE_ELEMENT_NAME = 'tab-item-substance';

export const TabInvalidationTarget = Object.freeze({
  Twisty:      1 << 0,
  SoundButton: 1 << 1,
  CloseBox:    1 << 2,
  Tooltip:     1 << 3,
  All:         1 << 0 | 1 << 1 | 1 << 2 | 1 << 3,
});

export const TabUpdateTarget = Object.freeze({
  Counter:                1 << 0,
  Overflow:               1 << 1,
  DescendantsHighlighted: 1 << 2,
  CollapseExpandState:    1 << 3,
  TabProperties:          1 << 4,
  All:                    1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4,
});

const kTAB_CLASS_NAME = 'tab';

const NATIVE_PROPERTIES = new Set([
  'active',
  'attention',
  'audible',
  'discarded',
  'hidden',
  'highlighted',
  'pinned'
]);
const IGNORE_CLASSES = new Set([
  'tab',
  Constants.kTAB_STATE_ANIMATION_READY,
  Constants.kTAB_STATE_SUBTREE_COLLAPSED
]);

export class TabElement extends HTMLElement {
  static define() {
    window.customElements.define(kTAB_ELEMENT_NAME, TabElement);
  }

  constructor() {
    super();

    // We should initialize private properties with blank value for better performance with a fixed shape.
    this._reservedUpdateTooltip = null;
    this.__onMouseOver = null;
    this.__onWindowResize = null;
    this.__onConfigChange = null;
  }

  connectedCallback() {
    this.setAttribute('role', 'option');

    if (this.initialized) {
      this.initializeContents();
      this.invalidate(TabInvalidationTarget.All);
      this.update(TabUpdateTarget.TabProperties);
      this._applyAttributes();
      this._initExtraItemsContainers();
      this._startListening();
      return;
    }

    // I make ensure to call these operation only once conservatively because:
    //  * If we do these operations in a constructor of this class, Gecko throws `NotSupportedError: Operation is not supported`.
    //    * I'm not familiar with details of the spec, but this is not Gecko's bug.
    //      See https://dom.spec.whatwg.org/#concept-create-element
    //      "6. If result has children, then throw a "NotSupportedError" DOMException."
    //  * `connectedCallback()` may be called multiple times by append/remove operations.
    //
    // FIXME:
    //  Ideally, these descendants should be in shadow tree. Thus I don't change these element to custom elements.
    //  However, I hesitate to do it at this moment by these reasons.
    //  If we move these to shadow tree,
    //    * We need some rewrite our style.
    //      * This includes that we need to move almost CSS code into this file as a string.
    //    * I'm not sure about that whether we should require [CSS Shadow Parts](https://bugzilla.mozilla.org/show_bug.cgi?id=1559074).
    //      * I suspect we can resolve almost problems by using CSS Custom Properties.

    // We preserve this class for backward compatibility with other addons.
    this.classList.add(kTAB_CLASS_NAME);

    const extraItemsContainerIndent = document.createElement('span');
    extraItemsContainerIndent.classList.add(Constants.kEXTRA_ITEMS_CONTAINER);
    extraItemsContainerIndent.classList.add('indent');
    this.appendChild(extraItemsContainerIndent);

    const substance = document.createElement(kTAB_SUBSTANCE_ELEMENT_NAME);
    substance.setAttribute('draggable', true);
    this.appendChild(substance);

    const background = document.createElement('span');
    background.classList.add(Constants.kBACKGROUND);
    substance.appendChild(background);

    const label = document.createElement(kTAB_LABEL_ELEMENT_NAME);
    substance.appendChild(label);

    const twisty = document.createElement(kTAB_TWISTY_ELEMENT_NAME);
    substance.insertBefore(twisty, label);

    const favicon = document.createElement(kTAB_FAVICON_ELEMENT_NAME);
    substance.insertBefore(favicon, label);

    const counter = document.createElement(kTAB_COUNTER_ELEMENT_NAME);
    substance.appendChild(counter);

    const soundButton = document.createElement(kTAB_SOUND_BUTTON_ELEMENT_NAME);
    substance.appendChild(soundButton);

    const closebox = document.createElement(kTAB_CLOSE_BOX_ELEMENT_NAME);
    substance.appendChild(closebox);

    const burster = document.createElement('span');
    burster.classList.add(Constants.kBURSTER);
    substance.appendChild(burster);

    const activeMarker = document.createElement('span');
    activeMarker.classList.add(Constants.kHIGHLIGHTER);
    substance.appendChild(activeMarker);

    const identityMarker = document.createElement('span');
    identityMarker.classList.add(Constants.kCONTEXTUAL_IDENTITY_MARKER);
    substance.appendChild(identityMarker);

    const extraItemsContainerBehind = document.createElement('span');
    extraItemsContainerBehind.classList.add(Constants.kEXTRA_ITEMS_CONTAINER);
    extraItemsContainerBehind.classList.add('behind');
    substance.appendChild(extraItemsContainerBehind);

    const extraItemsContainerFront = document.createElement('span');
    extraItemsContainerFront.classList.add(Constants.kEXTRA_ITEMS_CONTAINER);
    extraItemsContainerFront.classList.add('front');
    substance.appendChild(extraItemsContainerFront);

    this.removeAttribute('draggable');

    this.initializeContents();
    this.invalidate(TabInvalidationTarget.All);
    this.update(TabUpdateTarget.TabProperties);
    this._initExtraItemsContainers();
    this._applyAttributes();
    this._startListening();
  }

  disconnectedCallback() {
    this._endListening();
  }

  get initialized() {
    return !!this._substanceElement;
  }

  initializeContents() {
    // This can be called after the tab is removed, so
    // we need to initialize contents safely.
    if (this._labelElement) {
      if (!this._labelElement.owner) {
        this._labelElement.addOverflowChangeListener(() => {
          if (!this.$TST ||
              this.$TST.tab.pinned)
            return;
          this.invalidateTooltip();
        });
      }
      this._labelElement.owner = this;
    }
    if (this._twistyElement) {
      this._twistyElement.owner = this;
      this._twistyElement.makeAccessible();
    }
    if (this._counterElement)
      this._counterElement.owner = this;
    if (this._soundButtonElement) {
      this._soundButtonElement.owner = this;
      this._soundButtonElement.makeAccessible();
    }
    if (this.closeBoxElement) {
      this.closeBoxElement.owner = this;
      this.closeBoxElement.makeAccessible();
    }
  }

  // Elements restored from cache are initialized without bundled tabs.
  // Thus we provide abiltiy to get tab and service objects from cached/restored information.
  get tab() {
    return this._tab || (this._tab = Tab.get(this.getAttribute(Constants.kAPI_TAB_ID)));
  }
  set tab(value) {
    return this._tab = value;
  }

  get $TST() {
    return this._$TST || (this._$TST = this.tab && this.tab.$TST);
  }
  set $TST(value) {
    return this._$TST = value;
  }

  get _substanceElement() {
    return this.querySelector(kTAB_SUBSTANCE_ELEMENT_NAME);
  }

  get _twistyElement() {
    return this.querySelector(kTAB_TWISTY_ELEMENT_NAME);
  }

  get _favIconElement() {
    return this.querySelector(kTAB_FAVICON_ELEMENT_NAME);
  }

  get _labelElement() {
    return this.querySelector(kTAB_LABEL_ELEMENT_NAME);
  }

  get _soundButtonElement() {
    return this.querySelector(kTAB_SOUND_BUTTON_ELEMENT_NAME);
  }

  get _counterElement() {
    return this.querySelector(kTAB_COUNTER_ELEMENT_NAME);
  }

  get closeBoxElement() {
    return this.querySelector(kTAB_CLOSE_BOX_ELEMENT_NAME);
  }

  _applyAttributes() {
    this._labelElement.value = this.dataset.title;
    this.favIconUrl = this._favIconUrl;
    this.setAttribute('aria-selected', this.classList.contains(Constants.kTAB_STATE_HIGHLIGHTED) ? 'true' : 'false');

    // for convenience on customization with custom user styles
    this._substanceElement.setAttribute(Constants.kAPI_TAB_ID, this.getAttribute(Constants.kAPI_TAB_ID));
    this._substanceElement.setAttribute(Constants.kAPI_WINDOW_ID, this.getAttribute(Constants.kAPI_WINDOW_ID));
    this._labelElement.setAttribute(Constants.kAPI_TAB_ID, this.getAttribute(Constants.kAPI_TAB_ID));
    this._labelElement.setAttribute(Constants.kAPI_WINDOW_ID, this.getAttribute(Constants.kAPI_WINDOW_ID));
  }

  invalidate(targets) {
    if (!this.initialized)
      return;

    if (targets & TabInvalidationTarget.Twisty) {
      const twisty = this._twistyElement;
      if (twisty)
        twisty.invalidate();
    }

    if (targets & TabInvalidationTarget.SoundButton) {
      const soundButton = this._soundButtonElement;
      if (soundButton)
        soundButton.invalidate();
    }

    if (targets & TabInvalidationTarget.CloseBox) {
      const closeBox = this.closeBoxElement;
      if (closeBox)
        closeBox.invalidate();
    }

    if (targets & TabInvalidationTarget.Tooltip)
      this.invalidateTooltip();

    if (targets & TabInvalidationTarget.Overflow)
      this._needToUpdateOverflow = true;
  }

  invalidateTooltip() {
    if (this._reservedUpdateTooltip)
      return;

    this._reservedUpdateTooltip = () => {
      this._reservedUpdateTooltip = null;
      this._updateTooltip();
    };
    this.addEventListener('mouseover', this._reservedUpdateTooltip, { once: true });
  }

  update(targets) {
    if (!this.initialized)
      return;

    if (targets & TabUpdateTarget.Counter) {
      const counter = this._counterElement;
      if (counter)
        counter.update();
    }

    if (targets & TabUpdateTarget.Overflow)
      this._updateOverflow();

    if (targets & TabUpdateTarget.DescendantsHighlighted)
      this._updateDescendantsHighlighted();

    if (targets & TabUpdateTarget.CollapseExpandState)
      this._updateCollapseExpandState();

    if (targets & TabUpdateTarget.TabProperties)
      this._updateTabProperties();
  }

  updateOverflow() {
    if (this._needToUpdateOverflow || configs.labelOverflowStyle == 'fade')
      this._updateOverflow();
    this.invalidateTooltip();
  }

  _updateOverflow() {
    this._needToUpdateOverflow = false;
    const label = this._labelElement;
    if (label)
      label.updateOverflow();
  }

  _updateTooltip() {
    if (!this.$TST) // called before binding on restoration from cache
      return;

    const tab = this.$TST.tab;
    if (!TabsStore.ensureLivingTab(tab))
      return;

    if (configs.debug) {
      this.tooltip = `
${tab.title}
#${tab.id}
(${this.$TST.element.className})
uniqueId = <${this.$TST.uniqueId.id}>
duplicated = <${!!this.$TST.uniqueId.duplicated}> / <${this.$TST.uniqueId.originalTabId}> / <${this.$TST.uniqueId.originalId}>
restored = <${!!this.$TST.uniqueId.restored}>
tabId = ${tab.id}
windowId = ${tab.windowId}
`.trim();
      this.$TST.setAttribute('title', this.tooltip);
      return;
    }

    this.tooltip = this.$TST.cookieStoreName ? `${tab.title} - ${this.$TST.cookieStoreName}` : tab.title;
    this.tooltipWithDescendants = this._getTooltipWithDescendants(tab);

    if (configs.showCollapsedDescendantsByTooltip &&
        this.$TST.subtreeCollapsed &&
        this.$TST.hasChild) {
      this.$TST.setAttribute('title', this.tooltipWithDescendants);
    }
    else if (this.classList.contains('faviconized') || this.overflow || this.tooltip != tab.title) {
      this.$TST.setAttribute('title', this.tooltip);
    }
    else {
      this.$TST.removeAttribute('title');
    }
  }
  _getTooltipWithDescendants(tab) {
    const tooltip = [`* ${tab.$TST.element.tooltip || tab.title}`];
    for (const child of tab.$TST.children) {
      if (!child.$TST.element.tooltipWithDescendants)
        child.$TST.element.tooltipWithDescendants = this._getTooltipWithDescendants(child);
      tooltip.push(child.$TST.element.tooltipWithDescendants.replace(/^/gm, '  '));
    }
    return tooltip.join('\n');
  }

  _initExtraItemsContainers() {
    if (!this.extraItemsContainerIndentRoot) {
      this.extraItemsContainerIndentRoot = this.querySelector(`.${Constants.kEXTRA_ITEMS_CONTAINER}.indent`).attachShadow({ mode: 'open' });
      this.extraItemsContainerIndentRoot.itemById = new Map();
    }
    if (!this.extraItemsContainerBehindRoot) {
      this.extraItemsContainerBehindRoot = this.querySelector(`.${Constants.kEXTRA_ITEMS_CONTAINER}.behind`).attachShadow({ mode: 'open' });
      this.extraItemsContainerBehindRoot.itemById = new Map();
    }
    if (!this.extraItemsContainerFrontRoot) {
      this.extraItemsContainerFrontRoot = this.querySelector(`.${Constants.kEXTRA_ITEMS_CONTAINER}.front`).attachShadow({ mode: 'open' });
      this.extraItemsContainerFrontRoot.itemById = new Map();
    }
  }

  _startListening() {
    if (this.__onMouseOver)
      return;
    this.addEventListener('mouseover', this.__onMouseOver = this._onMouseOver.bind(this));
    window.addEventListener('resize', this.__onWindowResize = this._onWindowResize.bind(this));
    configs.$addObserver(this.__onConfigChange = this._onConfigChange.bind(this));
  }

  _endListening() {
    if (!this.__onMouseOver)
      return;
    this.removeEventListener('mouseover', this.__onMouseOver);
    this.__onMouseOver = null;
    window.removeEventListener('resize', this.__onWindowResize);
    this.__onWindowResize = null;
    configs.$removeObserver(this.__onConfigChange);
    this.__onConfigChange = null;
  }

  _onMouseOver(_event) {
    this._updateTabAndAncestorsTooltip(this.$TST.tab);
  }

  _onWindowResize(_event) {
    this.invalidateTooltip();
  }

  _onConfigChange(changedKey) {
    switch (changedKey) {
      case 'showCollapsedDescendantsByTooltip':
        this.invalidateTooltip();
        break;

      case 'labelOverflowStyle':
        this.updateOverflow();
        break;
    }
  }

  _updateTabAndAncestorsTooltip(tab) {
    if (!TabsStore.ensureLivingTab(tab))
      return;
    for (const updateTab of [tab].concat(tab.$TST.ancestors)) {
      updateTab.$TST.element.invalidateTooltip();
      // on the "fade" mode, overflow style was already updated,
      // so we don' need to update the status here.
      if (configs.labelOverflowStyle != 'fade')
        updateTab.$TST.element.updateOverflow();
    }
  }

  _updateDescendantsHighlighted() {
    if (!this.$TST) // called before binding on restoration from cache
      return;

    const children = this.$TST.children;
    if (!this.$TST.hasChild) {
      this.$TST.removeState(Constants.kTAB_STATE_SOME_DESCENDANTS_HIGHLIGHTED);
      this.$TST.removeState(Constants.kTAB_STATE_ALL_DESCENDANTS_HIGHLIGHTED);
      return;
    }
    let someHighlighted = false;
    let allHighlighted  = true;
    for (const child of children) {
      if (child.$TST.states.has(Constants.kTAB_STATE_HIGHLIGHTED)) {
        someHighlighted = true;
        allHighlighted = (
          allHighlighted &&
          (!child.$TST.hasChild ||
           child.$TST.states.has(Constants.kTAB_STATE_ALL_DESCENDANTS_HIGHLIGHTED))
        );
      }
      else {
        if (!someHighlighted &&
            child.$TST.states.has(Constants.kTAB_STATE_SOME_DESCENDANTS_HIGHLIGHTED)) {
          someHighlighted = true;
        }
        allHighlighted = false;
      }
    }
    if (someHighlighted) {
      this.$TST.addState(Constants.kTAB_STATE_SOME_DESCENDANTS_HIGHLIGHTED);
      if (allHighlighted)
        this.$TST.addState(Constants.kTAB_STATE_ALL_DESCENDANTS_HIGHLIGHTED);
      else
        this.$TST.removeState(Constants.kTAB_STATE_ALL_DESCENDANTS_HIGHLIGHTED);
    }
    else {
      this.$TST.removeState(Constants.kTAB_STATE_SOME_DESCENDANTS_HIGHLIGHTED);
      this.$TST.removeState(Constants.kTAB_STATE_ALL_DESCENDANTS_HIGHLIGHTED);
    }
  }

  _updateCollapseExpandState() {
    if (!this.$TST) // called before binding on restoration from cache
      return;

    const classList = this.classList;
    const parent = this.$TST.parent;
    if (this.$TST.collapsed ||
        (parent &&
         (parent.$TST.collapsed ||
          parent.$TST.subtreeCollapsed))) {
      if (!classList.contains(Constants.kTAB_STATE_COLLAPSED))
        classList.add(Constants.kTAB_STATE_COLLAPSED);
      if (!classList.contains(Constants.kTAB_STATE_COLLAPSED_DONE))
        classList.add(Constants.kTAB_STATE_COLLAPSED_DONE);
    }
    else {
      if (classList.contains(Constants.kTAB_STATE_COLLAPSED))
        classList.remove(Constants.kTAB_STATE_COLLAPSED);
      if (classList.contains(Constants.kTAB_STATE_COLLAPSED_DONE))
        classList.remove(Constants.kTAB_STATE_COLLAPSED_DONE);
    }
  }

  _updateTabProperties() {
    if (!this.$TST) // called before binding on restoration from cache
      return;

    const tab       = this.$TST.tab;
    const classList = this.classList;

    this.label = tab.title;

    const openerOfGroupTab = this.$TST.isGroupTab && Tab.getOpenerFromGroupTab(tab);
    this.favIconUrl = openerOfGroupTab && openerOfGroupTab.favIconUrl || tab.favIconUrl;

    for (const state of classList) {
      if (IGNORE_CLASSES.has(state) ||
          NATIVE_PROPERTIES.has(state))
        continue;
      if (!this.$TST.states.has(state))
        classList.remove(state);
    }
    for (const state of this.$TST.states) {
      if (IGNORE_CLASSES.has(state))
        continue;
      if (!classList.contains(state))
        classList.add(state);
    }

    for (const state of NATIVE_PROPERTIES) {
      if (tab[state] == classList.contains(state))
        continue;
      classList.toggle(state, tab[state]);
    }

    if (this.$TST.childIds.length > 0)
      this.setAttribute(Constants.kCHILDREN, `|${this.$TST.childIds.join('|')}|`);
    else
      this.removeAttribute(Constants.kCHILDREN);

    if (this.$TST.parentId)
      this.setAttribute(Constants.kPARENT, this.$TST.parentId);
    else
      this.removeAttribute(Constants.kPARENT);

    const alreadyGrouped = this.$TST.getAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER) || '';
    if (this.getAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER) != alreadyGrouped)
      this.setAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER, alreadyGrouped);

    const opener = this.$TST.getAttribute(Constants.kPERSISTENT_ORIGINAL_OPENER_TAB_ID) || '';
    if (this.getAttribute(Constants.kPERSISTENT_ORIGINAL_OPENER_TAB_ID) != opener)
      this.setAttribute(Constants.kPERSISTENT_ORIGINAL_OPENER_TAB_ID, opener);

    const uri = this.$TST.getAttribute(Constants.kCURRENT_URI) || tab.url;
    if (this.getAttribute(Constants.kCURRENT_URI) != uri)
      this.setAttribute(Constants.kCURRENT_URI, uri);

    const favIconUri = this.$TST.getAttribute(Constants.kCURRENT_FAVICON_URI) || tab.favIconUrl;
    if (this.getAttribute(Constants.kCURRENT_FAVICON_URI) != favIconUri)
      this.setAttribute(Constants.kCURRENT_FAVICON_URI, favIconUri);

    const level = this.$TST.getAttribute(Constants.kLEVEL) || 0;
    if (this.getAttribute(Constants.kLEVEL) != level)
      this.setAttribute(Constants.kLEVEL, level);

    const id = this.$TST.uniqueId.id;
    if (this.getAttribute(Constants.kPERSISTENT_ID) != id)
      this.setAttribute(Constants.kPERSISTENT_ID, id);

    if (this.$TST.subtreeCollapsed) {
      if (!classList.contains(Constants.kTAB_STATE_SUBTREE_COLLAPSED))
        classList.add(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
    }
    else {
      if (classList.contains(Constants.kTAB_STATE_SUBTREE_COLLAPSED))
        classList.remove(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
    }
  }

  get favIconUrl() {
    if (!this.initialized)
      return null;

    return this._favIconElement.src;
  }

  set favIconUrl(url) {
    this._favIconUrl = url;
    if (!this.initialized || !this.$TST)
      return url;

    TabFavIconHelper.loadToImage({
      image: this._favIconElement,
      tab: this.$TST.tab,
      url
    });
  }

  get overflow() {
    const label = this._labelElement;
    return label && label.overflow;
  }

  get label() {
    const label = this._labelElement;
    return label ? label.value : null;
  }
  set label(value) {
    const label = this._labelElement;
    if (label)
      label.value = value;

    this.dataset.title = value; // for custom CSS https://github.com/piroor/treestyletab/issues/2242

    if (!this.$TST) // called before binding on restoration from cache
      return;

    this.invalidateTooltip();
    if (this.$TST.collapsed)
      this._needToUpdateOverflow = true;
  }
}
