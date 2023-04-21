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

    this._searchProviderInfo = this._searchProviderInfo
      // Filter-out non-ad providers so that we're not trying to match against
      // those unnecessarily.
      .filter(p => "extraAdServersRegexps" in p)
      // Pre-build the regular expressions.
      .map(p => {
        if (p.components) {
          p.components.forEach(component => {
            if (component.included?.regexps) {
              component.included.regexps = component.included.regexps.map(
                r => new RegExp(r)
              );
            }
            if (component.excluded?.regexps) {
              component.excluded.regexps = component.excluded.regexps.map(
                r => new RegExp(r)
              );
            }
            return component;
          });
        }
        p.adServerAttributes = p.adServerAttributes ?? [];
        if (p.shoppingTab?.regexp) {
          p.shoppingTab.regexp = new RegExp(p.shoppingTab.regexp);
        }
        return {
          ...p,
          searchPageRegexp: new RegExp(p.searchPageRegexp),
          extraAdServersRegexps: p.extraAdServersRegexps.map(
            r => new RegExp(r)
          ),
          nonAdsLinkRegexps: p.nonAdsLinkRegexps?.length
            ? p.nonAdsLinkRegexps.map(r => new RegExp(r))
            : [],
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
   * An array containing RegExps that wouldn't be caught in
   * lists of ad expressions.
   */
  #nonAdRegexps = [];

  /**
   * An array of components to do a top-down search.
   */
  #topDownComponents = [];

  /**
   * Top level URL being viewed.
   *
   * @type {URL | null}
   */
  #pageUrl = null;

  set pageUrl(url) {
    this.#pageUrl = url;
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

    // Reset values.
    this.#nonAdRegexps = [];
    this.#topDownComponents = [];

    for (let component of this.#providerInfo.components) {
      // Shopping is parsed before any component, so its regular expression
      // and flags should not be added to avoid double-checking.
      if (component.included?.default) {
        this.#defaultComponent = component;
      }
      if (component.nonAd && component.included?.regexps) {
        this.#nonAdRegexps = this.#nonAdRegexps.concat(
          component.included.regexps
        );
      }
      if (component.topDown) {
        this.#topDownComponents.push(component);
      }
    }
  }

  /**
   * The callback that should fire when an element is interacted with.
   *
   * @type {function}
   */
  #eventCallback = null;

  set eventCallback(callback) {
    this.#eventCallback = callback;
  }

  /**
   * How far from the top the page has been scrolled.
   */
  #scrollFromTop = 0;

  set scrollFromTop(distance) {
    this.#scrollFromTop = distance;
  }

  /**
   * Check if the page has a shopping tab.
   *
   * @param {Document} document
   * @return {boolean}
   *   Whether the page has a shopping tab. Defaults to false.
   */
  hasShoppingTab(document) {
    if (!this.#providerInfo?.shoppingTab) {
      return false;
    }

    let selector = this.#providerInfo.shoppingTab.selector;
    let regexp = this.#providerInfo.shoppingTab.regexp;

    let elements = document.querySelectorAll(selector);
    for (let element of elements) {
      let href = element.getAttribute("href");
      if (href && regexp.test(href)) {
        this.#recordElementData(element, {
          type: "shopping_tab",
          count: 1,
        });
        return true;
      }
    }
    return false;
  }

  /**
   * Examine the list of anchors and the document object and find components
   * on the page.
   *
   * With the list of anchors, go through each and find the component it
   * belongs to and save it in elementToAdDataMap.
   *
   * Then, with the document object find components and save the results to
   * elementToAdDataMap.
   *
   * Lastly, combine the results together in a new Map that contains the number
   * of loaded, visible, and blocked results for the component.
   *
   * @param {HTMLCollectionOf<HTMLAnchorElement>} anchors
   * @param {Document} document
   *
   * @returns {Map<string, object>}
   *  A map where the key is a string containing the type of ad component
   *  and the value is an object containing the number of adsLoaded,
   *  adsVisible, and adsHidden within the component.
   */
  categorize(anchors, document) {
    // Bottom up approach.
    this.#categorizeAnchors(anchors);

    // Top down approach.
    this.#categorizeDocument(document);

    let componentToVisibilityMap = new Map();
    let hrefToComponentMap = new Map();
    // Iterate over the results:
    // - If it's searchbox add event listeners.
    // - If it is a non_ads_link, map its href to component type.
    // - For others, map its component type and check visibility.
    for (let [element, data] of this.#elementToAdDataMap.entries()) {
      if (data.type == "incontent_searchbox") {
        // If searchbox has child elements, observe those, otherwise
        // fallback to its parent element.
        this.#addEventListenerToElements(
          data.childElements.length ? data.childElements : [element],
          data.type,
          false
        );
        continue;
      }
      if (data.childElements.length) {
        for (let child of data.childElements) {
          let href = this.#extractHref(child);
          if (href) {
            hrefToComponentMap.set(href, data.type);
          }
        }
      } else {
        let href = this.#extractHref(element);
        if (href) {
          hrefToComponentMap.set(href, data.type);
        }
      }

      // If the component is a non_ads_link, skip visibility checks.
      if (data.type == "non_ads_link") {
        continue;
      }

      // If proxy children were found, check the visibility of all of them
      // otherwise just check the visiblity of the first child.
      let childElements;
      if (data.proxyChildElements.length) {
        childElements = data.proxyChildElements;
      } else if (data.childElements.length) {
        childElements = [data.childElements[0]];
      }

      let count = this.#countVisibleAndHiddenAds(
        element,
        data.adsLoaded,
        childElements
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

    return { componentToVisibilityMap, hrefToComponentMap };
  }

  /**
   * Given an element, find the href that is most likely to make the request if
   * the element is clicked. The initial value of the anchor is an href if the
   * attribute exists, otherwise it is a blank string. Then, if the element
   * contains a specific data attribute known to contain hrefs, it will be
   * used instead.
   *
   * @param {Element} element
   *  The element to inspect.
   * @returns {string}
   *   The href of the element.
   */
  #extractHref(element) {
    let href = element.getAttribute("href") ?? "";
    for (let name of this.#providerInfo.adServerAttributes) {
      if (
        element.dataset[name] &&
        this.#providerInfo.extraAdServersRegexps.some(regexp =>
          regexp.test(element.dataset[name])
        )
      ) {
        href = element.dataset[name];
        break;
      }
    }
    // Some hrefs might be using relative URLs.
    if (href?.startsWith("/")) {
      href = this.#pageUrl.origin + href;
    }
    return href;
  }

  /**
   * Given a list of anchor elements, group them into ad components.
   *
   * The first step in the process is to check if the anchor should be
   * inspected. This is based on whether it contains an href or a
   * data-attribute values that matches an ad link, or if it contains a
   * pattern caught by a components included regular expression.
   *
   * Determine which component it belongs to and the number of matches for
   * the component. The heuristic is described in findDataForAnchor.
   * If there was a result and we haven't seen it before, save it in
   * elementToAdDataMap.
   *
   * @param {HTMLCollectionOf<HTMLAnchorElement>} anchors
   *  The list of anchors to inspect.
   */
  #categorizeAnchors(anchors) {
    for (let anchor of anchors) {
      if (this.#shouldInspectAnchor(anchor)) {
        let result = this.#findDataForAnchor(anchor);
        if (result) {
          this.#recordElementData(result.element, {
            type: result.type,
            count: result.count,
            proxyChildElements: result.proxyChildElements,
            childElements: result.childElements,
          });
        }
        if (result.relatedElements?.length) {
          this.#addEventListenerToElements(result.relatedElements, result.type);
        }
        // If an anchor doesn't match any component, and it doesn't have a non
        // ads link regexp, cache the anchor so the parent process can observe
        // them.
      } else if (!this.#providerInfo.nonAdsLinkRegexps.length) {
        this.#recordElementData(anchor, {
          type: "non_ads_link",
        });
      }
    }
  }

  /**
   * Find components from the document object. This is mostly relevant for
   * components that are non-ads and don't have an obvious regular expression
   * that could match the pattern of the href.
   *
   * @param {Document} document
   */
  #categorizeDocument(document) {
    // using the subset of components that are top down,
    // go through each one.
    for (let component of this.#topDownComponents) {
      // Top-down searches must have the topDown attribute.
      if (!component.topDown) {
        continue;
      }
      // Top down searches must include a parent.
      if (!component.included?.parent) {
        continue;
      }
      let parents = document.querySelectorAll(
        component.included.parent.selector
      );
      if (parents.length) {
        for (let parent of parents) {
          if (component.included.related?.selector) {
            this.#addEventListenerToElements(
              parent.querySelectorAll(component.included.related.selector),
              component.type
            );
          }
          if (component.included.children) {
            for (let child of component.included.children) {
              let childElements = parent.querySelectorAll(child.selector);
              if (childElements) {
                this.#recordElementData(parent, {
                  type: component.type,
                  childElements: Array.from(childElements),
                });
                break;
              }
            }
          } else {
            this.#recordElementData(parent, {
              type: component.type,
            });
          }
        }
      }
    }
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
    let regexps = this.#providerInfo.extraAdServersRegexps;
    // Anchors can contain ad links in a data-attribute.
    for (let name of this.#providerInfo.adServerAttributes) {
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
    // Anchors can contain hrefs matching non-ad regular expressions.
    if (this.#nonAdRegexps.some(regexp => regexp.test(anchor.href))) {
      return true;
    }
    return false;
  }

  /**
   * Find the component data for an anchor.
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
  #findDataForAnchor(anchor) {
    for (let component of this.#providerInfo.components) {
      // First, check various conditions for skipping a component.

      // A component should always have at least one included statement.
      if (!component.included) {
        continue;
      }

      // Top down searches are done after the bottom up search.
      if (component.topDown) {
        continue;
      }

      // The default component doesn't need to be checked,
      // as it will be the fallback option.
      if (component.included.default) {
        continue;
      }

      // The anchor shouldn't belong to an excluded parent component.
      if (anchor.closest(component.excluded?.parent?.selector)) {
        continue;
      }

      // The anchor should not belong to an excluded regexp (if provided).
      if (component.excluded?.regexps?.some(r => r.test(anchor.href))) {
        continue;
      }

      // The anchor should belong to an included regexp (if provided).
      if (
        component.included.regexps &&
        !component.included.regexps.some(r => r.test(anchor.href))
      ) {
        continue;
      }

      // If no parent was provided, but it passed a previous regular
      // expression check, return the anchor. This might be because there
      // was no clear parent for the anchor to match against.
      if (!component.included.parent && component.included?.regexps) {
        return {
          element: anchor,
          type: component.type,
        };
      }

      // Find the parent of the anchor.
      let parent = anchor.closest(component.included.parent.selector);

      if (!parent) {
        continue;
      }

      // If we've already inspected the parent, add the child element to the
      // list of anchors. Don't increment the ads loaded count, as we only care
      // about grouping the anchor with the correct parent.
      if (this.#elementToAdDataMap.has(parent)) {
        return {
          element: parent,
          childElements: [anchor],
        };
      }

      let relatedElements = [];
      if (component.included.related?.selector) {
        relatedElements = parent.querySelectorAll(
          component.included.related.selector
        );
      }

      // If the component has no defined children, return the parent element.
      if (component.included.children) {
        // Look for the first instance of a matching child selector.
        for (let child of component.included.children) {
          // If counting by child, get all of them at once.
          if (child.countChildren) {
            let proxyChildElements = parent.querySelectorAll(child.selector);
            if (proxyChildElements.length) {
              return {
                element: parent,
                type: child.type ?? component.type,
                proxyChildElements: Array.from(proxyChildElements),
                count: proxyChildElements.length,
                childElements: [anchor],
                relatedElements,
              };
            }
          } else if (parent.querySelector(child.selector)) {
            return {
              element: parent,
              type: child.type ?? component.type,
              childElements: [anchor],
              relatedElements,
            };
          }
        }
      }
      // If no children were defined for this component, or none were found
      // in the DOM, use the default definition.
      return {
        element: parent,
        type: component.type,
        childElements: [anchor],
        relatedElements,
      };
    }
    // If no component was found, use default values.
    return {
      element: anchor,
      type: this.#defaultComponent.type,
    };
  }

  /**
   * Determines whether or not an ad was visible or hidden.
   *
   * An ad is considered visible if the parent element containing the
   * component has non-zero dimensions, and all child element in the
   * component have non-zero dimensions and fits within the window
   * at the time when the impression was takent.
   *
   * For some components, like text ads, we don't send every child
   * element for visibility, just the first text ad. For other components
   * like carousels, we send all child elements because we do care about
   * counting how many elements of the carousel were visible.
   *
   * @param {Element} element
   *  Element to be inspected
   * @param {number} adsLoaded
   *  Number of ads initially determined to be loaded for this element.
   * @param {Array<Element>} childElements
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

    // Since the parent element has dimensions but no child elements we want
    // to inspect, check the parent itself is within the viewable area.
    if (!childElements || !childElements.length) {
      if (this.#innerWindowHeight < elementRect.y + elementRect.height) {
        return {
          adsVisible: 0,
          adsHidden: 0,
        };
      }
      return {
        adsVisible: 1,
        adsHidden: 0,
      };
    }

    let adsVisible = 0;
    let adsHidden = 0;
    for (let child of childElements) {
      let itemRect = child.getBoundingClientRect();

      // If the child element we're inspecting has no dimension, it is hidden.
      if (itemRect.height == 0 || itemRect.width == 0) {
        adsHidden += 1;
        continue;
      }

      // If the child element is to the left of the containing element, or to
      // the right of the containing element, skip it.
      if (
        itemRect.x < elementRect.x ||
        itemRect.x + itemRect.width > elementRect.x + elementRect.width
      ) {
        continue;
      }

      // If the child element is too far down, skip it.
      if (this.#innerWindowHeight < itemRect.y + itemRect.height) {
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
   * categorized as a component. Subsequent calls to this passing the same
   * element will update the list of child elements.
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
   * @param {Array<Element>} params.proxyChildElements
   *  An array of DOM elements that should be inspected for visibility instead
   *  of the actual child elements, possibly because they are grouped.
   * @param {Array<Element>} params.childElements
   *  An array of DOM elements to inspect.
   */
  #recordElementData(
    element,
    { type, count = 1, proxyChildElements = [], childElements = [] } = {}
  ) {
    if (this.#elementToAdDataMap.has(element)) {
      let recordedValues = this.#elementToAdDataMap.get(element);
      if (childElements.length) {
        recordedValues.childElements = recordedValues.childElements.concat(
          childElements
        );
      }
    } else {
      this.#elementToAdDataMap.set(element, {
        type,
        adsLoaded: count,
        proxyChildElements,
        childElements,
      });
    }
  }

  /**
   * Adds a click listener to a specific element.
   *
   * @param {Array<Element>} elements
   *  DOM elements to add event listeners to.
   * @param {string} type
   *  The component type of the element.
   * @param {boolean} isRelated
   *  Whether the elements input are related to components or are actual
   *  components.
   */
  #addEventListenerToElements(elements, type, isRelated = true) {
    if (!elements?.length) {
      return;
    }
    let clickAction = "clicked";
    let keydownEnterAction = "clicked";

    switch (type) {
      case "incontent_searchbox":
        keydownEnterAction = "submitted";
        if (isRelated) {
          // The related element to incontent_search are autosuggested elements
          // which when clicked should cause different action than if the
          // searchbox is clicked.
          clickAction = "submitted";
        }
        break;
      case "ad_carousel":
      case "refined_search_buttons":
        if (isRelated) {
          clickAction = "expanded";
        }
        break;
    }
    for (let element of elements) {
      let clickCallback = () => {
        this.#eventCallback(type, clickAction);
      };
      element.addEventListener("click", clickCallback);

      let keydownCallback = event => {
        if (event.key == "Enter") {
          this.#eventCallback(type, keydownEnterAction);
        }
      };
      element.addEventListener("keydown", keydownCallback);

      searchAdImpressionListeners.set(element, {
        clicked: clickCallback,
        keydown: keydownCallback,
      });
      searchAdImpressionElements.add(element);
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
const searchAdImpressionListeners = new WeakMap();
const searchAdImpressionElements = new WeakSet();

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
    let anchors = doc.getElementsByTagName("a");
    let hasAds = false;
    for (let anchor of anchors) {
      if (!anchor.href) {
        continue;
      }
      for (let name of providerInfo.adServerAttributes) {
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
        searchAdImpression.pageUrl = new URL(url);
        searchAdImpression.providerInfo = providerInfo;
        searchAdImpression.scrollFromTop = this.contentWindow.scrollY;
        searchAdImpression.innerWindowHeight = this.contentWindow.innerHeight;
        searchAdImpression.eventCallback = (type, action) => {
          this.sendAsyncMessage("SearchTelemetry:Action", {
            type,
            url: this.document.documentURI,
            action,
          });
        };
        let start = Cu.now();
        let {
          componentToVisibilityMap,
          hrefToComponentMap,
        } = searchAdImpression.categorize(anchors, doc);
        ChromeUtils.addProfilerMarker(
          "SearchSERPTelemetryChild._checkForAdLink",
          start,
          "Checked anchors for visibility"
        );
        this.sendAsyncMessage("SearchTelemetry:AdImpressions", {
          adImpressions: componentToVisibilityMap,
          hrefToComponentMap,
          url,
        });
      }
    }
  }

  /**
   * Checks for the presence of certain components on the page that are
   * required for recording the page impression.
   */
  #checkForPageImpressionComponents() {
    let url = this.document.documentURI;
    let providerInfo = this._getProviderInfoForUrl(url);
    searchAdImpression.providerInfo = providerInfo;

    let start = Cu.now();
    let hasShoppingTab = searchAdImpression.hasShoppingTab(this.document);
    ChromeUtils.addProfilerMarker(
      "SearchSERPTelemetryChild.#recordImpression",
      start,
      "Checked for shopping tab"
    );
    this.sendAsyncMessage("SearchTelemetry:PageImpression", {
      url,
      hasShoppingTab,
    });
  }

  /**
   * Handles events received from the actor child notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    if (!this._getProviderInfoForUrl(this.document.documentURI)) {
      return;
    }
    switch (event.type) {
      case "pageshow": {
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case. Note: we do this
        // so that we remain consistent with the *.in-content:sap* count for the
        // SEARCH_COUNTS histogram.
        if (event.persisted) {
          this.#check(event.type);
          if (lazy.serpEventsEnabled) {
            this.#checkForPageImpressionComponents();
          }
        }
        break;
      }
      case "DOMContentLoaded": {
        if (lazy.serpEventsEnabled) {
          this.#checkForPageImpressionComponents();
        }
        this.#check(event.type);
        break;
      }
      case "load": {
        // We check both DOMContentLoaded and load in case the page has
        // taken a long time to load and the ad is only detected on load.
        // We still check at DOMContentLoaded because if the page hasn't
        // finished loading and the user navigates away, we still want to know
        // if there were ads on the page or not at that time.
        this.#check(event.type);
        break;
      }
      case "pagehide": {
        this.#cancelCheck();
        if (lazy.serpEventsEnabled) {
          this.#clearListeners();
        }
        break;
      }
    }
  }

  #clearListeners() {
    let start = Cu.now();
    for (let element of ChromeUtils.nondeterministicGetWeakSetKeys(
      searchAdImpressionElements
    )) {
      let listeners = searchAdImpressionListeners.get(element);
      if (listeners) {
        element.removeEventListener("clicked", listeners.clicked);
        element.removeEventListener("keydown", listeners.keydown);
      }
    }
    ChromeUtils.addProfilerMarker(
      "SearchSERPTelemetryChild.#clearListeners",
      start,
      "Removed event listeners."
    );
  }

  #cancelCheck() {
    if (this._waitForContentTimeout) {
      lazy.clearTimeout(this._waitForContentTimeout);
    }
  }

  #check(eventType) {
    this.#cancelCheck();
    this._waitForContentTimeout = lazy.setTimeout(() => {
      this._checkForAdLink(eventType);
    }, ADLINK_CHECK_TIMEOUT_MS);
  }
}
