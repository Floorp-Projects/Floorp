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
 * The Original Code is items.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ian Gilman <ian@iangilman.com>
 * Aza Raskin <aza@mozilla.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 * Sean Dunn <seanedunn@yahoo.com>
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
// Title: items.js

// ##########
// Class: Item
// Superclass for all visible objects (<TabItem>s and <GroupItem>s).
//
// If you subclass, in addition to the things Item provides, you need to also provide these methods:
//   setBounds - function(rect, immediately, options)
//   setZ - function(value)
//   close - function()
//   save - function()
//
// Subclasses of Item must also provide the <Subscribable> interface.
//
// ... and this property:
//   defaultSize - a Point
//   locked - an object (see below)
//
// Make sure to call _init() from your subclass's constructor.
function Item() {
  // Variable: isAnItem
  // Always true for Items
  this.isAnItem = true;

  // Variable: bounds
  // The position and size of this Item, represented as a <Rect>.
  // This should never be modified without using setBounds()
  this.bounds = null;

  // Variable: zIndex
  // The z-index for this item.
  this.zIndex = 0;

  // Variable: debug
  // When set to true, displays a rectangle on the screen that corresponds with bounds.
  // May be used for additional debugging features in the future.
  this.debug = false;

  // Variable: $debug
  // If <debug> is true, this will be the iQ object for the visible rectangle.
  this.$debug = null;

  // Variable: container
  // The outermost DOM element that describes this item on screen.
  this.container = null;

  // Variable: locked
  // Affects whether an item can be pushed, closed, renamed, etc
  //
  // The object may have properties to specify what can't be changed:
  //   .bounds - true if it can't be pushed, dragged, resized, etc
  //   .close - true if it can't be closed
  //   .title - true if it can't be renamed
  this.locked = null;

  // Variable: parent
  // The groupItem that this item is a child of
  this.parent = null;

  // Variable: userSize
  // A <Point> that describes the last size specifically chosen by the user.
  // Used by unsquish.
  this.userSize = null;

  // Variable: dragOptions
  // Used by <draggable>
  //
  // Possible properties:
  //   cancelClass - A space-delimited list of classes that should cancel a drag
  //   start - A function to be called when a drag starts
  //   drag - A function to be called each time the mouse moves during drag
  //   stop - A function to be called when the drag is done
  this.dragOptions = null;

  // Variable: dropOptions
  // Used by <draggable> if the item is set to droppable.
  //
  // Possible properties:
  //   accept - A function to determine if a particular item should be accepted for dropping
  //   over - A function to be called when an item is over this item
  //   out - A function to be called when an item leaves this item
  //   drop - A function to be called when an item is dropped in this item
  this.dropOptions = null;

  // Variable: resizeOptions
  // Used by <resizable>
  //
  // Possible properties:
  //   minWidth - Minimum width allowable during resize
  //   minHeight - Minimum height allowable during resize
  //   aspectRatio - true if we should respect aspect ratio; default false
  //   start - A function to be called when resizing starts
  //   resize - A function to be called each time the mouse moves during resize
  //   stop - A function to be called when the resize is done
  this.resizeOptions = null;

  // Variable: isDragging
  // Boolean for whether the item is currently being dragged or not.
  this.isDragging = false;
};

