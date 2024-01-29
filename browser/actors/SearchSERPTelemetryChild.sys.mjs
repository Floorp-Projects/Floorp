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
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventTelemetryCategorization",
  "browser.search.serpEventTelemetryCategorization.enabled",
  false
);

// Duplicated from SearchSERPTelemetry to avoid loading the module on content
// startup.
const SEARCH_TELEMETRY_SHARED = {
  PROVIDER_INFO: "SearchTelemetry:ProviderInfo",
  LOAD_TIMEOUT: "SearchTelemetry:LoadTimeout",
};

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

    this._searchProviderInfo = Services.cpmm.sharedData.get(
      SEARCH_TELEMETRY_SHARED.PROVIDER_INFO
    );

    if (!this._searchProviderInfo) {
      return null;
    }

    this._searchProviderInfo = this._searchProviderInfo
      // Filter-out non-ad providers so that we're not trying to match against
      // those unnecessarily.
      .filter(p => "extraAdServersRegexps" in p)
      // Pre-build the regular expressions.
      .map(p => {
        p.adServerAttributes = p.adServerAttributes ?? [];
        if (p.shoppingTab?.inspectRegexpInSERP) {
          p.shoppingTab.regexp = new RegExp(p.shoppingTab.regexp);
        }
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
        if (event.changedKeys.includes(SEARCH_TELEMETRY_SHARED.PROVIDER_INFO)) {
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
   * An array of components to do a top-down search.
   */
  #topDownComponents = [];

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
    this.#topDownComponents = [];

    for (let component of this.#providerInfo.components) {
      if (component.default) {
        this.#defaultComponent = component;
        continue;
      }
      if (component.topDown) {
        this.#topDownComponents.push(component);
      }
    }
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

    // If a provider has the inspectRegexpInSERP, we assume there must be an
    // associated regexp that must be used on any hrefs matched by the elements
    // found using the selector. If inspectRegexpInSERP is false, then check if
    // the number of items found using the selector matches exactly one element
    // to ensure we've used a fine-grained search.
    let elements = document.querySelectorAll(
      this.#providerInfo.shoppingTab.selector
    );
    if (this.#providerInfo.shoppingTab.inspectRegexpInSERP) {
      let regexp = this.#providerInfo.shoppingTab.regexp;
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
    } else if (elements.length == 1) {
      this.#recordElementData(elements[0], {
        type: "shopping_tab",
        count: 1,
      });
      return true;
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
    // Used for various functions to make relative URLs absolute.
    let origin = new URL(document.documentURI).origin;

    // Bottom up approach.
    this.#categorizeAnchors(anchors, origin);

    // Top down approach.
    this.#categorizeDocument(document);

    let componentToVisibilityMap = new Map();
    let hrefToComponentMap = new Map();

    let innerWindowHeight = document.ownerGlobal.innerHeight;
    let scrollY = document.ownerGlobal.scrollY;

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
          let href = this.#extractHref(child, origin);
          if (href) {
            hrefToComponentMap.set(href, data.type);
          }
        }
      } else {
        let href = this.#extractHref(element, origin);
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
        childElements,
        innerWindowHeight,
        scrollY
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
   * the element is clicked. If the element contains a specific data attribute
   * known to contain the url used to make the initial request, use it,
   * otherwise use its href. Specific character conversions are done to mimic
   * conversions likely to take place when urls are observed in network
   * activity.
   *
   * @param {Element} element
   *  The element to inspect.
   * @param {string} origin
   *  The origin for relative urls.
   * @returns {string}
   *   The href of the element.
   */
  #extractHref(element, origin) {
    let href;
    // Prioritize the href from a known data attribute value instead of
    // its href property, as the former is the initial url the page will
    // navigate to before being re-directed to the href.
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
    // If a data attribute value was not found, fallback to the href.
    href = href ?? element.getAttribute("href");
    if (!href) {
      return "";
    }
    // Hrefs can be relative.
    if (!href.startsWith("https://") && !href.startsWith("http://")) {
      href = origin + href;
    }
    // Per Bug 376844, apostrophes in query params are escaped, and thus, are
    // percent-encoded by the time they are observed in the network. Even
    // though it's more comprehensive, we avoid using newURI because its more
    // expensive and conversions should be the exception.
    // e.g. /path'?q=Mozilla's -> /path'?q=Mozilla%27s
    let arr = href.split("?");
    if (arr.length == 2 && arr[1].includes("'")) {
      href = arr[0] + "?" + arr[1].replaceAll("'", "%27");
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
   * @param {string} origin
   *  The origin of the document the anchors belong to.
   */
  #categorizeAnchors(anchors, origin) {
    for (let anchor of anchors) {
      if (this.#shouldInspectAnchor(anchor, origin)) {
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
              if (childElements.length) {
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
   * @param {string} origin
   * @returns {boolean}
   */
  #shouldInspectAnchor(anchor, origin) {
    let href = anchor.getAttribute("href");
    if (!href) {
      return false;
    }

    // Some hrefs might be relative.
    if (!href.startsWith("https://") && !href.startsWith("http://")) {
      href = origin + href;
    }

    let regexps = this.#providerInfo.extraAdServersRegexps;
    // Anchors can contain ad links in a data-attribute.
    for (let name of this.#providerInfo.adServerAttributes) {
      let attributeValue = anchor.dataset[name];
      if (
        attributeValue &&
        regexps.some(regexp => regexp.test(attributeValue))
      ) {
        return true;
      }
    }
    // Anchors can contain ad links in a specific href.
    if (regexps.some(regexp => regexp.test(href))) {
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
      if (component.default) {
        continue;
      }

      // The anchor shouldn't belong to an excluded parent component if one
      // is provided.
      if (
        component.excluded?.parent?.selector &&
        anchor.closest(component.excluded.parent.selector)
      ) {
        continue;
      }

      // All components with included should have a parent entry.
      if (!component.included.parent) {
        continue;
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
   * @param {number} innerWindowHeight
   *  Current height of the window containing the elements.
   * @param {number} scrollY
   *  Current distance the window has been scrolled.
   * @returns {object}
   *  Contains adsVisible which is the number of ads shown for the element
   *  and adsHidden, the number of ads not visible to the user.
   */
  #countVisibleAndHiddenAds(
    element,
    adsLoaded,
    childElements,
    innerWindowHeight,
    scrollY
  ) {
    let elementRect =
      element.ownerGlobal.windowUtils.getBoundsWithoutFlushing(element);

    // If the element lacks a dimension, assume all ads that
    // were contained within it are hidden.
    if (elementRect.width == 0 || elementRect.height == 0) {
      return {
        adsVisible: 0,
        adsHidden: adsLoaded,
      };
    }

    // If an ad is far above the possible visible area of a window, an
    // adblocker might be doing it as a workaround for blocking the ad.
    if (
      elementRect.bottom < 0 &&
      innerWindowHeight + scrollY + elementRect.bottom < 0
    ) {
      return {
        adsVisible: 0,
        adsHidden: adsLoaded,
      };
    }

    // Since the parent element has dimensions but no child elements we want
    // to inspect, check the parent itself is within the viewable area.
    if (!childElements || !childElements.length) {
      if (innerWindowHeight < elementRect.y + elementRect.height) {
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
      let itemRect =
        child.ownerGlobal.windowUtils.getBoundsWithoutFlushing(child);

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
      if (innerWindowHeight < itemRect.y + itemRect.height) {
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
        recordedValues.childElements =
          recordedValues.childElements.concat(childElements);
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

    let document = elements[0].ownerGlobal.document;
    let url = document.documentURI;
    let callback = documentToEventCallbackMap.get(document);

    for (let element of elements) {
      let clickCallback = () => {
        callback({
          type,
          url,
          action: clickAction,
        });
      };
      element.addEventListener("click", clickCallback);

      let keydownCallback = event => {
        if (event.key == "Enter") {
          callback({
            type,
            url,
            action: keydownEnterAction,
          });
        }
      };
      element.addEventListener("keydown", keydownCallback);

      document.ownerGlobal.addEventListener(
        "pagehide",
        () => {
          element.removeEventListener("click", clickCallback);
          element.removeEventListener("keydown", keydownCallback);
        },
        { once: true }
      );
    }
  }
}

/**
 * An object indicating which elements to examine for domains to extract and
 * which heuristic technique to use to extract that element's domain.
 *
 * @typedef {object} ExtractorInfo
 * @property {string} selectors
 *  A string representing the CSS selector that targets the elements on the
 *  page that contain domains we want to extract.
 * @property {string} method
 *  A string representing which domain extraction heuristic to use.
 *  One of: "href" or "data-attribute".
 * @property {object | null} options
 *  Options related to the domain extraction heuristic used.
 * @property {string | null} options.dataAttributeKey
 *  The key name of the data attribute to lookup.
 * @property {string | null} options.queryParamKey
 *  The key name of the query param value to lookup.
 */

/**
 * DomainExtractor examines elements on a page to retrieve the domains.
 */
class DomainExtractor {
  /**
   * Extract domains from the page using an array of information pertaining to
   * the SERP.
   *
   * @param {Document} document
   *  The document for the SERP we are extracting domains from.
   * @param {Array<ExtractorInfo>} extractorInfos
   *  Information used to target the domains we need to extract.
   * @return {Set<string>}
   *  A set of the domains extracted from the page.
   */
  extractDomainsFromDocument(document, extractorInfos) {
    let extractedDomains = new Set();
    if (!extractorInfos?.length) {
      return extractedDomains;
    }

    for (let extractorInfo of extractorInfos) {
      if (!extractorInfo.selectors) {
        continue;
      }

      let elements = document.querySelectorAll(extractorInfo.selectors);
      if (!elements) {
        continue;
      }

      switch (extractorInfo.method) {
        case "href": {
          // Origin is used in case a URL needs to be made absolute.
          let origin = new URL(document.documentURI).origin;
          this.#fromElementsConvertHrefsIntoDomains(
            elements,
            origin,
            extractedDomains,
            extractorInfo.options?.queryParamKey
          );
          break;
        }
        case "data-attribute": {
          this.#fromElementsRetrieveDataAttributeValues(
            elements,
            extractorInfo.options?.dataAttributeKey,
            extractedDomains
          );
          break;
        }
      }
    }

    return extractedDomains;
  }

  /**
   * Given a list of elements, extract domains using href attributes. If the
   * URL in the href includes the specified query param, the domain will be
   * that query param's value. Otherwise it will be the hostname of the href
   * attribute's URL.
   *
   * @param {NodeList<Element>} elements
   *  A list of elements from the page whose href attributes we want to
   *  inspect.
   * @param {string} origin
   *  Origin of the current page.
   * @param {Set<string>} extractedDomains
   *  The result set of domains extracted from the page.
   * @param {string | null} queryParam
   *  An optional query param to search for in an element's href attribute.
   */
  #fromElementsConvertHrefsIntoDomains(
    elements,
    origin,
    extractedDomains,
    queryParam
  ) {
    for (let element of elements) {
      let href = element.getAttribute("href");

      let url;
      try {
        url = new URL(href, origin);
      } catch (ex) {
        continue;
      }

      // Ignore non-standard protocols.
      if (url.protocol != "https:" && url.protocol != "http:") {
        continue;
      }

      let domain = queryParam ? url.searchParams.get(queryParam) : url.hostname;
      if (domain && !extractedDomains.has(domain)) {
        extractedDomains.add(domain);
      }
    }
  }

  /**
   * Given a list of elements, examine each for the specified data attribute.
   * If found, add that data attribute's value to the result set of extracted
   * domains as is.
   *
   * @param {NodeList<Element>} elements
   *  A list of elements from the page whose data attributes we want to
   *  inspect.
   * @param {string} attribute
   *  The name of a data attribute to search for within an element.
   * @param {Set<string>} extractedDomains
   *  The result set of domains extracted from the page.
   */
  #fromElementsRetrieveDataAttributeValues(
    elements,
    attribute,
    extractedDomains
  ) {
    for (let element of elements) {
      let value = element.dataset[attribute];
      if (value && !extractedDomains.has(value)) {
        extractedDomains.add(value);
      }
    }
  }
}

export const domainExtractor = new DomainExtractor();
const searchProviders = new SearchProviders();
const searchAdImpression = new SearchAdImpression();

const documentToEventCallbackMap = new WeakMap();

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
   * Amount of time to wait after a page event before examining the page
   * for ads.
   *
   * @type {number | null}
   */
  #adTimeout;
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
    }

    if (
      lazy.serpEventsEnabled &&
      providerInfo.components?.length &&
      (eventType == "load" || eventType == "pageshow")
    ) {
      // Start performance measurements.
      let start = Cu.now();
      let timerId = Glean.serp.categorizationDuration.start();

      let pageActionCallback = info => {
        this.sendAsyncMessage("SearchTelemetry:Action", {
          type: info.type,
          url: info.url,
          action: info.action,
        });
      };
      documentToEventCallbackMap.set(this.document, pageActionCallback);

      let componentToVisibilityMap, hrefToComponentMap;
      try {
        let result = searchAdImpression.categorize(anchors, doc);
        componentToVisibilityMap = result.componentToVisibilityMap;
        hrefToComponentMap = result.hrefToComponentMap;
      } catch (e) {
        // Cancel the timer if an error encountered.
        Glean.serp.categorizationDuration.cancel(timerId);
      }

      if (componentToVisibilityMap && hrefToComponentMap) {
        // End measurements.
        ChromeUtils.addProfilerMarker(
          "SearchSERPTelemetryChild._checkForAdLink",
          start,
          "Checked anchors for visibility"
        );
        Glean.serp.categorizationDuration.stopAndAccumulate(timerId);
        this.sendAsyncMessage("SearchTelemetry:AdImpressions", {
          adImpressions: componentToVisibilityMap,
          hrefToComponentMap,
          url,
        });
      }
    }

    if (
      lazy.serpEventTelemetryCategorization &&
      providerInfo.domainExtraction &&
      (eventType == "load" || eventType == "pageshow")
    ) {
      let start = Cu.now();
      let nonAdDomains = domainExtractor.extractDomainsFromDocument(
        doc,
        providerInfo.domainExtraction.nonAds
      );
      let adDomains = domainExtractor.extractDomainsFromDocument(
        doc,
        providerInfo.domainExtraction.ads
      );

      this.sendAsyncMessage("SearchTelemetry:Domains", {
        url,
        nonAdDomains,
        adDomains,
      });

      ChromeUtils.addProfilerMarker(
        "SearchSERPTelemetryChild._checkForAdLink",
        start,
        "Extract domains from elements"
      );
    }
  }

  /**
   * Checks for the presence of certain components on the page that are
   * required for recording the page impression.
   */
  #checkForPageImpressionComponents() {
    let url = this.document.documentURI;
    let providerInfo = this._getProviderInfoForUrl(url);
    if (providerInfo.components?.length) {
      searchAdImpression.providerInfo = providerInfo;
      let start = Cu.now();
      let shoppingTabDisplayed = searchAdImpression.hasShoppingTab(
        this.document
      );
      ChromeUtils.addProfilerMarker(
        "SearchSERPTelemetryChild.#recordImpression",
        start,
        "Checked for shopping tab"
      );
      this.sendAsyncMessage("SearchTelemetry:PageImpression", {
        url,
        shoppingTabDisplayed,
      });
    }
  }

  /**
   * Handles events received from the actor child notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    if (!this.#urlIsSERP(this.document.documentURI)) {
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
        break;
      }
    }
  }

  #urlIsSERP(url) {
    let provider = this._getProviderInfoForUrl(this.document.documentURI);
    if (provider) {
      // Some URLs can match provider info but also be the provider's homepage
      // instead of a SERP.
      // e.g. https://example.com/ vs. https://example.com/?foo=bar
      // To check this, we look for the presence of the query parameter
      // that contains a search term.
      let queries = new URLSearchParams(url.split("#")[0].split("?")[1]);
      for (let queryParamName of provider.queryParamNames) {
        if (queries.get(queryParamName)) {
          return true;
        }
      }
    }
    return false;
  }

  #cancelCheck() {
    if (this._waitForContentTimeout) {
      lazy.clearTimeout(this._waitForContentTimeout);
    }
  }

  #check(eventType) {
    if (!this.#adTimeout) {
      this.#adTimeout = Services.cpmm.sharedData.get(
        SEARCH_TELEMETRY_SHARED.LOAD_TIMEOUT
      );
    }
    this.#cancelCheck();
    this._waitForContentTimeout = lazy.setTimeout(() => {
      this._checkForAdLink(eventType);
    }, this.#adTimeout);
  }
}
