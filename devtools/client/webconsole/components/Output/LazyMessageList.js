/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   MIT License
 *
 *   Copyright (c) 2019 Oleg Grishechkin
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */
"use strict";

const {
  Fragment,
  Component,
  createElement,
  createRef,
} = require("devtools/client/shared/vendor/react");

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);

// This element is a webconsole optimization for handling large numbers of
// console messages. The purpose is to only create DOM elements for messages
// which are actually visible within the scrollport. This code was based on
// Oleg Grishechkin's react-viewport-list element - however, it has been quite
// heavily modified, to the point that it is mostly unrecognizable. The most
// notable behavioral modification is that the list implements the behavior of
// pinning the scrollport to the bottom of the scroll container.
class LazyMessageList extends Component {
  static get propTypes() {
    return {
      viewportRef: PropTypes.shape({ current: PropTypes.instanceOf(Element) })
        .isRequired,
      items: PropTypes.array.isRequired,
      itemsToKeepAlive: PropTypes.shape({
        has: PropTypes.func,
        keys: PropTypes.func,
        size: PropTypes.number,
      }).isRequired,
      editorMode: PropTypes.bool.isRequired,
      itemDefaultHeight: PropTypes.number.isRequired,
      scrollOverdrawCount: PropTypes.number.isRequired,
      renderItem: PropTypes.func.isRequired,
      shouldScrollBottom: PropTypes.func.isRequired,
      cacheGeneration: PropTypes.number.isRequired,
      serviceContainer: PropTypes.shape({
        emitForTests: PropTypes.func.isRequired,
      }),
    };
  }

