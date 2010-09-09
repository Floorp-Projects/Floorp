/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is trench.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 * Ian Gilman <ian@iangilman.com>
 * Aza Raskin <aza@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// **********
// Title: trench.js

// ##########
// Class: Trench
//
// Class for drag-snapping regions; called "trenches" as they are long and narrow.

// Constructor: Trench
//
// Parameters:
//   element - the DOM element for Item (GroupItem or TabItem) from which the trench is projected
//   xory - either "x" or "y": whether the trench's <position> is along the x- or y-axis.
//     In other words, if "x", the trench is vertical; if "y", the trench is horizontal.
//   type - either "border" or "guide". Border trenches mark the border of an Item.
//     Guide trenches extend out (unless they are intercepted) and act as "guides".
//   edge - which edge of the Item that this trench corresponds to.
//     Either "top", "left", "bottom", or "right".
function Trench(element, xory, type, edge) {
  //----------
  // Variable: id
  // (integer) The id for the Trench. Set sequentially via <Trenches.nextId>
  this.id = Trenches.nextId++;

  // ---------
  // Variables: Initial parameters
  //   element - (DOMElement)
  //   parentItem - <Item> which projects this trench; to be set with setParentItem
  //   xory - (string) "x" or "y"
  //   type - (string) "border" or "guide"
  //   edge - (string) "top", "left", "bottom", or "right"
  this.el = element;
  this.parentItem = null;
  this.xory = xory; // either "x" or "y"
  this.type = type; // "border" or "guide"
  this.edge = edge; // "top", "left", "bottom", or "right"

  this.$el = iQ(this.el);

  //----------
  // Variable: dom
  // (array) DOM elements for visible reflexes of the Trench
  this.dom = [];

  //----------
  // Variable: showGuide
  // (boolean) Whether this trench will project a visible guide (dotted line) or not.
  this.showGuide = false;

  //----------
  // Variable: active
  // (boolean) Whether this trench is currently active or not.
  // Basically every trench aside for those projected by the Item currently being dragged
  // all become active.
  this.active = false;
  this.gutter = Items.defaultGutter;

  //----------
  // Variable: position
  // (integer) position is the position that we should snap to.
  this.position = 0;

  //----------
  // Variables: some Ranges
  //   range - (<Range>) explicit range; this is along the transverse axis
  //   minRange - (<Range>) the minimum active range
  //   activeRange - (<Range>) the currently active range
  this.range = new Range(0,10000);
  this.minRange = new Range(0,0);
  this.activeRange = new Range(0,10000);
};

