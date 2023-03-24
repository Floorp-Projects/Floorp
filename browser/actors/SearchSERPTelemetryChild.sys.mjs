/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsEnabled",
  "browser.search.serpEventTelemetry.enabled",
  false
);

const SHARED_DATA_KEY = "SearchTelemetry:ProviderInfo";
export const ADLINK_CHECK_TIMEOUT_MS = 1000;

/**
 * SearchProviders looks after keeping track of the search provider information
 * received from the main process.
 *
 * It is separate to SearchTelemetryChild so that it is not constructed for each
 * tab, but once per process.
 */
class SearchProviders {
  constructor() {
    this._searchProviderInfo = null;
    Services.cpmm.sharedData.addEventListener("change", this);
  }

  /**
   * Gets the search provider information for any provider with advert information.
   * If there is nothing in the cache, it will obtain it from shared data.
   *
   * @returns {object} Returns the search provider information. @see SearchTelemetry.jsm
   */
  get info() {
    if (this._searchProviderInfo) {
      return this._searchProviderInfo;
    }

    this._searchProviderInfo = Services.cpmm.sharedData.get(SHARED_DATA_KEY);

    if (!this._searchProviderInfo) {
      return null;
    }

    // Filter-out non-ad providers so that we're not trying to match against
    // those unnecessarily.
    this._searchProviderInfo = this._searchProviderInfo
      .filter(p => "extraAdServersRegexps" in p)
      .map(p => {
        return {
          ...p,
          searchPageRegexp: new RegExp(p.searchPageRegexp),
          extraAdServersRegexps: p.extraAdServersRegexps.map(
            r => new RegExp(r)
          ),
        };
      });

    return this._searchProviderInfo;
  }

  /**
   * Handles events received from sharedData notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    switch (event.type) {
      case "change": {
        if (event.changedKeys.includes(SHARED_DATA_KEY)) {
          // Just null out the provider information for now, we'll fetch it next
          // time we need it.
          this._searchProviderInfo = null;
        }
        break;
      }
    }
  }
}

/**
 * Scans SERPs for ad components.
 */
class SearchAdImpression {
  /**
   * A reference to ad component information that is used if an anchor
   * element could not be categorized to a specific ad component.
   *
   * @type {object}
   */
  #defaultComponent = null;

  /**
   * Maps DOM elements to AdData.
   *
   * @type {Map<Element, AdData>}
   *
   * @typedef AdData
   * @type {object}
   * @property {string} type
   *  The type of ad component.
   * @property {number} adsLoaded
   *  The number of ads counted as loaded for the component.
   * @property {boolean} countChildren
   *  Whether all the children were counted for the component.
   */
  #elementToAdDataMap = new Map();

  /**
   * Height of the inner window in the browser.
   */
  #innerWindowHeight = 0;

  set innerWindowHeight(height) {
    this.#innerWindowHeight = height;
  }

  /**
   * A reference the providerInfo for this SERP.
   *
   * @type {object}
   */
  #providerInfo = null;

  set providerInfo(providerInfo) {
    if (this.#providerInfo?.telemetryId == providerInfo.telemetryId) {
      return;
    }

    this.#providerInfo = providerInfo;
    for (let component of this.#providerInfo.components) {
      if (component.included?.default) {
        this.#defaultComponent = component;
        break;
      }
    }
  }

  /**
   * How far from the top the page has been scrolled.
   */
  #scrollFromTop = 0;

  set scrollFromTop(distance) {
    this.#scrollFromTop = distance;
  }

  /**
   * Given a list of anchor elements, group them into ad components
   * and count the number of loaded, visible, and hidden ads.
   *
   * The first step in the process is to check if the anchor should be
   * inspected. This is based on whether it contains an href or a
   * data-attribute values that matches an ad link.
   *
   * If it looks like an ad link, determine which ad component it belongs to
   * and the number of ads for the component. The heuristic is described in
   * findAdDataForAnchor. If there was a result and we haven't seen it before,
   * save in elementToAdDataMap.
   *
   * Once all the links have been parsed, go through each component
   * in elementToAdDataMap and check if the ad was visible to the user.
   *
   * @param {HTMLCollectionOf<HTMLAnchorElement>} anchors
   *  Anchors to inspect.
   * @returns {Map<string, object>}
   *  A map where the key is a string containing the type of ad component
   *  and the value is an object containing the number of adsLoaded,
   *  adsVisible, and adsHidden within the component.
   */
  resultFromAnchors(anchors) {
    for (let anchor of anchors) {
      if (this.#shouldInspectAnchor(anchor)) {
        let result = this.#findAdDataForAnchor(anchor);
        if (result) {
          this.#recordElementData(result.element, {
            type: result.type,
            count: result.count,
            countChildren: result.countChildren,
            childElements: result.childElements,
          });
        }
      }
    }

