/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// **********
// Title: drag.js

// ----------
// Variable: drag
// The Drag that's currently in process.
var drag = {
  info: null,
  zIndex: 100,
  lastMoveTime: 0
};

//----------
//Variable: resize
//The resize (actually a Drag) that is currently in process
var resize = {
  info: null,
  lastMoveTime: 0
};

// ##########
// Class: Drag (formerly DragInfo)
// Helper class for dragging <Item>s
//
// ----------
// Constructor: Drag
// Called to create a Drag in response to an <Item> draggable "start" event.
// Note that it is also used partially during <Item>'s resizable method as well.
//
// Parameters:
//   item - The <Item> being dragged
//   event - The DOM event that kicks off the drag
function Drag(item, event) {
  Utils.assert(item && (item.isAnItem || item.isAFauxItem), 
      'must be an item, or at least a faux item');

  this.item = item;
  this.el = item.container;
  this.$el = iQ(this.el);
  this.parent = this.item.parent;
  this.startPosition = new Point(event.clientX, event.clientY);
  this.startTime = Date.now();

  this.item.isDragging = true;
  this.item.setZ(999999);

  this.safeWindowBounds = Items.getSafeWindowBounds();

  Trenches.activateOthersTrenches(this.el);
};

Drag.prototype = {
  // ----------
  // Function: toString
  // Prints [Drag (item)] for debug use
  toString: function Drag_toString() {
    return "[Drag (" + this.item + ")]";
  },

  // ----------
  // Function: snapBounds
  // Adjusts the given bounds according to the currently active trenches. Used by <Drag.snap>
  //
  // Parameters:
  //   bounds             - (<Rect>) bounds
  //   stationaryCorner   - which corner is stationary? by default, the top left in LTR mode,
  //                        and top right in RTL mode.
  //                        "topleft", "bottomleft", "topright", "bottomright"
  //   assumeConstantSize - (boolean) whether the bounds' dimensions are sacred or not.
  //   keepProportional   - (boolean) if assumeConstantSize is false, whether we should resize
  //                        proportionally or not
  //   checkItemStatus    - (boolean) make sure this is a valid item which should be snapped
  snapBounds: function Drag_snapBounds(bounds, stationaryCorner, assumeConstantSize, keepProportional, checkItemStatus) {
    if (!stationaryCorner)
      stationaryCorner = UI.rtl ? 'topright' : 'topleft';
    var update = false; // need to update
    var updateX = false;
    var updateY = false;
    var newRect;
    var snappedTrenches = {};

    // OH SNAP!

    // if we aren't holding down the meta key or have trenches disabled...
    if (!Keys.meta && !Trenches.disabled) {
      // snappable = true if we aren't a tab on top of something else, and
      // there's no active drop site...
      let snappable = !(this.item.isATabItem &&
                       this.item.overlapsWithOtherItems()) &&
                       !iQ(".acceptsDrop").length;
      if (!checkItemStatus || snappable) {
        newRect = Trenches.snap(bounds, stationaryCorner, assumeConstantSize,
                                keepProportional);
        if (newRect) { // might be false if no changes were made
          update = true;
          snappedTrenches = newRect.snappedTrenches || {};
          bounds = newRect;
        }
      }
    }

    // make sure the bounds are in the window.
    newRect = this.snapToEdge(bounds, stationaryCorner, assumeConstantSize,
                              keepProportional);
    if (newRect) {
      update = true;
      bounds = newRect;
      Utils.extend(snappedTrenches, newRect.snappedTrenches);
    }

    Trenches.hideGuides();
    for (var edge in snappedTrenches) {
      var trench = snappedTrenches[edge];
      if (typeof trench == 'object') {
        trench.showGuide = true;
        trench.show();
      }
    }

    return update ? bounds : false;
  },

  // ----------
  // Function: snap
  // Called when a drag or mousemove occurs. Set the bounds based on the mouse move first, then
  // call snap and it will adjust the item's bounds if appropriate. Also triggers the display of
  // trenches that it snapped to.
  //
  // Parameters:
  //   stationaryCorner   - which corner is stationary? by default, the top left in LTR mode,
  //                        and top right in RTL mode.
  //                        "topleft", "bottomleft", "topright", "bottomright"
  //   assumeConstantSize - (boolean) whether the bounds' dimensions are sacred or not.
  //   keepProportional   - (boolean) if assumeConstantSize is false, whether we should resize
  //                        proportionally or not
  snap: function Drag_snap(stationaryCorner, assumeConstantSize, keepProportional) {
    var bounds = this.item.getBounds();
    bounds = this.snapBounds(bounds, stationaryCorner, assumeConstantSize, keepProportional, true);
    if (bounds) {
      this.item.setBounds(bounds, true);
      return true;
    }
    return false;
  },

  // --------
  // Function: snapToEdge
  // Returns a version of the bounds snapped to the edge if it is close enough. If not,
  // returns false. If <Keys.meta> is true, this function will simply enforce the
  // window edges.
  //
  // Parameters:
  //   rect - (<Rect>) current bounds of the object
  //   stationaryCorner   - which corner is stationary? by default, the top left in LTR mode,
  //                        and top right in RTL mode.
  //                        "topleft", "bottomleft", "topright", "bottomright"
  //   assumeConstantSize - (boolean) whether the rect's dimensions are sacred or not
  //   keepProportional   - (boolean) if we are allowed to change the rect's size, whether the
  //                                  dimensions should scaled proportionally or not.
  snapToEdge: function Drag_snapToEdge(rect, stationaryCorner, assumeConstantSize, keepProportional) {

    var swb = this.safeWindowBounds;
    var update = false;
    var updateX = false;
    var updateY = false;
    var snappedTrenches = {};

    var snapRadius = (Keys.meta ? 0 : Trenches.defaultRadius);
    if (rect.left < swb.left + snapRadius ) {
      if (stationaryCorner.indexOf('right') > -1 && !assumeConstantSize)
        rect.width = rect.right - swb.left;
      rect.left = swb.left;
      update = true;
      updateX = true;
      snappedTrenches.left = 'edge';
    }

    if (rect.right > swb.right - snapRadius) {
      if (updateX || !assumeConstantSize) {
        var newWidth = swb.right - rect.left;
        if (keepProportional)
          rect.height = rect.height * newWidth / rect.width;
        rect.width = newWidth;
        update = true;
      } else if (!updateX || !Trenches.preferLeft) {
        rect.left = swb.right - rect.width;
        update = true;
      }
      snappedTrenches.right = 'edge';
      delete snappedTrenches.left;
    }
    if (rect.top < swb.top + snapRadius) {
      if (stationaryCorner.indexOf('bottom') > -1 && !assumeConstantSize)
        rect.height = rect.bottom - swb.top;
      rect.top = swb.top;
      update = true;
      updateY = true;
      snappedTrenches.top = 'edge';
    }
    if (rect.bottom > swb.bottom - snapRadius) {
      if (updateY || !assumeConstantSize) {
        var newHeight = swb.bottom - rect.top;
        if (keepProportional)
          rect.width = rect.width * newHeight / rect.height;
        rect.height = newHeight;
        update = true;
      } else if (!updateY || !Trenches.preferTop) {
        rect.top = swb.bottom - rect.height;
        update = true;
      }
      snappedTrenches.top = 'edge';
      delete snappedTrenches.bottom;
    }

    if (update) {
      rect.snappedTrenches = snappedTrenches;
      return rect;
    }
    return false;
  },

  // ----------
  // Function: drag
  // Called in response to an <Item> draggable "drag" event.
  drag: function Drag_drag(event) {
    this.snap(UI.rtl ? 'topright' : 'topleft', true);

    if (this.parent && this.parent.expanded) {
      var distance = this.startPosition.distance(new Point(event.clientX, event.clientY));
      if (distance > 100) {
        this.parent.remove(this.item);
        this.parent.collapse();
      }
    }
  },

  // ----------
  // Function: stop
  // Called in response to an <Item> draggable "stop" event.
  //
  // Parameters:
  //  immediately - bool for doing the pushAway immediately, without animation
  stop: function Drag_stop(immediately) {
    Trenches.hideGuides();
    this.item.isDragging = false;

    if (this.parent && this.parent != this.item.parent)
      this.parent.closeIfEmpty();

    if (this.parent && this.parent.expanded)
      this.parent.arrange();

    if (this.item.parent)
      this.item.parent.arrange();

    if (this.item.isAGroupItem) {
      this.item.setZ(drag.zIndex);
      drag.zIndex++;

      this.item.pushAway(immediately);
    }

    Trenches.disactivate();
  }
};