Trench.prototype = {
  //----------
  // Variable: radius
  // (integer) radius is how far away we should snap from
  get radius() this.customRadius || Trenches.defaultRadius,

  setParentItem: function Trench_setParentItem(item) {
    if (!item.isAnItem) {
      Utils.assert(false, "parentItem must be an Item");
      return false;
    }
    this.parentItem = item;
    return true;
  },

  //----------
  // Function: setPosition
  // set the trench's position.
  //
  // Parameters:
  //   position - (integer) px center position of the trench
  //   range - (<Range>) the explicit active range of the trench
  //   minRange - (<Range>) the minimum range of the trench
  setPosition: function Trench_setPos(position, range, minRange) {
    this.position = position;

    var page = Items.getPageBounds(true);

    // optionally, set the range.
    if (Utils.isRange(range)) {
      this.range = range;
    } else {
      this.range = new Range(0, (this.xory == 'x' ? page.height : page.width));
    }

    // if there's a minRange, set that too.
    if (Utils.isRange(minRange))
      this.minRange = minRange;

    // set the appropriate bounds as a rect.
    if (this.xory == "x") // vertical
      this.rect = new Rect(this.position - this.radius, this.range.min, 2 * this.radius, this.range.extent);
    else // horizontal
      this.rect = new Rect(this.range.min, this.position - this.radius, this.range.extent, 2 * this.radius);

    this.show(); // DEBUG
  },

  //----------
  // Function: setActiveRange
  // set the trench's currently active range.
  //
  // Parameters:
  //   activeRange - (<Range>)
  setActiveRange: function Trench_setActiveRect(activeRange) {
    if (!Utils.isRange(activeRange))
      return false;
    this.activeRange = activeRange;
    if (this.xory == "x") { // horizontal
      this.activeRect = new Rect(this.position - this.radius, this.activeRange.min, 2 * this.radius, this.activeRange.extent);
      this.guideRect = new Rect(this.position, this.activeRange.min, 0, this.activeRange.extent);
    } else { // vertical
      this.activeRect = new Rect(this.activeRange.min, this.position - this.radius, this.activeRange.extent, 2 * this.radius);
      this.guideRect = new Rect(this.activeRange.min, this.position, this.activeRange.extent, 0);
    }
    return true;
  },

  //----------
  // Function: setWithRect
  // Set the trench's position using the given rect. We know which side of the rect we should match
  // because we've already recorded this information in <edge>.
  //
  // Parameters:
  //   rect - (<Rect>)
  setWithRect: function Trench_setWithRect(rect) {

    if (!Utils.isRect(rect))
      Utils.error('argument must be Rect');

    // First, calculate the range for this trench.
    // Border trenches are always only active for the length of this range.
    // Guide trenches, however, still use this value as its minRange.
    if (this.xory == "x")
      var range = new Range(rect.top - this.gutter, rect.bottom + this.gutter);
    else
      var range = new Range(rect.left - this.gutter, rect.right + this.gutter);

    if (this.type == "border") {
      // border trenches have a range, so set that too.
      if (this.edge == "left")
        this.setPosition(rect.left - this.gutter, range);
      else if (this.edge == "right")
        this.setPosition(rect.right + this.gutter, range);
      else if (this.edge == "top")
        this.setPosition(rect.top - this.gutter, range);
      else if (this.edge == "bottom")
        this.setPosition(rect.bottom + this.gutter, range);
    } else if (this.type == "guide") {
      // guide trenches have no range, but do have a minRange.
      if (this.edge == "left")
        this.setPosition(rect.left, false, range);
      else if (this.edge == "right")
        this.setPosition(rect.right, false, range);
      else if (this.edge == "top")
        this.setPosition(rect.top, false, range);
      else if (this.edge == "bottom")
        this.setPosition(rect.bottom, false, range);
    }
  },

  //----------
  // Function: show
  //
  // Show guide (dotted line), if <showGuide> is true.
  //
  // If <Trenches.showDebug> is true, we will draw the trench. Active portions are drawn with 0.5
  // opacity. If <active> is false, the entire trench will be
  // very translucent.
  show: function Trench_show() { // DEBUG
    if (this.active && this.showGuide) {
      if (!this.dom.guideTrench)
        this.dom.guideTrench = iQ("<div/>").addClass('guideTrench').css({id: 'guideTrench'+this.id});
      var guideTrench = this.dom.guideTrench;
      guideTrench.css(this.guideRect.css());
      iQ("body").append(guideTrench);
    } else {
      if (this.dom.guideTrench) {
        this.dom.guideTrench.remove();
        delete this.dom.guideTrench;
      }
    }

    if (!Trenches.showDebug) {
      this.hide(true); // true for dontHideGuides
      return;
    }

    if (!this.dom.visibleTrench)
      this.dom.visibleTrench = iQ("<div/>")
        .addClass('visibleTrench')
        .addClass(this.type) // border or guide
        .css({id: 'visibleTrench'+this.id});
    var visibleTrench = this.dom.visibleTrench;

    if (!this.dom.activeVisibleTrench)
      this.dom.activeVisibleTrench = iQ("<div/>")
        .addClass('activeVisibleTrench')
        .addClass(this.type) // border or guide
        .css({id: 'activeVisibleTrench'+this.id});
    var activeVisibleTrench = this.dom.activeVisibleTrench;

    if (this.active)
      activeVisibleTrench.addClass('activeTrench');
    else
      activeVisibleTrench.removeClass('activeTrench');

    visibleTrench.css(this.rect.css());
    activeVisibleTrench.css((this.activeRect || this.rect).css());
    iQ("body").append(visibleTrench);
    iQ("body").append(activeVisibleTrench);
  },

  //----------
  // Function: hide
  // Hide the trench.
  hide: function Trench_hide(dontHideGuides) {
    if (this.dom.visibleTrench)
      this.dom.visibleTrench.remove();
    if (this.dom.activeVisibleTrench)
      this.dom.activeVisibleTrench.remove();
    if (!dontHideGuides && this.dom.guideTrench)
      this.dom.guideTrench.remove();
  },

  //----------
  // Function: rectOverlaps
  // Given a <Rect>, compute whether it overlaps with this trench. If it does, return an
  // adjusted ("snapped") <Rect>; if it does not overlap, simply return false.
  //
  // Note that simply overlapping is not all that is required to be affected by this function.
  // Trenches can only affect certain edges of rectangles... for example, a "left"-edge guide
  // trench should only affect left edges of rectangles. We don't snap right edges to left-edged
  // guide trenches. For border trenches, the logic is a bit different, so left snaps to right and
  // top snaps to bottom.
  //
  // Parameters:
  //   rect - (<Rect>) the rectangle in question
  //   stationaryCorner   - which corner is stationary? by default, the top left.
  //                        "topleft", "bottomleft", "topright", "bottomright"
  //   assumeConstantSize - (boolean) whether the rect's dimensions are sacred or not
  //   keepProportional - (boolean) if we are allowed to change the rect's size, whether the
  //                                dimensions should scaled proportionally or not.
  //
  // Returns:
  //   false - if rect does not overlap with this trench
  //   newRect - (<Rect>) an adjusted version of rect, if it is affected by this trench
  rectOverlaps: function Trench_rectOverlaps(rect,stationaryCorner,assumeConstantSize,keepProportional) {
    var edgeToCheck;
    if (this.type == "border") {
      if (this.edge == "left")
        edgeToCheck = "right";
      else if (this.edge == "right")
        edgeToCheck = "left";
      else if (this.edge == "top")
        edgeToCheck = "bottom";
      else if (this.edge == "bottom")
        edgeToCheck = "top";
    } else { // if trench type is guide or barrier...
      edgeToCheck = this.edge;
    }

    rect.adjustedEdge = edgeToCheck;

    switch (edgeToCheck) {
      case "left":
        if (this.ruleOverlaps(rect.left, rect.yRange)) {
          if (stationaryCorner.indexOf('right') > -1)
            rect.width = rect.right - this.position;
          rect.left = this.position;
          return rect;
        }
        break;
      case "right":
        if (this.ruleOverlaps(rect.right, rect.yRange)) {
          if (assumeConstantSize) {
            rect.left = this.position - rect.width;
          } else {
            var newWidth = this.position - rect.left;
            if (keepProportional)
              rect.height = rect.height * newWidth / rect.width;
            rect.width = newWidth;
          }
          return rect;
        }
        break;
      case "top":
        if (this.ruleOverlaps(rect.top, rect.xRange)) {
          if (stationaryCorner.indexOf('bottom') > -1)
            rect.height = rect.bottom - this.position;
          rect.top = this.position;
          return rect;
        }
        break;
      case "bottom":
        if (this.ruleOverlaps(rect.bottom, rect.xRange)) {
          if (assumeConstantSize) {
            rect.top = this.position - rect.height;
          } else {
            var newHeight = this.position - rect.top;
            if (keepProportional)
              rect.width = rect.width * newHeight / rect.height;
            rect.height = newHeight;
          }
          return rect;
        }
    }

    return false;
  },

  //----------
  // Function: ruleOverlaps
  // Computes whether the given "rule" (a line segment, essentially), given by the position and
  // range arguments, overlaps with the current trench. Note that this function assumes that
  // the rule and the trench are in the same direction: both horizontal, or both vertical.
  //
  // Parameters:
  //   position - (integer) a position in px
  //   range - (<Range>) the rule's range
  ruleOverlaps: function Trench_ruleOverlaps(position, range) {
    return (this.position - this.radius < position &&
           position < this.position + this.radius &&
           this.activeRange.overlaps(range));
  },

  //----------
  // Function: adjustRangeIfIntercept
  // Computes whether the given boundary (given as a position and its active range), perpendicular
  // to the trench, intercepts the trench or not. If it does, it returns an adjusted <Range> for
  // the trench. If not, it returns false.
  //
  // Parameters:
  //   position - (integer) the position of the boundary
  //   range - (<Range>) the target's range, on the trench's transverse axis
  adjustRangeIfIntercept: function Trench_adjustRangeIfIntercept(position, range) {
    if (this.position - this.radius > range.min && this.position + this.radius < range.max) {
      var activeRange = new Range(this.activeRange);

      // there are three ways this can go:
      // 1. position < minRange.min
      // 2. position > minRange.max
      // 3. position >= minRange.min && position <= minRange.max

      if (position < this.minRange.min) {
        activeRange.min = Math.min(this.minRange.min,position);
      } else if (position > this.minRange.max) {
        activeRange.max = Math.max(this.minRange.max,position);
      } else {
        // this should be impossible because items can't overlap and we've already checked
        // that the range intercepts.
      }
      return activeRange;
    }
    return false;
  },

  //----------
  // Function: calculateActiveRange
  // Computes and sets the <activeRange> for the trench, based on the <GroupItems> around.
  // This makes it so trenches' active ranges don't extend through other groupItems.
  calculateActiveRange: function Trench_calculateActiveRange() {

    // set it to the default: just the range itself.
    this.setActiveRange(this.range);

    // only guide-type trenches need to set a separate active range
    if (this.type != 'guide')
      return;

    var groupItems = GroupItems.groupItems;
    var trench = this;
    groupItems.forEach(function(groupItem) {
      if (groupItem.isDragging) // floating groupItems don't block trenches
        return;
      if (trench.el == groupItem.container) // groupItems don't block their own trenches
        return;
      var bounds = groupItem.getBounds();
      var activeRange = new Range();
      if (trench.xory == 'y') { // if this trench is horizontal...
        activeRange = trench.adjustRangeIfIntercept(bounds.left, bounds.yRange);
        if (activeRange)
          trench.setActiveRange(activeRange);
        activeRange = trench.adjustRangeIfIntercept(bounds.right, bounds.yRange);
        if (activeRange)
          trench.setActiveRange(activeRange);
      } else { // if this trench is vertical...
        activeRange = trench.adjustRangeIfIntercept(bounds.top, bounds.xRange);
        if (activeRange)
          trench.setActiveRange(activeRange);
        activeRange = trench.adjustRangeIfIntercept(bounds.bottom, bounds.xRange);
        if (activeRange)
          trench.setActiveRange(activeRange);
      }
    });
  }
};