  constructor(props) {
    super(props);
    this.#initialized = false;
    this.#topBufferRef = createRef();
    this.#bottomBufferRef = createRef();
    this.#viewportHeight = window.innerHeight;
    this.#startIndex = 0;
    this.#resizeObserver = null;
    this.#cachedHeights = [];

    this.#scrollHandlerBinding = this.#scrollHandler.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillUpdate(nextProps, nextState) {
    if (nextProps.cacheGeneration !== this.props.cacheGeneration) {
      this.#cachedHeights = [];
      this.#startIndex = 0;
    } else if (
      (this.props.shouldScrollBottom() &&
        nextProps.items.length > this.props.items.length) ||
      this.#startIndex > nextProps.items.length - this.#numItemsToDraw
    ) {
      this.#startIndex = Math.max(
        0,
        nextProps.items.length - this.#numItemsToDraw
      );
    }
  }

  componentDidUpdate(prevProps) {
    const { viewportRef, serviceContainer } = this.props;
    if (!viewportRef.current || !this.#topBufferRef.current) {
      return;
    }

    if (!this.#initialized) {
      // We set these up from a one-time call in componentDidUpdate, rather than in
      // componentDidMount, because we need the parent to be mounted first, to add
      // listeners to it, and React orders things such that children mount before
      // parents.
      this.#addListeners();
    }

    if (!this.#initialized || prevProps.editorMode !== this.props.editorMode) {
      this.#resizeObserver.observe(viewportRef.current);
    }

    this.#initialized = true;

    // Since we updated, we're now going to compute the heights of all visible
    // elements and store them in a cache. This allows us to get more accurate
    // buffer regions to make scrolling correct when these elements no longer
    // exist.
    let index = this.#startIndex;
    let element = this.#topBufferRef.current.nextSibling;
    let elementRect = element?.getBoundingClientRect();
    while (
      Element.isInstance(element) &&
      index < this.#clampedEndIndex &&
      element !== this.#bottomBufferRef.current
    ) {
      const next = element.nextSibling;
      const nextRect = next.getBoundingClientRect();
      this.#cachedHeights[index] = nextRect.top - elementRect.top;
      element = next;
      elementRect = nextRect;
      index++;
    }

    serviceContainer.emitForTests("lazy-message-list-updated-or-noop");
  }

  componentWillUnmount() {
    this.#removeListeners();
  }

  #initialized;
  #topBufferRef;
  #bottomBufferRef;
  #viewportHeight;
  #startIndex;
  #resizeObserver;
  #cachedHeights;
  #scrollHandlerBinding;

  get #maxIndex() {
    return this.props.items.length - 1;
  }

  get #overdrawHeight() {
    return this.props.scrollOverdrawCount * this.props.itemDefaultHeight;
  }

  get #numItemsToDraw() {
    const scrollingWindowCount = Math.ceil(
      this.#viewportHeight / this.props.itemDefaultHeight
    );
    return scrollingWindowCount + 2 * this.props.scrollOverdrawCount;
  }

  get #unclampedEndIndex() {
    return this.#startIndex + this.#numItemsToDraw;
  }

  // Since the "end index" is computed based off a fixed offset from the start
  // index, it can exceed the length of our items array. This is just a helper
  // to ensure we don't exceed that.
  get #clampedEndIndex() {
    return Math.min(this.#unclampedEndIndex, this.props.items.length);
  }

  /**
   * Increases our start index until we've passed enough elements to cover
   * the difference in px between where we are and where we want to be.
   *
   * @param Number startIndex
   *        The current value of our start index.
   * @param Number deltaPx
   *        The difference in pixels between where we want to be and
   *        where we are.
   * @return {Number} The new computed start index.
   */
  #increaseStartIndex(startIndex, deltaPx) {
    for (let i = startIndex + 1; i < this.props.items.length; i++) {
      deltaPx -= this.#cachedHeights[i];
      startIndex = i;

      if (deltaPx <= 0) {
        break;
      }
    }
    return startIndex;
  }

  /**
   * Decreases our start index until we've passed enough elements to cover
   * the difference in px between where we are and where we want to be.
   *
   * @param Number startIndex
   *        The current value of our start index.
   * @param Number deltaPx
   *        The difference in pixels between where we want to be and
   *        where we are.
   * @return {Number} The new computed start index.
   */
  #decreaseStartIndex(startIndex, diff) {
    for (let i = startIndex - 1; i >= 0; i--) {
      diff -= this.#cachedHeights[i];
      startIndex = i;

      if (diff <= 0) {
        break;
      }
    }
    return startIndex;
  }

  #scrollHandler() {
    if (!this.props.viewportRef.current || !this.#topBufferRef.current) {
      return;
    }

    const scrollportMin =
      this.props.viewportRef.current.getBoundingClientRect().top -
      this.#overdrawHeight;
    const uppermostItemRect = this.#topBufferRef.current.nextSibling.getBoundingClientRect();
    const uppermostItemMin = uppermostItemRect.top;
    const uppermostItemMax = uppermostItemRect.bottom;

    let nextStartIndex = this.#startIndex;
    const downwardPx = scrollportMin - uppermostItemMax;
    const upwardPx = uppermostItemMin - scrollportMin;
    if (downwardPx > 0) {
      nextStartIndex = this.#increaseStartIndex(nextStartIndex, downwardPx);
    } else if (upwardPx > 0) {
      nextStartIndex = this.#decreaseStartIndex(nextStartIndex, upwardPx);
    }

    nextStartIndex = Math.max(
      0,
      Math.min(nextStartIndex, this.props.items.length - this.#numItemsToDraw)
    );

    if (nextStartIndex !== this.#startIndex) {
      this.#startIndex = nextStartIndex;
      this.forceUpdate();
    } else {
      const { serviceContainer } = this.props;
      serviceContainer.emitForTests("lazy-message-list-updated-or-noop");
    }
  }

  #addListeners() {
    const { viewportRef } = this.props;
    viewportRef.current.addEventListener("scroll", this.#scrollHandlerBinding);
    this.#resizeObserver = new ResizeObserver(entries => {
      this.#viewportHeight =
        viewportRef.current.parentNode.parentNode.clientHeight;
      this.forceUpdate();
    });
  }

  #removeListeners() {
    const { viewportRef } = this.props;
    this.#resizeObserver?.disconnect();
    viewportRef.current?.removeEventListener(
      "scroll",
      this.#scrollHandlerBinding
    );
  }

  get bottomBuffer() {
    return this.#bottomBufferRef.current;
  }

  isItemNearBottom(index) {
    return index >= this.props.items.length - this.#numItemsToDraw;
  }

  render() {
    const {
      items,
      itemDefaultHeight,
      renderItem,
      itemsToKeepAlive,
    } = this.props;
    if (!items.length) {
      return createElement(Fragment, {
        key: "LazyMessageList",
      });
    }

    // Resize our cached heights to fit if necessary.
    const countUncached = items.length - this.#cachedHeights.length;
    if (countUncached > 0) {
      // It would be lovely if javascript allowed us to resize an array in one
      // go. I think this is the closest we can get to that. This in theory
      // allows us to realloc, and doesn't require copying the whole original
      // array like concat does.
      this.#cachedHeights.push(...Array(countUncached).fill(itemDefaultHeight));
    }

    let topBufferHeight = 0;
    let bottomBufferHeight = 0;
    // We can't compute the bottom buffer height until the end, so we just
    // store the index of where it needs to go.
    let bottomBufferIndex = 0;
    let currentChild = 0;
    const startIndex = this.#startIndex;
    const endIndex = this.#clampedEndIndex;
    // We preallocate this array to avoid allocations in the loop. The minimum,
    // and typical length for it is the size of the body plus 2 for the top and
    // bottom buffers. It can be bigger due to itemsToKeepAlive, but we can't just
    // add the size, since itemsToKeepAlive could in theory hold items which are
    // not even in the list.
    const children = new Array(endIndex - startIndex + 2);
    const pushChild = c => {
      if (currentChild >= children.length) {
        children.push(c);
      } else {
        children[currentChild] = c;
      }
      return currentChild++;
    };
    for (let i = 0; i < items.length; i++) {
      const itemId = items[i];
      if (i < startIndex) {
        if (i == 0 || itemsToKeepAlive.has(itemId)) {
          // If this is our first item, and we wouldn't otherwise be rendering
          // it, we want to ensure that it's at the beginning of our children
          // array to ensure keyboard navigation functions properly.
          pushChild(renderItem(itemId, i));
        } else {
          topBufferHeight += this.#cachedHeights[i];
        }
      } else if (i < endIndex) {
        if (i == startIndex) {
          pushChild(
            createElement("div", {
              key: "LazyMessageListTop",
              className: "lazy-message-list-top",
              ref: this.#topBufferRef,
              style: { height: topBufferHeight },
            })
          );
        }
        pushChild(renderItem(itemId, i));
        if (i == endIndex - 1) {
          // We're just reserving the bottom buffer's spot in the children
          // array here. We will create the actual element and assign it at
          // this index after the loop.
          bottomBufferIndex = pushChild(null);
        }
      } else if (i == items.length - 1 || itemsToKeepAlive.has(itemId)) {
        // Similarly to the logic for our first item, we also want to ensure
        // that our last item is always rendered as the last item in our
        // children array.
        pushChild(renderItem(itemId, i));
      } else {
        bottomBufferHeight += this.#cachedHeights[i];
      }
    }

    children[bottomBufferIndex] = createElement("div", {
      key: "LazyMessageListBottom",
      className: "lazy-message-list-bottom",
      ref: this.#bottomBufferRef,
      style: { height: bottomBufferHeight },
    });

    return createElement(
      Fragment,
      {
        key: "LazyMessageList",
      },
      children
    );
  }
}

module.exports = LazyMessageList;