    let componentToVisibilityMap = new Map();
    // Count the number of visible and hidden ads within each cached
    // element and save the results according to the component they
    // belonged to.
    for (let [element, data] of this.#elementToAdDataMap.entries()) {
      let count = this.#countVisibleAndHiddenAds(
        element,
        data.adsLoaded,
        data.childElements
      );
      if (componentToVisibilityMap.has(data.type)) {
        let componentInfo = componentToVisibilityMap.get(data.type);
        componentInfo.adsLoaded += data.adsLoaded;
        componentInfo.adsVisible += count.adsVisible;
        componentInfo.adsHidden += count.adsHidden;
      } else {
        componentToVisibilityMap.set(data.type, {
          adsLoaded: data.adsLoaded,
          adsVisible: count.adsVisible,
          adsHidden: count.adsHidden,
        });
      }
    }

    // Release the DOM elements from the Map.
    this.#elementToAdDataMap.clear();

    return componentToVisibilityMap;
  }

  /**
   * Evaluates whether an anchor should be inspected based on matching
   * regular expressions on either its href or specified data-attribute values.
   *
   * @param {HTMLAnchorElement} anchor
   * @returns {boolean}
   */
  #shouldInspectAnchor(anchor) {
    if (!anchor.href) {
      return false;
    }
    let adServerAttributes = this.#providerInfo.adServerAttributes ?? [];
    let regexps = this.#providerInfo.extraAdServersRegexps;
    // Anchors can contain ad links in a data-attribute.
    for (let name of adServerAttributes) {
      if (
        anchor.dataset[name] &&
        regexps.some(regexp => regexp.test(anchor.dataset[name]))
      ) {
        return true;
      }
    }
    // Anchors can contain ad links in a specific href.
    if (regexps.some(regexp => regexp.test(anchor.href))) {
      return true;
    }
    return false;
  }

  /**
   * Find the ad data for an anchor.
   *
   * To categorize the anchor, we iterate over the list of possible components
   * the anchor could be categorized. If the component is default, we skip
   * checking because the fallback option for all anchor links is the default.
   *
   * First, get the "parent" of the anchor which best represents the DOM element
   * that contains the anchor links for the component and no other component.
   * This parent will be cached so that other anchors that share the same
   * parent can be counted together.
   *
   * The check for a parent is a loop because we can define more than one best
   * parent since on certain SERPs, it's possible for a "better" DOM element
   * parent to appear occassionally.
   *
   * If no parent is found, skip this component.
   *
   * If a parent was found, check for specific child elements.
   *
   * Finding child DOM elements of a parent is optional. One reason to do so is
   * to use child elements instead of anchor links to count the number of ads for
   * a component via the `countChildren` property. This is provided because some ads
   * (i.e. carousels) have multiple ad links in a single child element that go to the
   * same location. In this scenario, all instances of the child are recorded as ads.
   * Subsequent anchor elements that map to the same parent are ignored.
   *
   * Whether or not a child was found, return the information that was found,
   * including whether or not all child elements were counted instead of anchors.
   *
   * If another anchor belonging to a parent that was previously recorded is the input
   * for this function, we either increment the ad count by 1 or don't increment the ad
   * count because the parent used `countChildren` completed the calculation in a
   * previous step.
   *
   *
   * @param {HTMLAnchorElement} anchor
   *  The anchor to be inspected.
   * @returns {object}
   *  An object containing the element representing the root DOM element for
   *  the component, the type of component, how many ads were counted,
   *  and whether or not the count was of all the children.
   */
  #findAdDataForAnchor(anchor) {
    for (let component of this.#providerInfo.components) {
      // First, check various conditions for skipping a component.

      // A component should always have at least one included statement,
      // and a included statement with a parent.
      if (!component.included || !component.included.parent) {
        continue;
      }

      // The default component doesn't need to be checked,
      // as it will be the fallback option.
      if (component.included.default) {
        continue;
      }

      // Find the parent of the anchor.
      let parent = anchor.closest(component.included.parent.selector);

      // If no parent was found, this wasn't the right component.
      if (!parent) {
        continue;
      }

      // If we've already inspected the parent, either:
      // Increment the number of ads seen for this element,
      // or if its child elements have already been counted, return null.
      if (this.#elementToAdDataMap.has(parent)) {
        return !this.#childElementsCounted(parent)
          ? { element: parent, count: 1, childElements: [anchor] }
          : null;
      }

      // If the component has no defined children, return the parent element.
      if (component.included.children) {
        // Look for the first instance of a matching child selector.
        for (let child of component.included.children) {
          // If counting by child, get all of them at once.
          if (child.countChildren) {
            let childElements = parent.querySelectorAll(child.selector);
            if (childElements.length) {
              return {
                element: parent,
                type: component.type,
                count: childElements.length,
                countChildren: child.countChildren,
                childElements,
              };
            }
          } else if (parent.querySelector(child.selector)) {
            return {
              element: parent,
              type: component.type,
              count: 1,
              childElements: [anchor],
            };
          }
        }
      }
      // If no children were defined for this component, or none were found
      // in the DOM, use the default definition.
      return {
        element: parent,
        type: component.type,
        count: 1,
        childElements: [anchor],
      };
    }
    // If no component was found, use default values.
    return {
      element: anchor,
      type: this.#defaultComponent.type,
      count: 1,
      childElements: [anchor],
    };
  }

  /**
   * Determines whether or not an ad was visible or hidden.
   *
   * An ad is considered visible if it has non-zero dimensions, and is in
   * the possible viewing area of the users window at the time the ad
   * impression is recorded.
   *
   * @param {Element} element
   *  Element to be inspected
   * @param {number} adsLoaded
   *  Number of ads initially determined to be loaded for this element.
   * @param {NodeListOf<Element>} childElements
   *  List of children belonging to element.
   * @returns {object}
   *  Contains adsVisible which is the number of ads shown for the element
   *  and adsHidden, the number of ads not visible to the user.
   */
  #countVisibleAndHiddenAds(element, adsLoaded, childElements) {
    let elementRect = element.getBoundingClientRect();

    // If the element lacks a dimension, assume all ads that
    // were contained within it are hidden.
    if (elementRect.width == 0 || elementRect.height == 0) {
      return {
        adsVisible: 0,
        adsHidden: adsLoaded,
      };
    }

    let adsVisible = 0;
    let adsHidden = 0;
    for (let child of childElements) {
      let itemRect = child.getBoundingClientRect();

      // If the element we're inspecting has no dimension, it is hidden.
      if (itemRect.height == 0 || itemRect.width == 0) {
        adsHidden += 1;
        continue;
      }

      // If the element is to the left of the containing element, or to the
      // right of the containing element, skip it.
      if (
        itemRect.x < elementRect.x ||
        itemRect.x + itemRect.width > elementRect.x + elementRect.width
      ) {
        continue;
      }

      // If the element is too far down, skip it.
      if (this.#scrollFromTop + this.#innerWindowHeight < elementRect.y) {
        continue;
      }
      ++adsVisible;
    }

    return {
      adsVisible,
      adsHidden,
    };
  }

  /**
   * Caches ad data for a DOM element. The key of the map is by Element rather
   * than Component for fast lookup on whether an Element has been already been
   * categorized as a component.
   *
   * @param {Element} element
   *  The element considered to be the root for the component.
   * @param {object} params
   *  Various parameters that can be recorded. Whether the input values exist
   *  or not depends on which component was found, which heuristic should be used
   *  to determine whether an ad was visible, and whether we've already seen this
   *  element.
   * @param {string | null} params.type
   *  The type of component.
   * @param {number} params.count
   *  The number of ads found for a component. The number represents either
   *  the number of elements that match an ad expression or the number of DOM
   *  elements containing an ad link.
   * @param {boolean | null} params.countChildren
   *  Whether all the children were counted for the element.
   * @param {Array<Element> | null} params.childElements
   *  An array of DOM elements to inspect.
   */
  #recordElementData(
    element,
    { type, count = 0, countChildren = false, childElements = null } = {}
  ) {
    if (this.#elementToAdDataMap.has(element)) {
      let recordedValues = this.#elementToAdDataMap.get(element);
      recordedValues.adsLoaded = recordedValues.adsLoaded + count;
      if (childElements) {
        recordedValues.childElements = recordedValues.childElements.concat(
          childElements
        );
      }
    } else {
      this.#elementToAdDataMap.set(element, {
        type,
        adsLoaded: count,
        countChildren,
        childElements,
      });
    }
  }

  /**
   * Given a DOM element, return whether or not this element was counted
   * by specific child elements rather than the number of anchor links.
   *
   * @param {Element} domElement
   *  The element to lookup.
   * @returns {boolean}
   *  Returns true if child elements were counted, false otherwise.
   */
  #childElementsCounted(domElement) {
    return !!this.#elementToAdDataMap.get(domElement)?.countChildren;
  }
}