Item.prototype = {
  // ----------
  // Function: _init
  // Initializes the object. To be called from the subclass's intialization function.
  //
  // Parameters:
  //   container - the outermost DOM element that describes this item onscreen.
  _init: function Item__init(container) {
    Utils.assert(typeof this.addSubscriber == 'function' && 
        typeof this.removeSubscriber == 'function' && 
        typeof this._sendToSubscribers == 'function',
        'Subclass must implement the Subscribable interface');
    Utils.assert(Utils.isDOMElement(container), 'container must be a DOM element');
    Utils.assert(typeof this.setBounds == 'function', 'Subclass must provide setBounds');
    Utils.assert(typeof this.setZ == 'function', 'Subclass must provide setZ');
    Utils.assert(typeof this.close == 'function', 'Subclass must provide close');
    Utils.assert(typeof this.save == 'function', 'Subclass must provide save');
    Utils.assert(Utils.isPoint(this.defaultSize), 'Subclass must provide defaultSize');
    Utils.assert(this.locked, 'Subclass must provide locked');
    Utils.assert(Utils.isRect(this.bounds), 'Subclass must provide bounds');

    this.container = container;

    if (this.debug) {
      this.$debug = iQ('<div>')
        .css({
          border: '2px solid green',
          zIndex: -10,
          position: 'absolute'
        })
        .appendTo('body');
    }

    iQ(this.container).data('item', this);

    // ___ drag
    this.dragOptions = {
      cancelClass: 'close stackExpander',
      start: function(e, ui) {
        if (this.isAGroupItem)
          GroupItems.setActiveGroupItem(this);
        // if we start dragging a tab within a group, start with dropSpace on.
        else if (this.parent != null)
          this.parent._dropSpaceActive = true;
        drag.info = new Drag(this, e);
      },
      drag: function(e) {
        drag.info.drag(e);
      },
      stop: function() {
        drag.info.stop();
        drag.info = null;
      },
      // The minimum the mouse must move after mouseDown in order to move an 
      // item
      minDragDistance: 3
    };

    // ___ drop
    this.dropOptions = {
      over: function() {},
      out: function() {
        let groupItem = drag.info.item.parent;
        if (groupItem)
          groupItem.remove(drag.info.$el, {dontClose: true});
        iQ(this.container).removeClass("acceptsDrop");
      },
      drop: function(event) {
        iQ(this.container).removeClass("acceptsDrop");
      },
      // Function: dropAcceptFunction
      // Given a DOM element, returns true if it should accept tabs being dropped on it.
      // Private to this file.
      accept: function dropAcceptFunction(item) {
        return (item && item.isATabItem && (!item.parent || !item.parent.expanded));
      }
    };

    // ___ resize
    var self = this;
    this.resizeOptions = {
      aspectRatio: self.keepProportional,
      minWidth: 90,
      minHeight: 90,
      start: function(e,ui) {
        if (this.isAGroupItem)
          GroupItems.setActiveGroupItem(this);
        resize.info = new Drag(this, e);
      },
      resize: function(e,ui) {
        resize.info.snap(UI.rtl ? 'topright' : 'topleft', false, self.keepProportional);
      },
      stop: function() {
        self.setUserSize();
        self.pushAway();
        resize.info.stop();
        resize.info = null;
      }
    };
  },

  // ----------
  // Function: getBounds
  // Returns a copy of the Item's bounds as a <Rect>.
  getBounds: function Item_getBounds() {
    Utils.assert(Utils.isRect(this.bounds), 'this.bounds should be a rect');
    return new Rect(this.bounds);
  },

  // ----------
  // Function: overlapsWithOtherItems
  // Returns true if this Item overlaps with any other Item on the screen.
  overlapsWithOtherItems: function Item_overlapsWithOtherItems() {
    var self = this;
    var items = Items.getTopLevelItems();
    var bounds = this.getBounds();
    return items.some(function(item) {
      if (item == self) // can't overlap with yourself.
        return false;
      var myBounds = item.getBounds();
      return myBounds.intersects(bounds);
    } );
  },

  // ----------
  // Function: setPosition
  // Moves the Item to the specified location.
  //
  // Parameters:
  //   left - the new left coordinate relative to the window
  //   top - the new top coordinate relative to the window
  //   immediately - if false or omitted, animates to the new position;
  //   otherwise goes there immediately
  setPosition: function Item_setPosition(left, top, immediately) {
    Utils.assert(Utils.isRect(this.bounds), 'this.bounds');
    this.setBounds(new Rect(left, top, this.bounds.width, this.bounds.height), immediately);
  },

  // ----------
  // Function: setSize
  // Resizes the Item to the specified size.
  //
  // Parameters:
  //   width - the new width in pixels
  //   height - the new height in pixels
  //   immediately - if false or omitted, animates to the new size;
  //   otherwise resizes immediately
  setSize: function Item_setSize(width, height, immediately) {
    Utils.assert(Utils.isRect(this.bounds), 'this.bounds');
    this.setBounds(new Rect(this.bounds.left, this.bounds.top, width, height), immediately);
  },

  // ----------
  // Function: setUserSize
  // Remembers the current size as one the user has chosen.
  setUserSize: function Item_setUserSize() {
    Utils.assert(Utils.isRect(this.bounds), 'this.bounds');
    this.userSize = new Point(this.bounds.width, this.bounds.height);
    this.save();
  },

  // ----------
  // Function: getZ
  // Returns the zIndex of the Item.
  getZ: function Item_getZ() {
    return this.zIndex;
  },

  // ----------
  // Function: setRotation
  // Rotates the object to the given number of degrees.
  setRotation: function Item_setRotation(degrees) {
    var value = degrees ? "rotate(%deg)".replace(/%/, degrees) : null;
    iQ(this.container).css({"-moz-transform": value});
  },

  // ----------
  // Function: setParent
  // Sets the receiver's parent to the given <Item>.
  setParent: function Item_setParent(parent) {
    this.parent = parent;
    this.removeTrenches();
    this.save();
  },

  // ----------
  // Function: pushAway
  // Pushes all other items away so none overlap this Item.
  //
  // Parameters:
  //  immediately - boolean for doing the pushAway without animation
  pushAway: function Item_pushAway(immediately) {
    var buffer = Math.floor(Items.defaultGutter / 2);

    var items = Items.getTopLevelItems();
    // setup each Item's pushAwayData attribute:
    items.forEach(function pushAway_setupPushAwayData(item) {
      var data = {};
      data.bounds = item.getBounds();
      data.startBounds = new Rect(data.bounds);
      // Infinity = (as yet) unaffected
      data.generation = Infinity;
      item.pushAwayData = data;
    });

    // The first item is a 0-generation pushed item. It all starts here.
    var itemsToPush = [this];
    this.pushAwayData.generation = 0;

    var pushOne = function Item_pushAway_pushOne(baseItem) {
      // the baseItem is an n-generation pushed item. (n could be 0)
      var baseData = baseItem.pushAwayData;
      var bb = new Rect(baseData.bounds);

      // make the bounds larger, adding a +buffer margin to each side.
      bb.inset(-buffer, -buffer);
      // bbc = center of the base's bounds
      var bbc = bb.center();

      items.forEach(function Item_pushAway_pushOne_pushEach(item) {
        if (item == baseItem || item.locked.bounds)
          return;

        var data = item.pushAwayData;
        // if the item under consideration has already been pushed, or has a lower
        // "generation" (and thus an implictly greater placement priority) then don't move it.
        if (data.generation <= baseData.generation)
          return;

        // box = this item's current bounds, with a +buffer margin.
        var bounds = data.bounds;
        var box = new Rect(bounds);
        box.inset(-buffer, -buffer);

        // if the item under consideration overlaps with the base item...
        if (box.intersects(bb)) {

          // Let's push it a little.

          // First, decide in which direction and how far to push. This is the offset.
          var offset = new Point();
          // center = the current item's center.
          var center = box.center();

          // Consider the relationship between the current item (box) + the base item.
          // If it's more vertically stacked than "side by side"...
          if (Math.abs(center.x - bbc.x) < Math.abs(center.y - bbc.y)) {
            // push vertically.
            if (center.y > bbc.y)
              offset.y = bb.bottom - box.top;
            else
              offset.y = bb.top - box.bottom;
          } else { // if they're more "side by side" than stacked vertically...
            // push horizontally.
            if (center.x > bbc.x)
              offset.x = bb.right - box.left;
            else
              offset.x = bb.left - box.right;
          }

          // Actually push the Item.
          bounds.offset(offset);

          // This item now becomes an (n+1)-generation pushed item.
          data.generation = baseData.generation + 1;
          // keep track of who pushed this item.
          data.pusher = baseItem;
          // add this item to the queue, so that it, in turn, can push some other things.
          itemsToPush.push(item);
        }
      });
    };

    // push each of the itemsToPush, one at a time.
    // itemsToPush starts with just [this], but pushOne can add more items to the stack.
    // Maximally, this could run through all Items on the screen.
    while (itemsToPush.length)
      pushOne(itemsToPush.shift());

    // ___ Squish!
    var pageBounds = Items.getSafeWindowBounds();
    items.forEach(function Item_pushAway_squish(item) {
      var data = item.pushAwayData;
      if (data.generation == 0 || item.locked.bounds)
        return;

      let apply = function Item_pushAway_squish_apply(item, posStep, posStep2, sizeStep) {
        var data = item.pushAwayData;
        if (data.generation == 0)
          return;

        var bounds = data.bounds;
        bounds.width -= sizeStep.x;
        bounds.height -= sizeStep.y;
        bounds.left += posStep.x;
        bounds.top += posStep.y;

        let validSize;
        if (item.isAGroupItem) {
          validSize = GroupItems.calcValidSize(
            new Point(bounds.width, bounds.height));
          bounds.width = validSize.x;
          bounds.height = validSize.y;
        } else {
          if (sizeStep.y > sizeStep.x) {
            validSize = TabItems.calcValidSize(new Point(-1, bounds.height));
            bounds.left += (bounds.width - validSize.x) / 2;
            bounds.width = validSize.x;
          } else {
            validSize = TabItems.calcValidSize(new Point(bounds.width, -1));
            bounds.top += (bounds.height - validSize.y) / 2;
            bounds.height = validSize.y;        
          }
        }

        var pusher = data.pusher;
        if (pusher) {
          var newPosStep = new Point(posStep.x + posStep2.x, posStep.y + posStep2.y);
          apply(pusher, newPosStep, posStep2, sizeStep);
        }
      }

      var bounds = data.bounds;
      var posStep = new Point();
      var posStep2 = new Point();
      var sizeStep = new Point();

      if (bounds.left < pageBounds.left) {
        posStep.x = pageBounds.left - bounds.left;
        sizeStep.x = posStep.x / data.generation;
        posStep2.x = -sizeStep.x;
      } else if (bounds.right > pageBounds.right) { // this may be less of a problem post-601534
        posStep.x = pageBounds.right - bounds.right;
        sizeStep.x = -posStep.x / data.generation;
        posStep.x += sizeStep.x;
        posStep2.x = sizeStep.x;
      }

      if (bounds.top < pageBounds.top) {
        posStep.y = pageBounds.top - bounds.top;
        sizeStep.y = posStep.y / data.generation;
        posStep2.y = -sizeStep.y;
      } else if (bounds.bottom > pageBounds.bottom) { // this may be less of a problem post-601534
        posStep.y = pageBounds.bottom - bounds.bottom;
        sizeStep.y = -posStep.y / data.generation;
        posStep.y += sizeStep.y;
        posStep2.y = sizeStep.y;
      }

      if (posStep.x || posStep.y || sizeStep.x || sizeStep.y)
        apply(item, posStep, posStep2, sizeStep);        
    });

    // ___ Unsquish
    var pairs = [];
    items.forEach(function Item_pushAway_setupUnsquish(item) {
      var data = item.pushAwayData;
      pairs.push({
        item: item,
        bounds: data.bounds
      });
    });

    Items.unsquish(pairs);

    // ___ Apply changes
    items.forEach(function Item_pushAway_setBounds(item) {
      var data = item.pushAwayData;
      var bounds = data.bounds;
      if (!bounds.equals(data.startBounds)) {
        item.setBounds(bounds, immediately);
      }
    });
  },

  // ----------
  // Function: _updateDebugBounds
  // Called by a subclass when its bounds change, to update the debugging rectangles on screen.
  // This functionality is enabled only by the debug property.
  _updateDebugBounds: function Item__updateDebugBounds() {
    if (this.$debug) {
      this.$debug.css(this.bounds);
    }
  },

  // ----------
  // Function: setTrenches
  // Sets up/moves the trenches for snapping to this item.
  setTrenches: function Item_setTrenches(rect) {
    if (this.parent !== null)
      return;

    if (!this.borderTrenches)
      this.borderTrenches = Trenches.registerWithItem(this,"border");

    var bT = this.borderTrenches;
    Trenches.getById(bT.left).setWithRect(rect);
    Trenches.getById(bT.right).setWithRect(rect);
    Trenches.getById(bT.top).setWithRect(rect);
    Trenches.getById(bT.bottom).setWithRect(rect);

    if (!this.guideTrenches)
      this.guideTrenches = Trenches.registerWithItem(this,"guide");

    var gT = this.guideTrenches;
    Trenches.getById(gT.left).setWithRect(rect);
    Trenches.getById(gT.right).setWithRect(rect);
    Trenches.getById(gT.top).setWithRect(rect);
    Trenches.getById(gT.bottom).setWithRect(rect);

  },

  // ----------
  // Function: removeTrenches
  // Removes the trenches for snapping to this item.
  removeTrenches: function Item_removeTrenches() {
    for (var edge in this.borderTrenches) {
      Trenches.unregister(this.borderTrenches[edge]); // unregister can take an array
    }
    this.borderTrenches = null;
    for (var edge in this.guideTrenches) {
      Trenches.unregister(this.guideTrenches[edge]); // unregister can take an array
    }
    this.guideTrenches = null;
  },

  // ----------
  // Function: snap
  // The snap function used during groupItem creation via drag-out
  //
  // Parameters:
  //  immediately - bool for having the drag do the final positioning without animation
  snap: function Item_snap(immediately) {
    // make the snapping work with a wider range!
    var defaultRadius = Trenches.defaultRadius;
    Trenches.defaultRadius = 2 * defaultRadius; // bump up from 10 to 20!

    var event = {startPosition:{}}; // faux event
    var FauxDragInfo = new Drag(this, event, true);
    // true == isFauxDrag
    FauxDragInfo.snap('none', false);
    FauxDragInfo.stop(immediately);

    Trenches.defaultRadius = defaultRadius;
  },

  // ----------
  // Function: draggable
  // Enables dragging on this item. Note: not to be called multiple times on the same item!
  draggable: function Item_draggable() {
    try {
      Utils.assert(this.dragOptions, 'dragOptions');

      var cancelClasses = [];
      if (typeof this.dragOptions.cancelClass == 'string')
        cancelClasses = this.dragOptions.cancelClass.split(' ');

      var self = this;
      var $container = iQ(this.container);
      var startMouse;
      var startPos;
      var startSent;
      var startEvent;
      var droppables;
      var dropTarget;

      // ___ mousemove
      var handleMouseMove = function(e) {
        // global drag tracking
        drag.lastMoveTime = Date.now();

        // positioning
        var mouse = new Point(e.pageX, e.pageY);
        if (!startSent) {
          if(Math.abs(mouse.x - startMouse.x) > self.dragOptions.minDragDistance ||
             Math.abs(mouse.y - startMouse.y) > self.dragOptions.minDragDistance) {
            if (typeof self.dragOptions.start == "function")
              self.dragOptions.start.apply(self,
                  [startEvent, {position: {left: startPos.x, top: startPos.y}}]);
            startSent = true;
          }
        }
        if (startSent) {
          // drag events
          var box = self.getBounds();
          box.left = startPos.x + (mouse.x - startMouse.x);
          box.top = startPos.y + (mouse.y - startMouse.y);
          self.setBounds(box, true);

          if (typeof self.dragOptions.drag == "function")
            self.dragOptions.drag.apply(self, [e]);

          // drop events
          var best = {
            dropTarget: null,
            score: 0
          };

          droppables.forEach(function(droppable) {
            var intersection = box.intersection(droppable.bounds);
            if (intersection && intersection.area() > best.score) {
              var possibleDropTarget = droppable.item;
              var accept = true;
              if (possibleDropTarget != dropTarget) {
                var dropOptions = possibleDropTarget.dropOptions;
                if (dropOptions && typeof dropOptions.accept == "function")
                  accept = dropOptions.accept.apply(possibleDropTarget, [self]);
              }

              if (accept) {
                best.dropTarget = possibleDropTarget;
                best.score = intersection.area();
              }
            }
          });

          if (best.dropTarget != dropTarget) {
            var dropOptions;
            if (dropTarget) {
              dropOptions = dropTarget.dropOptions;
              if (dropOptions && typeof dropOptions.out == "function")
                dropOptions.out.apply(dropTarget, [e]);
            }

            dropTarget = best.dropTarget;

            if (dropTarget) {
              dropOptions = dropTarget.dropOptions;
              if (dropOptions && typeof dropOptions.over == "function")
                dropOptions.over.apply(dropTarget, [e]);
            }
          }
          if (dropTarget) {
            dropOptions = dropTarget.dropOptions;
            if (dropOptions && typeof dropOptions.move == "function")
              dropOptions.move.apply(dropTarget, [e]);
          }
        }

        e.preventDefault();
      };

      // ___ mouseup
      var handleMouseUp = function(e) {
        iQ(gWindow)
          .unbind('mousemove', handleMouseMove)
          .unbind('mouseup', handleMouseUp);

        if (dropTarget) {
          var dropOptions = dropTarget.dropOptions;
          if (dropOptions && typeof dropOptions.drop == "function")
            dropOptions.drop.apply(dropTarget, [e]);
        }

        if (startSent && typeof self.dragOptions.stop == "function")
          self.dragOptions.stop.apply(self, [e]);

        e.preventDefault();
      };

      // ___ mousedown
      $container.mousedown(function(e) {
        if (Utils.isRightClick(e))
          return;

        var cancel = false;
        var $target = iQ(e.target);
        cancelClasses.forEach(function(className) {
          if ($target.hasClass(className))
            cancel = true;
        });

        if (cancel) {
          e.preventDefault();
          return;
        }

        startMouse = new Point(e.pageX, e.pageY);
        startPos = self.getBounds().position();
        startEvent = e;
        startSent = false;
        dropTarget = null;

        droppables = [];
        iQ('.iq-droppable').each(function(elem) {
          if (elem != self.container) {
            var item = Items.item(elem);
            droppables.push({
              item: item,
              bounds: item.getBounds()
            });
          }
        });

        iQ(gWindow)
          .mousemove(handleMouseMove)
          .mouseup(handleMouseUp);

        e.preventDefault();
      });
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: droppable
  // Enables or disables dropping on this item.
  droppable: function Item_droppable(value) {
    try {
      var $container = iQ(this.container);
      if (value) {
        Utils.assert(this.dropOptions, 'dropOptions');
        $container.addClass('iq-droppable');
      } else
        $container.removeClass('iq-droppable');
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: resizable
  // Enables or disables resizing of this item.
  resizable: function Item_resizable(value) {
    try {
      var $container = iQ(this.container);
      iQ('.iq-resizable-handle', $container).remove();

      if (!value) {
        $container.removeClass('iq-resizable');
      } else {
        Utils.assert(this.resizeOptions, 'resizeOptions');

        $container.addClass('iq-resizable');

        var self = this;
        var startMouse;
        var startSize;
        var startAspect;

        // ___ mousemove
        var handleMouseMove = function(e) {
          // global resize tracking
          resize.lastMoveTime = Date.now();

          var mouse = new Point(e.pageX, e.pageY);
          var box = self.getBounds();
          if (UI.rtl) {
            var minWidth = (self.resizeOptions.minWidth || 0);
            var oldWidth = box.width;
            if (minWidth != oldWidth || mouse.x < startMouse.x) {
              box.width = Math.max(minWidth, startSize.x - (mouse.x - startMouse.x));
              box.left -= box.width - oldWidth;
            }
          } else {
            box.width = Math.max(self.resizeOptions.minWidth || 0, startSize.x + (mouse.x - startMouse.x));
          }
          box.height = Math.max(self.resizeOptions.minHeight || 0, startSize.y + (mouse.y - startMouse.y));

          if (self.resizeOptions.aspectRatio) {
            if (startAspect < 1)
              box.height = box.width * startAspect;
            else
              box.width = box.height / startAspect;
          }

          self.setBounds(box, true);

          if (typeof self.resizeOptions.resize == "function")
            self.resizeOptions.resize.apply(self, [e]);

          e.preventDefault();
          e.stopPropagation();
        };

        // ___ mouseup
        var handleMouseUp = function(e) {
          iQ(gWindow)
            .unbind('mousemove', handleMouseMove)
            .unbind('mouseup', handleMouseUp);

          if (typeof self.resizeOptions.stop == "function")
            self.resizeOptions.stop.apply(self, [e]);

          e.preventDefault();
          e.stopPropagation();
        };

        // ___ handle + mousedown
        iQ('<div>')
          .addClass('iq-resizable-handle iq-resizable-se')
          .appendTo($container)
          .mousedown(function(e) {
            if (Utils.isRightClick(e))
              return;

            startMouse = new Point(e.pageX, e.pageY);
            startSize = self.getBounds().size();
            startAspect = startSize.y / startSize.x;

            if (typeof self.resizeOptions.start == "function")
              self.resizeOptions.start.apply(self, [e]);

            iQ(gWindow)
              .mousemove(handleMouseMove)
              .mouseup(handleMouseUp);

            e.preventDefault();
            e.stopPropagation();
          });
        }
    } catch(e) {
      Utils.log(e);
    }
  }
};

// ##########
// Class: Items
// Keeps track of all Items.
let Items = {
  // ----------
  // Variable: defaultGutter
  // How far apart Items should be from each other and from bounds
  defaultGutter: 15,

  // ----------
  // Function: item
  // Given a DOM element representing an Item, returns the Item.
  item: function Items_item(el) {
    return iQ(el).data('item');
  },

  // ----------
  // Function: getTopLevelItems
  // Returns an array of all Items not grouped into groupItems.
  getTopLevelItems: function Items_getTopLevelItems() {
    var items = [];

    iQ('.tab, .groupItem, .info-item').each(function(elem) {
      var $this = iQ(elem);
      var item = $this.data('item');
      if (item && !item.parent && !$this.hasClass('phantom'))
        items.push(item);
    });

    return items;
  },

  // ----------
  // Function: getPageBounds
  // Returns a <Rect> defining the area of the page <Item>s should stay within.
  getPageBounds: function Items_getPageBounds() {
    var width = Math.max(100, window.innerWidth);
    var height = Math.max(100, window.innerHeight);
    return new Rect(0, 0, width, height);
  },

  // ----------
  // Function: getSafeWindowBounds
  // Returns the bounds within which it is safe to place all non-stationary <Item>s.
  getSafeWindowBounds: function Items_getSafeWindowBounds() {
    // the safe bounds that would keep it "in the window"
    var gutter = Items.defaultGutter;
    // Here, I've set the top gutter separately, as the top of the window has its own
    // extra chrome which makes a large top gutter unnecessary.
    // TODO: set top gutter separately, elsewhere.
    var topGutter = 5;
    return new Rect(gutter, topGutter,
        window.innerWidth - 2 * gutter, window.innerHeight - gutter - topGutter);

  },

  // ----------
  // Function: arrange
  // Arranges the given items in a grid within the given bounds,
  // maximizing item size but maintaining standard tab aspect ratio for each
  //
  // Parameters:
  //   items - an array of <Item>s. Can be null, in which case we won't
  //     actually move anything.
  //   bounds - a <Rect> defining the space to arrange within
  //   options - an object with various properites (see below)
  //
  // Possible "options" properties:
  //   animate - whether to animate; default: true.
  //   z - the z index to set all the items; default: don't change z.
  //   return - if set to 'widthAndColumns', it'll return an object with the
  //     width of children and the columns.
  //   count - overrides the item count for layout purposes;
  //     default: the actual item count
  //   padding - pixels between each item
  //   columns - (int) a preset number of columns to use
  //   dropPos - a <Point> which should have a one-tab space left open, used
  //             when a tab is dragged over.
  //
  // Returns:
  //   By default, an object with three properties: `rects`, the list of <Rect>s,
  //   `dropIndex`, the index which a dragged tab should have if dropped
  //   (null if no `dropPos` was specified), and the number of columns (`columns`).
  //   If the `return` option is set to 'widthAndColumns', an object with the
  //   width value of the child items (`childWidth`) and the number of columns
  //   (`columns`) is returned.
  arrange: function Items_arrange(items, bounds, options) {
    if (!options)
      options = {};
    var animate = "animate" in options ? options.animate : true;
    var immediately = !animate;

    var rects = [];

    var count = options.count || (items ? items.length : 0);
    if (options.addTab)
      count++;
    if (!count) {
      let dropIndex = (Utils.isPoint(options.dropPos)) ? 0 : null;
      return {rects: rects, dropIndex: dropIndex};
    }

    var columns = options.columns || 1;
    // We'll assume for the time being that all the items have the same styling
    // and that the margin is the same width around.
    var itemMargin = items && items.length ?
                       parseInt(iQ(items[0].container).css('margin-left')) : 0;
    var padding = itemMargin * 2;
    var rows;
    var tabWidth;
    var tabHeight;
    var totalHeight;

    function figure() {
      rows = Math.ceil(count / columns);
      let validSize = TabItems.calcValidSize(
        new Point((bounds.width - (padding * columns)) / columns, -1),
        options);
      tabWidth = validSize.x;
      tabHeight = validSize.y;

      totalHeight = (tabHeight * rows) + (padding * rows);    
    }

    figure();

    while (rows > 1 && totalHeight > bounds.height) {
      columns++;
      figure();
    }

    if (rows == 1) {
      let validSize = TabItems.calcValidSize(new Point(tabWidth,
        bounds.height - 2 * itemMargin), options);
      tabWidth = validSize.x;
      tabHeight = validSize.y;
    }
    
    if (options.return == 'widthAndColumns')
      return {childWidth: tabWidth, columns: columns};

    let initialOffset = 0;
    if (UI.rtl) {
      initialOffset = bounds.width - tabWidth - padding;
    }
    var box = new Rect(bounds.left + initialOffset, bounds.top, tabWidth, tabHeight);

    var column = 0;

    var dropIndex = false;
    var dropRect = false;
    if (Utils.isPoint(options.dropPos))
      dropRect = new Rect(options.dropPos.x, options.dropPos.y, 1, 1);
    for (let a = 0; a < count; a++) {
      // If we had a dropPos, see if this is where we should place it
      if (dropRect) {
        let activeBox = new Rect(box);
        activeBox.inset(-itemMargin - 1, -itemMargin - 1);
        // if the designated position (dropRect) is within the active box,
        // this is where, if we drop the tab being dragged, it should land!
        if (activeBox.contains(dropRect))
          dropIndex = a;
      }
      
      // record the box.
      rects.push(new Rect(box));

      box.left += (UI.rtl ? -1 : 1) * (box.width + padding);
      column++;
      if (column == columns) {
        box.left = bounds.left + initialOffset;
        box.top += box.height + padding;
        column = 0;
      }
    }

    return {rects: rects, dropIndex: dropIndex, columns: columns};
  },

  // ----------
  // Function: unsquish
  // Checks to see which items can now be unsquished.
  //
  // Parameters:
  //   pairs - an array of objects, each with two properties: item and bounds. The bounds are
  //     modified as appropriate, but the items are not changed. If pairs is null, the
  //     operation is performed directly on all of the top level items.
  //   ignore - an <Item> to not include in calculations (because it's about to be closed, for instance)
  unsquish: function Items_unsquish(pairs, ignore) {
    var pairsProvided = (pairs ? true : false);
    if (!pairsProvided) {
      var items = Items.getTopLevelItems();
      pairs = [];
      items.forEach(function(item) {
        pairs.push({
          item: item,
          bounds: item.getBounds()
        });
      });
    }

    var pageBounds = Items.getSafeWindowBounds();
    pairs.forEach(function(pair) {
      var item = pair.item;
      if (item.locked.bounds || item == ignore)
        return;

      var bounds = pair.bounds;
      var newBounds = new Rect(bounds);

      var newSize;
      if (Utils.isPoint(item.userSize))
        newSize = new Point(item.userSize);
      else if (item.isAGroupItem)
        newSize = GroupItems.calcValidSize(
          new Point(GroupItems.minGroupWidth, -1));
      else
        newSize = TabItems.calcValidSize(
          new Point(TabItems.tabWidth, -1));

      if (item.isAGroupItem) {
          newBounds.width = Math.max(newBounds.width, newSize.x);
          newBounds.height = Math.max(newBounds.height, newSize.y);
      } else {
        if (bounds.width < newSize.x) {
          newBounds.width = newSize.x;
          newBounds.height = newSize.y;
        }
      }

      newBounds.left -= (newBounds.width - bounds.width) / 2;
      newBounds.top -= (newBounds.height - bounds.height) / 2;

      var offset = new Point();
      if (newBounds.left < pageBounds.left)
        offset.x = pageBounds.left - newBounds.left;
      else if (newBounds.right > pageBounds.right)
        offset.x = pageBounds.right - newBounds.right;

      if (newBounds.top < pageBounds.top)
        offset.y = pageBounds.top - newBounds.top;
      else if (newBounds.bottom > pageBounds.bottom)
        offset.y = pageBounds.bottom - newBounds.bottom;

      newBounds.offset(offset);

      if (!bounds.equals(newBounds)) {
        var blocked = false;
        pairs.forEach(function(pair2) {
          if (pair2 == pair || pair2.item == ignore)
            return;

          var bounds2 = pair2.bounds;
          if (bounds2.intersects(newBounds))
            blocked = true;
          return;
        });

        if (!blocked) {
          pair.bounds.copy(newBounds);
        }
      }
      return;
    });

    if (!pairsProvided) {
      pairs.forEach(function(pair) {
        pair.item.setBounds(pair.bounds);
      });
    }
  }
};