// ##########
// Class: Trenches
// Singelton for managing all <Trench>es.
var Trenches = {
  // ---------
  // Variables:
  //   nextId - (integer) a counter for the next <Trench>'s <Trench.id> value.
  //   showDebug - (boolean) whether to draw the <Trench>es or not.
  //   defaultRadius - (integer) the default radius for new <Trench>es.
  nextId: 0,
  showDebug: false,
  defaultRadius: 10,

  // ---------
  // Variables: snapping preferences; used to break ties in snapping.
  //   preferTop - (boolean) prefer snapping to the top to the bottom
  //   preferLeft - (boolean) prefer snapping to the left to the right
  preferTop: true,
  preferLeft: true,

  trenches: [],

  // ---------
  // Function: getById
  // Return the specified <Trench>.
  //
  // Parameters:
  //   id - (integer)
  getById: function Trenches_getById(id) {
    return this.trenches[id];
  },

  // ---------
  // Function: register
  // Register a new <Trench> and returns the resulting <Trench> ID.
  //
  // Parameters:
  // See the constructor <Trench.Trench>'s parameters.
  //
  // Returns:
  //   id - (int) the new <Trench>'s ID.
  register: function Trenches_register(element, xory, type, edge) {
    var trench = new Trench(element, xory, type, edge);
    this.trenches[trench.id] = trench;
    return trench.id;
  },

  // ---------
  // Function: registerWithItem
  // Register a whole set of <Trench>es using an <Item> and returns the resulting <Trench> IDs.
  //
  // Parameters:
  //   item - the <Item> to project trenches
  //   type - either "border" or "guide"
  //
  // Returns:
  //   ids - array of the new <Trench>es' IDs.
  registerWithItem: function Trenches_registerWithItem(item, type) {
    var container = item.container;
    var ids = {};
    ids.left = Trenches.register(container,"x",type,"left");
    ids.right = Trenches.register(container,"x",type,"right");
    ids.top = Trenches.register(container,"y",type,"top");
    ids.bottom = Trenches.register(container,"y",type,"bottom");

    this.getById(ids.left).setParentItem(item);
    this.getById(ids.right).setParentItem(item);
    this.getById(ids.top).setParentItem(item);
    this.getById(ids.bottom).setParentItem(item);

    return ids;
  },

  // ---------
  // Function: unregister
  // Unregister one or more <Trench>es.
  //
  // Parameters:
  //   ids - (integer) a single <Trench> ID or (array) a list of <Trench> IDs.
  unregister: function Trenches_unregister(ids) {
    if (!Array.isArray(ids))
      ids = [ids];
    var self = this;
    ids.forEach(function(id) {
      self.trenches[id].hide();
      delete self.trenches[id];
    });
  },

  // ---------
  // Function: activateOthersTrenches
  // Activate all <Trench>es other than those projected by the current element.
  //
  // Parameters:
  //   element - (DOMElement) the DOM element of the Item being dragged or resized.
  activateOthersTrenches: function Trenches_activateOthersTrenches(element) {
    this.trenches.forEach(function(t) {
      if (t.el === element)
        return;
      if (t.parentItem && (t.parentItem.isAFauxItem ||
         t.parentItem.isDragging ||
         t.parentItem.isDropTarget))
        return;
      t.active = true;
      t.calculateActiveRange();
      t.show(); // debug
    });
  },

  // ---------
  // Function: disactivate
  // After <activateOthersTrenches>, disactivates all the <Trench>es again.
  disactivate: function Trenches_disactivate() {
    this.trenches.forEach(function(t) {
      t.active = false;
      t.showGuide = false;
      t.show();
    });
  },

  // ---------
  // Function: hideGuides
  // Hide all guides (dotted lines) en masse.
  hideGuides: function Trenches_hideGuides() {
    this.trenches.forEach(function(t) {
      t.showGuide = false;
      t.show();
    });
  },

  // ---------
  // Function: snap
  // Used to "snap" an object's bounds to active trenches and to the edge of the window.
  // If the meta key is down (<Key.meta>), it will not snap but will still enforce the rect
  // not leaving the safe bounds of the window.
  //
  // Parameters:
  //   rect               - (<Rect>) the object's current bounds
  //   stationaryCorner   - which corner is stationary? by default, the top left.
  //                        "topleft", "bottomleft", "topright", "bottomright"
  //   assumeConstantSize - (boolean) whether the rect's dimensions are sacred or not
  //   keepProportional   - (boolean) if we are allowed to change the rect's size, whether the
  //                                  dimensions should scaled proportionally or not.
  //
  // Returns:
  //   (<Rect>) - the updated bounds, if they were updated
  //   false - if the bounds were not updated
  snap: function Trenches_snap(rect,stationaryCorner,assumeConstantSize,keepProportional) {
    // hide all the guide trenches, because the correct ones will be turned on later.
    Trenches.hideGuides();

    var updated = false;
    var updatedX = false;
    var updatedY = false;

    var snappedTrenches = {};

    for (var i in this.trenches) {
      var t = this.trenches[i];
      if (!t.active || t.parentItem.isDropTarget)
        continue;
      // newRect will be a new rect, or false
      var newRect = t.rectOverlaps(rect,stationaryCorner,assumeConstantSize,keepProportional);

      if (newRect) { // if rectOverlaps returned an updated rect...

        if (assumeConstantSize && updatedX && updatedY)
          break;
        if (assumeConstantSize && updatedX && (newRect.adjustedEdge == "left"||newRect.adjustedEdge == "right"))
          continue;
        if (assumeConstantSize && updatedY && (newRect.adjustedEdge == "top"||newRect.adjustedEdge == "bottom"))
          continue;

        rect = newRect;
        updated = true;

        // register this trench as the "snapped trench" for the appropriate edge.
        snappedTrenches[newRect.adjustedEdge] = t;

        // if updatedX, we don't need to update x any more.
        if (newRect.adjustedEdge == "left" && this.preferLeft)
          updatedX = true;
        if (newRect.adjustedEdge == "right" && !this.preferLeft)
          updatedX = true;

        // if updatedY, we don't need to update x any more.
        if (newRect.adjustedEdge == "top" && this.preferTop)
          updatedY = true;
        if (newRect.adjustedEdge == "bottom" && !this.preferTop)
          updatedY = true;

      }
    }

    if (updated) {
      rect.snappedTrenches = snappedTrenches;
      return rect;
    }
    return false;
  },

  // ---------
  // Function: show
  // <Trench.show> all <Trench>es.
  show: function Trenches_show() {
    this.trenches.forEach(function(t) {
      t.show();
    });
  },

  // ---------
  // Function: toggleShown
  // Toggle <Trenches.showDebug> and trigger <Trenches.show>
  toggleShown: function Trenches_toggleShown() {
    this.showDebug = !this.showDebug;
    this.show();
  }
};