const searchProviders = new SearchProviders();
const searchAdImpression = new SearchAdImpression();

/**
 * SearchTelemetryChild monitors for pages that are partner searches, and
 * looks through them to find links which looks like adverts and sends back
 * a notification to SearchTelemetry for possible telemetry reporting.
 *
 * Only the partner details and the fact that at least one ad was found on the
 * page are returned to SearchTelemetry. If no ads are found, no notification is
 * given.
 */
export class SearchSERPTelemetryChild extends JSWindowActorChild {
  /**
   * Determines if there is a provider that matches the supplied URL and returns
   * the information associated with that provider.
   *
   * @param {string} url The url to check
   * @returns {array|null} Returns null if there's no match, otherwise an array
   *   of provider name and the provider information.
   */
  _getProviderInfoForUrl(url) {
    return searchProviders.info?.find(info => info.searchPageRegexp.test(url));
  }

  /**
   * Checks to see if the page is a partner and has an ad link within it. If so,
   * it will notify SearchTelemetry.
   */
  _checkForAdLink(eventType) {
    try {
      if (!this.contentWindow) {
        return;
      }
    } catch (ex) {
      // unload occurred before the timer expired
      return;
    }

    let doc = this.document;
    let url = doc.documentURI;
    let providerInfo = this._getProviderInfoForUrl(url);
    if (!providerInfo) {
      return;
    }

    let regexps = providerInfo.extraAdServersRegexps;
    let adServerAttributes = providerInfo.adServerAttributes ?? [];
    let anchors = doc.getElementsByTagName("a");
    let hasAds = false;
    for (let anchor of anchors) {
      if (!anchor.href) {
        continue;
      }
      for (let name of adServerAttributes) {
        hasAds = regexps.some(regexp => regexp.test(anchor.dataset[name]));
        if (hasAds) {
          break;
        }
      }
      if (!hasAds) {
        hasAds = regexps.some(regexp => regexp.test(anchor.href));
      }
      if (hasAds) {
        break;
      }
    }

    if (hasAds) {
      this.sendAsyncMessage("SearchTelemetry:PageInfo", {
        hasAds,
        url,
      });

      if (
        lazy.serpEventsEnabled &&
        providerInfo?.components &&
        (eventType == "load" || eventType == "pageshow")
      ) {
        searchAdImpression.providerInfo = providerInfo;
        searchAdImpression.scrollFromTop = this.contentWindow.scrollY;
        searchAdImpression.innerWindowHeight = this.contentWindow.innerHeight;
        let adImpressions = searchAdImpression.resultFromAnchors(anchors);
        this.sendAsyncMessage("SearchTelemetry:AdImpressions", {
          adImpressions,
          url,
        });
      }
    }
  }

  /**
   * Handles events received from the actor child notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    const cancelCheck = () => {
      if (this._waitForContentTimeout) {
        lazy.clearTimeout(this._waitForContentTimeout);
      }
    };

    const check = eventType => {
      cancelCheck();
      this._waitForContentTimeout = lazy.setTimeout(() => {
        this._checkForAdLink(eventType);
      }, ADLINK_CHECK_TIMEOUT_MS);
    };

    switch (event.type) {
      case "pageshow": {
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case. Note: we do this
        // so that we remain consistent with the *.in-content:sap* count for the
        // SEARCH_COUNTS histogram.
        if (event.persisted) {
          check(event.type);
        }
        break;
      }
      case "DOMContentLoaded": {
        check(event.type);
        break;
      }
      case "load": {
        // We check both DOMContentLoaded and load in case the page has
        // taken a long time to load and the ad is only detected on load.
        // We still check at DOMContentLoaded because if the page hasn't
        // finished loading and the user navigates away, we still want to know
        // if there were ads on the page or not at that time.
        check(event.type);
        break;
      }
      case "unload": {
        cancelCheck();
        break;
      }
    }
  }
}
