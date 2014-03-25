#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton allows to transform the grid by repositioning a site's node
 * in the DOM and by showing or hiding the node. It additionally provides
 * convenience methods to work with a site's DOM node.
 */
let gTransformation = {
  /**
   * Returns the width of the left and top border of a cell. We need to take it
   * into account when measuring and comparing site and cell positions.
   */
  get _cellBorderWidths() {
    let cstyle = window.getComputedStyle(gGrid.cells[0].node, null);
    let widths = {
      left: parseInt(cstyle.getPropertyValue("border-left-width")),
      top: parseInt(cstyle.getPropertyValue("border-top-width"))
    };

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "_cellBorderWidths",
                          {value: widths, enumerable: true});

    return widths;
  },

  /**
   * Gets a DOM node's position.
   * @param aNode The DOM node.
   * @return A Rect instance with the position.
   */
  getNodePosition: function Transformation_getNodePosition(aNode) {
    let {left, top, width, height} = aNode.getBoundingClientRect();
    return new Rect(left + scrollX, top + scrollY, width, height);
  },

  /**
   * Fades a given site from zero to full opacity.
   * @param aSite The site to fade.
   */
  showSite: function (aSite) {
    let node = aSite.node;
    return this._setNodeOpacity(node, 1).then(() => {
      // Clear the style property.
      node.style.opacity = "";
    });
  },

  /**
   * Fades a given site from full to zero opacity.
   * @param aSite The site to fade.
   */
  hideSite: function (aSite) {
    return this._setNodeOpacity(aSite.node, 0);
  },

  /**
   * Allows to set a site's position.
   * @param aSite The site to re-position.
   * @param aPosition The desired position for the given site.
   */
  setSitePosition: function Transformation_setSitePosition(aSite, aPosition) {
    let style = aSite.node.style;
    let {top, left} = aPosition;

    style.top = top + "px";
    style.left = left + "px";
  },

  /**
   * Freezes a site in its current position by positioning it absolute.
   * @param aSite The site to freeze.
   */
  freezeSitePosition: function Transformation_freezeSitePosition(aSite) {
    if (this._isFrozen(aSite))
      return;

    let style = aSite.node.style;
    let comp = getComputedStyle(aSite.node, null);
    style.width = comp.getPropertyValue("width")
    style.height = comp.getPropertyValue("height");

    aSite.node.setAttribute("frozen", "true");
    this.setSitePosition(aSite, this.getNodePosition(aSite.node));
  },

  /**
   * Unfreezes a site by removing its absolute positioning.
   * @param aSite The site to unfreeze.
   */
  unfreezeSitePosition: function Transformation_unfreezeSitePosition(aSite) {
    if (!this._isFrozen(aSite))
      return;

    let style = aSite.node.style;
    style.left = style.top = style.width = style.height = "";
    aSite.node.removeAttribute("frozen");
  },

  /**
   * Slides the given site to the target node's position.
   * @param aSite The site to move.
   * @param aTarget The slide target.
   * @param aOptions Set of options (see below).
   *        unfreeze - unfreeze the site after sliding
   */
  slideSiteTo: function (aSite, aTarget, aOptions) {
    let currentPosition = this.getNodePosition(aSite.node);
    let targetPosition = this.getNodePosition(aTarget.node)
    let promise;

    // We need to take the width of a cell's border into account.
    targetPosition.left += this._cellBorderWidths.left;
    targetPosition.top += this._cellBorderWidths.top;

    // Nothing to do here if the positions already match.
    if (currentPosition.left == targetPosition.left &&
        currentPosition.top == targetPosition.top) {
      promise = Promise.resolve();
    } else {
      this.setSitePosition(aSite, targetPosition);
      promise = this._whenTransitionEnded(aSite.node, ["left", "top"]);
    }

    if (aOptions && aOptions.unfreeze) {
      promise = promise.then(() => this.unfreezeSitePosition(aSite));
    }

    return promise;
  },

  /**
   * Rearranges a given array of sites and moves them to their new positions or
   * fades in/out new/removed sites.
   * @param aSites An array of sites to rearrange.
   * @param aOptions Set of options (see below).
   *        unfreeze - unfreeze the site after rearranging
   */
  rearrangeSites: function (aSites, aOptions) {
    let self = this;
    let cells = gGrid.cells;
    let unfreeze = aOptions && aOptions.unfreeze;

    function* promises() {
      let index = 0;

      for (let site of aSites) {
        if (site && site !== gDrag.draggedSite) {
          if (!cells[index]) {
            // The site disappeared from the grid, hide it.
            yield self.hideSite(site);
          } else if (self._getNodeOpacity(site.node) != 1) {
            // The site disappeared before but is now back, show it.
            yield self.showSite(site);
          } else {
            // The site's position has changed, move it around.
            yield self._moveSite(site, index, {unfreeze: unfreeze});
          }
        }
        index++;
      }
    }

    return Promise.all([p for (p of promises())]);
  },

  /**
   * Listens for the 'transitionend' event on a given node.
   * @param aNode The node that is transitioned.
   * @param aProperties The properties we'll wait to be transitioned.
   */
  _whenTransitionEnded: function (aNode, aProperties) {
    let deferred = Promise.defer();
    let props = new Set(aProperties);
    aNode.addEventListener("transitionend", function onEnd(e) {
      if (props.has(e.propertyName)) {
        aNode.removeEventListener("transitionend", onEnd);
        deferred.resolve();
      }
    });
    return deferred.promise;
  },

  /**
   * Gets a given node's opacity value.
   * @param aNode The node to get the opacity value from.
   * @return The node's opacity value.
   */
  _getNodeOpacity: function Transformation_getNodeOpacity(aNode) {
    let cstyle = window.getComputedStyle(aNode, null);
    return cstyle.getPropertyValue("opacity");
  },

  /**
   * Sets a given node's opacity.
   * @param aNode The node to set the opacity value for.
   * @param aOpacity The opacity value to set.
   */
  _setNodeOpacity: function (aNode, aOpacity) {
    if (this._getNodeOpacity(aNode) == aOpacity) {
      return Promise.resolve();
    }

    aNode.style.opacity = aOpacity;
    return this._whenTransitionEnded(aNode, ["opacity"]);
  },

  /**
   * Moves a site to the cell with the given index.
   * @param aSite The site to move.
   * @param aIndex The target cell's index.
   * @param aOptions Options that are directly passed to slideSiteTo().
   */
  _moveSite: function (aSite, aIndex, aOptions) {
    this.freezeSitePosition(aSite);
    return this.slideSiteTo(aSite, gGrid.cells[aIndex], aOptions);
  },

  /**
   * Checks whether a site is currently frozen.
   * @param aSite The site to check.
   * @return Whether the given site is frozen.
   */
  _isFrozen: function Transformation_isFrozen(aSite) {
    return aSite.node.hasAttribute("frozen");
  }
};
