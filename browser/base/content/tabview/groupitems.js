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
 * The Original Code is groupItems.js.
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
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Raymond Lee <raymond@appcoast.com>
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
// Title: groupItems.js

// ##########
// Class: GroupItem
// A single groupItem in the TabView window. Descended from <Item>.
// Note that it implements the <Subscribable> interface.
//
// ----------
// Constructor: GroupItem
//
// Parameters:
//   listOfEls - an array of DOM elements for tabs to be added to this groupItem
//   options - various options for this groupItem (see below). In addition, gets passed
//     to <add> along with the elements provided.
//
// Possible options:
//   id - specifies the groupItem's id; otherwise automatically generated
//   locked - see <Item.locked>; default is {}
//   userSize - see <Item.userSize>; default is null
//   bounds - a <Rect>; otherwise based on the locations of the provided elements
//   container - a DOM element to use as the container for this groupItem; otherwise will create
//   title - the title for the groupItem; otherwise blank
//   dontPush - true if this groupItem shouldn't push away on creation; default is false
function GroupItem(listOfEls, options) {
  if (typeof options == 'undefined')
    options = {};

  this._inited = false;
  this._children = []; // an array of Items
  this.defaultSize = new Point(TabItems.tabWidth * 1.5, TabItems.tabHeight * 1.5);
  this.isAGroupItem = true;
  this.id = options.id || GroupItems.getNextID();
  this._isStacked = false;
  this._stackAngles = [0];
  this.expanded = null;
  this.locked = (options.locked ? Utils.copy(options.locked) : {});
  this.topChild = null;
  this.hidden = false;

  this.keepProportional = false;

  // Variable: _activeTab
  // The <TabItem> for the groupItem's active tab.
  this._activeTab = null;

  // Variables: xDensity, yDensity
  // "density" ranges from 0 to 1, with 0 being "not dense" = "squishable" and 1 being "dense"
  // = "not squishable". For example, if there is extra space in the vertical direction,
  // yDensity will be < 1. These are set by <GroupItem.arrange>, as it is dependent on the tab items
  // inside the groupItem.
  this.xDensity = 0;
  this.yDensity = 0;

  if (Utils.isPoint(options.userSize))
    this.userSize = new Point(options.userSize);

  var self = this;

  var rectToBe;
  if (options.bounds) {
    Utils.assert(Utils.isRect(options.bounds), "options.bounds must be a Rect");
    rectToBe = new Rect(options.bounds);
  }

  if (!rectToBe) {
    rectToBe = GroupItems.getBoundingBox(listOfEls);
    rectToBe.inset(-30, -30);
  }

  var $container = options.container;
  if (!$container) {
    $container = iQ('<div>')
      .addClass('groupItem')
      .css({position: 'absolute'})
      .css(rectToBe);
  }

  this.bounds = $container.bounds();

  this.isDragging = false;
  $container
    .css({zIndex: -100})
    .appendTo("body");

  // ___ New Tab Button
  this.$ntb = iQ("<div>")
    .addClass('newTabButton')
    .click(function() {
      self.newTab();
    })
    .attr('title', tabviewString('groupItem.newTabButton'))
    .appendTo($container);

  // ___ Resizer
  this.$resizer = iQ("<div>")
    .addClass('resizer')
    .appendTo($container)
    .hide();

  // ___ Titlebar
  var html =
    "<div class='title-container'>" +
      "<input class='name'/>" +
      "<div class='title-shield' />" +
    "</div>";

  this.$titlebar = iQ('<div>')
    .addClass('titlebar')
    .html(html)
    .appendTo($container);

  var $close = iQ('<div>')
    .addClass('close')
    .click(function() {
      self.closeAll();
    })
    .appendTo($container);

  // ___ Title
  this.$titleContainer = iQ('.title-container', this.$titlebar);
  this.$title = iQ('.name', this.$titlebar);
  this.$titleShield = iQ('.title-shield', this.$titlebar);
  this.setTitle(options.title || this.defaultName);

  var titleUnfocus = function() {
    self.$titleShield.show();
    if (!self.getTitle()) {
      self.$title
        .addClass("defaultName")
        .val(self.defaultName);
    } else {
      self.$title
        .css({"background":"none"})
        .animate({
          "padding-left": "1px"
        }, {
          duration: 200,
          easing: "tabviewBounce"
        });
    }
  };

  var handleKeyDown = function(e) {
    if (e.which == 13 || e.which == 27) { // return & escape
      (self.$title)[0].blur();
      self.$title
        .addClass("transparentBorder")
        .one("mouseout", function() {
          self.$title.removeClass("transparentBorder");
        });
      e.stopPropagation();
      e.preventDefault();
    }
  };

  var handleKeyUp = function(e) {
    // NOTE: When user commits or cancels IME composition, the last key
    //       event fires only a keyup event.  Then, we shouldn't take any
    //       reactions but we should update our status.
    self.adjustTitleSize();
    self.save();
  };

  this.$title
    .css({backgroundRepeat: 'no-repeat'})
    .blur(titleUnfocus)
    .focus(function() {
      if (self.locked.title) {
        (self.$title)[0].blur();
        return;
      }
      (self.$title)[0].select();
      if (!self.getTitle()) {
        self.$title
          .removeClass("defaultName")
          .val('');
      }
    })
    .keydown(handleKeyDown)
    .keyup(handleKeyUp);

  titleUnfocus();

  if (this.locked.title)
    this.$title.addClass('name-locked');
  else {
    this.$titleShield
      .mousedown(function(e) {
        self.lastMouseDownTarget = (Utils.isRightClick(e) ? null : e.target);
      })
      .mouseup(function(e) {
        var same = (e.target == self.lastMouseDownTarget);
        self.lastMouseDownTarget = null;
        if (!same)
          return;

        if (!self.isDragging) {
          self.$titleShield.hide();
          (self.$title)[0].focus();
        }
      });
  }

  // ___ Stack Expander
  this.$expander = iQ("<div/>")
    .addClass("stackExpander")
    .appendTo($container)
    .hide();

  // ___ app tabs: create app tab tray and populate it
  this.$appTabTray = iQ("<div/>")
    .addClass("appTabTray")
    .appendTo($container);

  AllTabs.tabs.forEach(function(xulTab) {
    if (xulTab.pinned && xulTab.ownerDocument.defaultView == gWindow)
      self.addAppTab(xulTab);
  });

  // ___ locking
  if (this.locked.bounds)
    $container.css({cursor: 'default'});

  if (this.locked.close)
    $close.hide();

  // ___ Undo Close
  this.$undoContainer = null;

  // ___ Superclass initialization
  this._init($container[0]);

  if (this.$debug)
    this.$debug.css({zIndex: -1000});

  // ___ Children
  Array.prototype.forEach.call(listOfEls, function(el) {
    self.add(el, null, options);
  });

  // ___ Finish Up
  this._addHandlers($container);

  if (!this.locked.bounds)
    this.setResizable(true);

  GroupItems.register(this);

  // ___ Position
  var immediately = $container ? true : false;
  this.setBounds(rectToBe, immediately);
  this.snap();
  if ($container)
    this.setBounds(rectToBe, immediately);

  // ___ Push other objects away
  if (!options.dontPush)
    this.pushAway();

  this._inited = true;
  this.save();
};

// ----------
GroupItem.prototype = Utils.extend(new Item(), new Subscribable(), {
  // ----------
  // Variable: defaultName
  // The prompt text for the title field.
  defaultName: tabviewString('groupItem.defaultName'),

  // -----------
  // Function: setActiveTab
  // Sets the active <TabItem> for this groupItem; can be null, but only
  // if there are no children.
  setActiveTab: function GroupItem_setActiveTab(tab) {
    Utils.assertThrow((!tab && this._children.length == 0) || tab.isATabItem,
        "tab must be null (if no children) or a TabItem");

    this._activeTab = tab;
  },

  // -----------
  // Function: getActiveTab
  // Gets the active <TabItem> for this groupItem; can be null, but only
  // if there are no children.
  getActiveTab: function GroupItem_getActiveTab() {
    return this._activeTab;
  },

  // ----------
  // Function: getStorageData
  // Returns all of the info worth storing about this groupItem.
  getStorageData: function GroupItem_getStorageData() {
    var data = {
      bounds: this.getBounds(),
      userSize: null,
      locked: Utils.copy(this.locked),
      title: this.getTitle(),
      id: this.id
    };

    if (Utils.isPoint(this.userSize))
      data.userSize = new Point(this.userSize);

    return data;
  },

  // ----------
  // Function: isEmpty
  // Returns true if the tab groupItem is empty and unnamed.
  isEmpty: function GroupItem_isEmpty() {
    return !this._children.length && !this.getTitle();
  },

  // ----------
  // Function: save
  // Saves this groupItem to persistent storage.
  save: function GroupItem_save() {
    if (!this._inited) // too soon to save now
      return;

    var data = this.getStorageData();
    if (GroupItems.groupItemStorageSanity(data))
      Storage.saveGroupItem(gWindow, data);
  },

  // ----------
  // Function: getTitle
  // Returns the title of this groupItem as a string.
  getTitle: function GroupItem_getTitle() {
    var value = (this.$title ? this.$title.val() : '');
    return (value == this.defaultName ? '' : value);
  },

  // ----------
  // Function: setTitle
  // Sets the title of this groupItem with the given string
  setTitle: function GroupItem_setTitle(value) {
    this.$title.val(value);
    this.save();
  },

  // ----------
  // Function: adjustTitleSize
  // Used to adjust the width of the title box depending on groupItem width and title size.
  adjustTitleSize: function GroupItem_adjustTitleSize() {
    Utils.assert(this.bounds, 'bounds needs to have been set');
    let closeButton = iQ('.close', this.container);
    var w = Math.min(this.bounds.width - parseInt(closeButton.width()) - parseInt(closeButton.css('right')),
                     Math.max(150, this.getTitle().length * 6));
    // The * 6 multiplier calculation is assuming that characters in the title
    // are approximately 6 pixels wide. Bug 586545
    var css = {width: w};
    this.$title.css(css);
    this.$titleShield.css(css);
  },

  // ----------
  // Function: getContentBounds
  // Returns a <Rect> for the groupItem's content area (which doesn't include the title, etc).
  getContentBounds: function GroupItem_getContentBounds() {
    var box = this.getBounds();
    var titleHeight = this.$titlebar.height();
    box.top += titleHeight;
    box.height -= titleHeight;

    box.width -= this.$appTabTray.width();

    // Make the computed bounds' "padding" and new tab button margin actually be
    // themeable --OR-- compute this from actual bounds. Bug 586546
    box.inset(6, 6);
    box.height -= 33; // For new tab button

    return box;
  },

  // ----------
  // Function: setBounds
  // Sets the bounds with the given <Rect>, animating unless "immediately" is false.
  //
  // Parameters:
  //   rect - a <Rect> giving the new bounds
  //   immediately - true if it should not animate; default false
  //   options - an object with additional parameters, see below
  //
  // Possible options:
  //   force - true to always update the DOM even if the bounds haven't changed; default false
  setBounds: function GroupItem_setBounds(rect, immediately, options) {
    if (!Utils.isRect(rect)) {
      Utils.trace('GroupItem.setBounds: rect is not a real rectangle!', rect);
      return;
    }

    if (!options)
      options = {};

    rect.width = Math.max(110, rect.width);
    rect.height = Math.max(125, rect.height);

    var titleHeight = this.$titlebar.height();

    // ___ Determine what has changed
    var css = {};
    var titlebarCSS = {};
    var contentCSS = {};

    if (rect.left != this.bounds.left || options.force)
      css.left = rect.left;

    if (rect.top != this.bounds.top || options.force)
      css.top = rect.top;

    if (rect.width != this.bounds.width || options.force) {
      css.width = rect.width;
      titlebarCSS.width = rect.width;
      contentCSS.width = rect.width;
    }

    if (rect.height != this.bounds.height || options.force) {
      css.height = rect.height;
      contentCSS.height = rect.height - titleHeight;
    }

    if (Utils.isEmptyObject(css))
      return;

    var offset = new Point(rect.left - this.bounds.left, rect.top - this.bounds.top);
    this.bounds = new Rect(rect);

    // ___ Deal with children
    if (css.width || css.height) {
      this.arrange({animate: !immediately}); //(immediately ? 'sometimes' : true)});
    } else if (css.left || css.top) {
      this._children.forEach(function(child) {
        var box = child.getBounds();
        child.setPosition(box.left + offset.x, box.top + offset.y, immediately);
      });
    }

    // ___ Update our representation
    if (immediately) {
      iQ(this.container).css(css);
      this.$titlebar.css(titlebarCSS);
    } else {
      TabItems.pausePainting();
      iQ(this.container).animate(css, {
        duration: 350,
        easing: "tabviewBounce",
        complete: function() {
          TabItems.resumePainting();
        }
      });

      this.$titlebar.animate(titlebarCSS, {
        duration: 350
      });
    }

    this.adjustTitleSize();

    this._updateDebugBounds();
    this.setTrenches(rect);

    this.save();
  },

  // ----------
  // Function: setZ
  // Set the Z order for the groupItem's container, as well as its children.
  setZ: function GroupItem_setZ(value) {
    this.zIndex = value;

    iQ(this.container).css({zIndex: value});

    if (this.$debug)
      this.$debug.css({zIndex: value + 1});

    var count = this._children.length;
    if (count) {
      var topZIndex = value + count + 1;
      var zIndex = topZIndex;
      var self = this;
      this._children.forEach(function(child) {
        if (child == self.topChild)
          child.setZ(topZIndex + 1);
        else {
          child.setZ(zIndex);
          zIndex--;
        }
      });
    }
  },

  // ----------
  // Function: close
  // Closes the groupItem, removing (but not closing) all of its children.
  close: function GroupItem_close() {
    this.removeAll();
    GroupItems.unregister(this);
    this._sendToSubscribers("close");
    this.removeTrenches();

    iQ(this.container).animate({
      opacity: 0,
      "-moz-transform": "scale(.3)",
    }, {
      duration: 170,
      complete: function() {
        iQ(this).remove();
        Items.unsquish();
      }
    });

    Storage.deleteGroupItem(gWindow, this.id);
  },

  // ----------
  // Function: closeAll
  // Closes the groupItem and all of its children.
  closeAll: function GroupItem_closeAll() {
    if (this._children.length > 0) {
      this._children.forEach(function(child) {
        iQ(child.container).hide();
      });

      iQ(this.container).animate({
         opacity: 0,
         "-moz-transform": "scale(.3)",
      }, {
        duration: 170,
        complete: function() {
          iQ(this).hide();
        }
      });

      this._createUndoButton();
    } else {
      if (!this.locked.close)
        this.close();
    }
  },

  // ----------
  // Function: _createUndoButton
  // Makes the affordance for undo a close group action
  _createUndoButton: function() {
    let self = this;
    this.$undoContainer = iQ("<div/>")
      .addClass("undo")
      .attr("type", "button")
      .text("Undo Close Group")
      .appendTo("body");
    let undoClose = iQ("<span/>")
      .addClass("close")
      .appendTo(this.$undoContainer);

    this.$undoContainer.css({
      left: this.bounds.left + this.bounds.width/2 - iQ(self.$undoContainer).width()/2,
      top:  this.bounds.top + this.bounds.height/2 - iQ(self.$undoContainer).height()/2,
      "-moz-transform": "scale(.1)",
      opacity: 0
    });
    this.hidden = true;

    setTimeout(function() {
      self.$undoContainer.animate({
        "-moz-transform": "scale(1)",
        "opacity": 1
      }, {
        easing: "tabviewBounce",
        duration: 170,
        complete: function() {
          self._sendToSubscribers("groupHidden", { groupItemId: self.id });
        }
      });
    }, 50);

    let remove = function() {
      // close all children
      let toClose = self._children.concat();
      toClose.forEach(function(child) {
        child.removeSubscriber(self, "close");
        child.close();
      });
 
      // remove all children
      self.removeAll();
      GroupItems.unregister(self);
      self._sendToSubscribers("close");
      self.removeTrenches();

      iQ(self.container).remove();
      self.$undoContainer.remove();
      self.$undoContainer = null;
      Items.unsquish();

      Storage.deleteGroupItem(gWindow, self.id);
    };

    this.$undoContainer.click(function(e) {
      // Only do this for clicks on this actual element.
      if (e.target.nodeName != self.$undoContainer[0].nodeName)
        return;

      self.$undoContainer.fadeOut(function() {
        iQ(this).remove();
        self.hidden = false;
        self.$undoContainer = null;

        iQ(self.container).show().animate({
          "-moz-transform": "scale(1)",
          "opacity": 1
        }, {
          duration: 170,
          complete: function() {
            self._children.forEach(function(child) {
              iQ(child.container).show();
            });
          }
        });

        self._sendToSubscribers("groupShown", { groupItemId: self.id });
      });
    });

    undoClose.click(function() {
      self.$undoContainer.fadeOut(remove);
    });

    // After 15 seconds, fade away.
    const WAIT = 15000;
    const FADE = 300;

    let fadeaway = function() {
      if (self.$undoContainer)
        self.$undoContainer.animate({
          color: "transparent",
          opacity: 0
        }, {
          duration: FADE,
          complete: remove
        });
    };

    let timeoutId = setTimeout(fadeaway, WAIT);
    // Cancel the fadeaway if you move the mouse over the undo
    // button, and restart the countdown once you move out of it.
    this.$undoContainer.mouseover(function() clearTimeout(timeoutId));
    this.$undoContainer.mouseout(function() {
      timeoutId = setTimeout(fadeaway, WAIT);
    });
  },

  // ----------
  // Function: add
  // Adds an item to the groupItem.
  // Parameters:
  //
  //   a - The item to add. Can be an <Item>, a DOM element or an iQ object.
  //       The latter two must refer to the container of an <Item>.
  //   dropPos - An object with left and top properties referring to the location dropped at.  Optional.
  //   options - An object with optional settings for this call. Currently the only one is dontArrange.
  add: function GroupItem_add(a, dropPos, options) {
    try {
      var item;
      var $el;
      if (a.isAnItem) {
        item = a;
        $el = iQ(a.container);
      } else {
        $el = iQ(a);
        item = Items.item($el);
      }
      Utils.assertThrow(!item.parent || item.parent == this,
          "shouldn't already be in another groupItem");

      item.removeTrenches();

      if (typeof options == 'undefined')
        options = {};

      var self = this;

      var wasAlreadyInThisGroupItem = false;
      var oldIndex = this._children.indexOf(item);
      if (oldIndex != -1) {
        this._children.splice(oldIndex, 1);
        wasAlreadyInThisGroupItem = true;
      }

      // TODO: You should be allowed to drop in the white space at the bottom
      // and have it go to the end (right now it can match the thumbnail above
      // it and go there)
      // Bug 586548
      function findInsertionPoint(dropPos) {
        if (self.shouldStack(self._children.length + 1))
          return 0;

        var best = {dist: Infinity, item: null};
        var index = 0;
        var box;
        self._children.forEach(function(child) {
          box = child.getBounds();
          if (box.bottom < dropPos.top || box.top > dropPos.top)
            return;

          var dist = Math.sqrt(Math.pow((box.top+box.height/2)-dropPos.top,2)
              + Math.pow((box.left+box.width/2)-dropPos.left,2));

          if (dist <= best.dist) {
            best.item = child;
            best.dist = dist;
            best.index = index;
          }
        });

        if (self._children.length) {
          if (best.item) {
            box = best.item.getBounds();
            var insertLeft = dropPos.left <= box.left + box.width/2;
            if (!insertLeft)
              return best.index+1;
            return best.index;
          }
          return self._children.length;
        }

        return 0;
      }

      // Insert the tab into the right position.
      var index = dropPos ? findInsertionPoint(dropPos) : this._children.length;
      this._children.splice(index, 0, item);

      item.setZ(this.getZ() + 1);
      $el.addClass("tabInGroupItem");

      if (!wasAlreadyInThisGroupItem) {
        item.droppable(false);
        item.groupItemData = {};

        item.addSubscriber(this, "close", function() {
          self.remove(item);
        });

        item.setParent(this);

        if (typeof item.setResizable == 'function')
          item.setResizable(false);

        if (item.tab == gBrowser.selectedTab)
          GroupItems.setActiveGroupItem(this);
      }

      if (!options.dontArrange) {
        this.arrange();
      }

      this._sendToSubscribers("childAdded",{ groupItemId: this.id, item: item });

      UI.setReorderTabsOnHide(this);
    } catch(e) {
      Utils.log('GroupItem.add error', e);
    }
  },

  // ----------
  // Function: remove
  // Removes an item from the groupItem.
  // Parameters:
  //
  //   a - The item to remove. Can be an <Item>, a DOM element or an iQ object.
  //       The latter two must refer to the container of an <Item>.
  //   options - An object with optional settings for this call. Currently the only one is dontArrange.
  remove: function GroupItem_remove(a, options) {
    try {
      var $el;
      var item;

      if (a.isAnItem) {
        item = a;
        $el = iQ(item.container);
      } else {
        $el = iQ(a);
        item = Items.item($el);
      }

      if (typeof options == 'undefined')
        options = {};

      var index = this._children.indexOf(item);
      if (index != -1)
        this._children.splice(index, 1);

      if (item == this._activeTab) {
        if (this._children.length)
          this._activeTab = this._children[0];
        else
          this._activeTab = null;
      }

      item.setParent(null);
      item.removeClass("tabInGroupItem");
      item.removeClass("stacked");
      item.removeClass("stack-trayed");
      item.setRotation(0);

      item.droppable(true);
      item.removeSubscriber(this, "close");

      if (typeof item.setResizable == 'function')
        item.setResizable(true);

      if (!this._children.length && !this.locked.close && !this.getTitle() && !options.dontClose) {
        this.close();
      } else if (!options.dontArrange) {
        this.arrange();
      }

      this._sendToSubscribers("childRemoved",{ groupItemId: this.id, item: item });

    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: removeAll
  // Removes all of the groupItem's children.
  removeAll: function GroupItem_removeAll() {
    var self = this;
    var toRemove = this._children.concat();
    toRemove.forEach(function(child) {
      self.remove(child, {dontArrange: true});
    });
  },

  // ----------
  // Adds the given xul:tab as an app tab in this group's apptab tray
  addAppTab: function GroupItem_addAppTab(xulTab) {
    let self = this;

    let icon = xulTab.image || Utils.defaultFaviconURL;
    let $appTab = iQ("<img>")
      .addClass("appTabIcon")
      .attr("src", icon)
      .data("xulTab", xulTab)
      .appendTo(this.$appTabTray)
      .click(function(event) {
        if (Utils.isRightClick(event))
          return;

        GroupItems.setActiveGroupItem(self);
        GroupItems._updateTabBar();
        UI.goToTab(iQ(this).data("xulTab"));
      });

    let columnWidth = $appTab.width();
    if (parseInt(this.$appTabTray.css("width")) != columnWidth) {
      this.$appTabTray.css({width: columnWidth});
      this.arrange();
    }
  },

  // ----------
  // Function: hideExpandControl
  // Hide the control which expands a stacked groupItem into a quick-look view.
  hideExpandControl: function GroupItem_hideExpandControl() {
    this.$expander.hide();
  },

  // ----------
  // Function: showExpandControl
  // Show the control which expands a stacked groupItem into a quick-look view.
  showExpandControl: function GroupItem_showExpandControl() {
    var childBB = this.getChild(0).getBounds();
    var dT = childBB.top - this.getBounds().top;
    var dL = childBB.left - this.getBounds().left;

    this.$expander
        .show()
        .css({
          opacity: .2,
          top: dT + childBB.height + Math.min(7, (this.getBounds().bottom-childBB.bottom)/2),
          // TODO: Why the magic -6? because the childBB.width seems to be over-sizing itself.
          // But who can blame an object for being a bit optimistic when self-reporting size.
          // It has to impress the ladies somehow. Bug 586549
          left: dL + childBB.width/2 - this.$expander.width()/2 - 6,
        });
  },

  // ----------
  // Function: shouldStack
  // Returns true if the groupItem, given "count", should stack (instead of grid).
  shouldStack: function GroupItem_shouldStack(count) {
    if (count <= 1)
      return false;

    var bb = this.getContentBounds();
    var options = {
      pretend: true,
      count: count
    };

    var rects = Items.arrange(null, bb, options);
    return (rects[0].width < TabItems.minTabWidth * 1.35);
  },

  // ----------
  // Function: arrange
  // Lays out all of the children.
  //
  // Parameters:
  //   options - passed to <Items.arrange> or <_stackArrange>
  arrange: function GroupItem_arrange(options) {
    if (this.expanded) {
      this.topChild = null;
      var box = new Rect(this.expanded.bounds);
      box.inset(8, 8);
      Items.arrange(this._children, box, Utils.extend({}, options, {z: 99999}));
    } else {
      var bb = this.getContentBounds();
      var count = this._children.length;
      if (!this.shouldStack(count)) {
        var animate;
        if (!options || typeof options.animate == 'undefined')
          animate = true;
        else
          animate = options.animate;

        if (typeof options == 'undefined')
          options = {};

        this._children.forEach(function(child) {
            child.removeClass("stacked")
        });

        this.topChild = null;

        var arrangeOptions = Utils.copy(options);
        Utils.extend(arrangeOptions, {
          pretend: true,
          count: count
        });

        if (!count) {
          this.xDensity = 0;
          this.yDensity = 0;
          return;
        }

        var rects = Items.arrange(this._children, bb, arrangeOptions);

        // yDensity = (the distance of the bottom of the last tab to the top of the content area)
        // / (the total available content height)
        this.yDensity = (rects[rects.length - 1].bottom - bb.top) / (bb.height);

        // xDensity = (the distance from the left of the content area to the right of the rightmost
        // tab) / (the total available content width)

        // first, find the right of the rightmost tab! luckily, they're in order.
        // TODO: does this change for rtl?
        var rightMostRight = 0;
        for each (var rect in rects) {
          if (rect.right > rightMostRight)
            rightMostRight = rect.right;
          else
            break;
        }
        this.xDensity = (rightMostRight - bb.left) / (bb.width);

        this._children.forEach(function(child, index) {
          if (!child.locked.bounds) {
            child.setBounds(rects[index], !animate);
            child.setRotation(0);
            if (options.z)
              child.setZ(options.z);
          }
        });

        this._isStacked = false;
      } else
        this._stackArrange(bb, options);
    }

    if (this._isStacked && !this.expanded) this.showExpandControl();
    else this.hideExpandControl();
  },

  // ----------
  // Function: _stackArrange
  // Arranges the children in a stack.
  //
  // Parameters:
  //   bb - <Rect> to arrange within
  //   options - see below
  //
  // Possible "options" properties:
  //   animate - whether to animate; default: true.
  _stackArrange: function GroupItem__stackArrange(bb, options) {
    var animate;
    if (!options || typeof options.animate == 'undefined')
      animate = true;
    else
      animate = options.animate;

    if (typeof options == 'undefined')
      options = {};

    var count = this._children.length;
    if (!count)
      return;

    var zIndex = this.getZ() + count + 1;

    var maxRotation = 35; // degress
    var scale = 0.8;
    var newTabsPad = 10;
    var w;
    var h;
    var itemAspect = TabItems.tabHeight / TabItems.tabWidth;
    var bbAspect = bb.height / bb.width;

    // compute h and w. h and w are the dimensions of each of the tabs... in other words, the
    // height and width of the entire stack, modulo rotation.
    if (bbAspect > itemAspect) { // Tall, thin groupItem
      w = bb.width * scale;
      h = w * itemAspect;
      // let's say one, because, even though there's more space, we're enforcing that with scale.
      this.xDensity = 1;
      this.yDensity = h / (bb.height * scale);
    } else { // Short, wide groupItem
      h = bb.height * scale;
      w = h * (1 / itemAspect);
      this.yDensity = 1;
      this.xDensity = h / (bb.width * scale);
    }

    // x is the left margin that the stack will have, within the content area (bb)
    // y is the vertical margin
    var x = (bb.width - w) / 2;

    var y = Math.min(x, (bb.height - h) / 2);
    var box = new Rect(bb.left + x, bb.top + y, w, h);

    var self = this;
    var children = [];
    this._children.forEach(function(child) {
      if (child == self.topChild)
        children.unshift(child);
      else
        children.push(child);
    });

    children.forEach(function(child, index) {
      if (!child.locked.bounds) {
        child.setZ(zIndex);
        zIndex--;

        child.addClass("stacked");
        child.setBounds(box, !animate);
        child.setRotation(self._randRotate(maxRotation, index));
      }
    });

    self._isStacked = true;
  },

  // ----------
  // Function: _randRotate
  // Random rotation generator for <_stackArrange>
  _randRotate: function GroupItem__randRotate(spread, index) {
    if (index >= this._stackAngles.length) {
      var randAngle = 5*index + parseInt((Math.random()-.5)*1);
      this._stackAngles.push(randAngle);
      return randAngle;
    }

    if (index > 5) index = 5;

    return this._stackAngles[index];
  },

  // ----------
  // Function: childHit
  // Called by one of the groupItem's children when the child is clicked on.
  //
  // Returns an object:
  //   shouldZoom - true if the browser should launch into the tab represented by the child
  //   callback - called after the zoom animation is complete
  childHit: function GroupItem_childHit(child) {
    var self = this;

    // ___ normal click
    if (!this._isStacked || this.expanded) {
      return {
        shouldZoom: true,
        callback: function() {
          self.collapse();
        }
      };
    }

    GroupItems.setActiveGroupItem(self);
    return { shouldZoom: true };
  },

  expand: function GroupItem_expand() {
    var self = this;
    // ___ we're stacked, and command is held down so expand
    GroupItems.setActiveGroupItem(self);
    var startBounds = this.getChild(0).getBounds();
    var $tray = iQ("<div>").css({
      top: startBounds.top,
      left: startBounds.left,
      width: startBounds.width,
      height: startBounds.height,
      position: "absolute",
      zIndex: 99998
    }).appendTo("body");


    var w = 180;
    var h = w * (TabItems.tabHeight / TabItems.tabWidth) * 1.1;
    var padding = 20;
    var col = Math.ceil(Math.sqrt(this._children.length));
    var row = Math.ceil(this._children.length/col);

    var overlayWidth = Math.min(window.innerWidth - (padding * 2), w*col + padding*(col+1));
    var overlayHeight = Math.min(window.innerHeight - (padding * 2), h*row + padding*(row+1));

    var pos = {left: startBounds.left, top: startBounds.top};
    pos.left -= overlayWidth / 3;
    pos.top  -= overlayHeight / 3;

    if (pos.top < 0)
      pos.top = 20;
    if (pos.left < 0)
      pos.left = 20;
    if (pos.top + overlayHeight > window.innerHeight)
      pos.top = window.innerHeight - overlayHeight - 20;
    if (pos.left + overlayWidth > window.innerWidth)
      pos.left = window.innerWidth - overlayWidth - 20;

    $tray
      .animate({
        width:  overlayWidth,
        height: overlayHeight,
        top: pos.top,
        left: pos.left
      }, {
        duration: 200,
        easing: "tabviewBounce"
      })
      .addClass("overlay");

    this._children.forEach(function(child) {
      child.addClass("stack-trayed");
    });

    var $shield = iQ('<div>')
      .addClass('shield')
      .css({
        zIndex: 99997
      })
      .appendTo('body')
      .click(function() { // just in case
        self.collapse();
      });

    // There is a race-condition here. If there is
    // a mouse-move while the shield is coming up
    // it will collapse, which we don't want. Thus,
    // we wait a little bit before adding this event
    // handler.
    setTimeout(function() {
      $shield.mouseover(function() {
        self.collapse();
      });
    }, 200);

    this.expanded = {
      $tray: $tray,
      $shield: $shield,
      bounds: new Rect(pos.left, pos.top, overlayWidth, overlayHeight)
    };

    this.arrange();
  },

  // ----------
  // Function: collapse
  // Collapses the groupItem from the expanded "tray" mode.
  collapse: function GroupItem_collapse() {
    if (this.expanded) {
      var z = this.getZ();
      var box = this.getBounds();
      this.expanded.$tray
        .css({
          zIndex: z + 1
        })
        .animate({
          width:  box.width,
          height: box.height,
          top: box.top,
          left: box.left,
          opacity: 0
        }, {
          duration: 350,
          easing: "tabviewBounce",
          complete: function() {
            iQ(this).remove();
          }
        });

      this.expanded.$shield.remove();
      this.expanded = null;

      this._children.forEach(function(child) {
        child.removeClass("stack-trayed");
      });

      this.arrange({z: z + 2});
    }
  },

  // ----------
  // Function: _addHandlers
  // Helper routine for the constructor; adds various event handlers to the container.
  _addHandlers: function GroupItem__addHandlers(container) {
    var self = this;

    this.dropOptions.over = function() {
      iQ(this.container).addClass("acceptsDrop");
    };
    this.dropOptions.drop = function(event) {
      iQ(this.container).removeClass("acceptsDrop");
      this.add(drag.info.$el, {left:event.pageX, top:event.pageY});
      GroupItems.setActiveGroupItem(this);
    };

    if (!this.locked.bounds)
      this.draggable();

    iQ(container)
      .mousedown(function(e) {
        self._mouseDown = {
          location: new Point(e.clientX, e.clientY),
          className: e.target.className
        };
      })
      .mouseup(function(e) {
        if (!self._mouseDown || !self._mouseDown.location || !self._mouseDown.className)
          return;

        // Don't zoom in on clicks inside of the controls.
        var className = self._mouseDown.className;
        if (className.indexOf('title-shield') != -1 ||
           className.indexOf('name') != -1 ||
           className.indexOf('close') != -1 ||
           className.indexOf('newTabButton') != -1 ||
           className.indexOf('appTabTray') != -1 ||
           className.indexOf('appTabIcon') != -1 ||
           className.indexOf('stackExpander') != -1) {
          return;
        }

        var location = new Point(e.clientX, e.clientY);

        if (location.distance(self._mouseDown.location) > 1.0)
          return;

        // Zoom into the last-active tab when the groupItem
        // is clicked, but only for non-stacked groupItems.
        var activeTab = self.getActiveTab();
        if (!self._isStacked) {
          if (activeTab)
            activeTab.zoomIn();
          else if (self.getChild(0))
            self.getChild(0).zoomIn();
        }

        self._mouseDown = null;
    });

    this.droppable(true);

    this.$expander.click(function() {
      self.expand();
    });
  },

  // ----------
  // Function: setResizable
  // Sets whether the groupItem is resizable and updates the UI accordingly.
  setResizable: function GroupItem_setResizable(value) {
    this.resizeOptions.minWidth = 90;
    this.resizeOptions.minHeight = 90;

    if (value) {
      this.$resizer.fadeIn();
      this.resizable(true);
    } else {
      this.$resizer.fadeOut();
      this.resizable(false);
    }
  },

  // ----------
  // Function: newTab
  // Creates a new tab within this groupItem.
  newTab: function GroupItem_newTab(url) {
    GroupItems.setActiveGroupItem(this);
    let newTab = gBrowser.loadOneTab(url || "about:blank", {inBackground: true});

    // TabItems will have handled the new tab and added the tabItem property.
    // We don't have to check if it's an app tab (and therefore wouldn't have a
    // TabItem), since we've just created it.
    let newItem = newTab.tabItem;

    var self = this;
    iQ(newItem.container).css({opacity: 0});
    let $anim = iQ("<div>")
      .addClass("newTabAnimatee")
      .css({
        top: newItem.bounds.top + 5,
        left: newItem.bounds.left + 5,
        width: newItem.bounds.width - 10,
        height: newItem.bounds.height - 10,
        zIndex: 999,
        opacity: 0
      })
      .appendTo("body")
      .animate({opacity: 1}, {
        duration: 500,
        complete: function() {
          $anim.animate({
            top: 0,
            left: 0,
            width: window.innerWidth,
            height: window.innerHeight
          }, {
            duration: 270,
            complete: function() {
              iQ(newItem.container).css({opacity: 1});
              newItem.zoomIn(!url);
              $anim.remove();
            }
          });
        }
      });
  },

  // ----------
  // Function: reorderTabItemsBasedOnTabOrder
  // Reorders the tabs in a groupItem based on the arrangment of the tabs
  // shown in the tab bar. It does it by sorting the children
  // of the groupItem by the positions of their respective tabs in the
  // tab bar.
  reorderTabItemsBasedOnTabOrder: function GroupItem_reorderTabItemsBasedOnTabOrder() {
    this._children.sort(function(a,b) a.tab._tPos - b.tab._tPos);

    this.arrange({animate: false});
    // this.arrange calls this.save for us
  },

  // Function: reorderTabsBasedOnTabItemOrder
  // Reorders the tabs in the tab bar based on the arrangment of the tabs
  // shown in the groupItem.
  reorderTabsBasedOnTabItemOrder: function GroupItem_reorderTabsBasedOnTabItemOrder() {
    var tabBarTabs = Array.slice(gBrowser.tabs);
    var currentIndex;

    // ToDo: optimisation is needed to further reduce the tab move.
    // Bug 586553
    this._children.forEach(function(tabItem) {
      tabBarTabs.some(function(tab, i) {
        if (tabItem.tab == tab) {
          if (!currentIndex)
            currentIndex = i;
          else if (tab.pinned)
            currentIndex++;
          else {
            var removed;
            if (currentIndex < i)
              currentIndex = i;
            else if (currentIndex > i) {
              removed = tabBarTabs.splice(i, 1);
              tabBarTabs.splice(currentIndex, 0, removed);
              gBrowser.moveTabTo(tabItem.tab, currentIndex);
            }
          }
          return true;
        }
        return false;
      });
    });
  },

  // ----------
  // Function: setTopChild
  // Sets the <Item> that should be displayed on top when in stack mode.
  setTopChild: function GroupItem_setTopChild(topChild) {
    this.topChild = topChild;

    this.arrange({animate: false});
    // this.arrange calls this.save for us
  },

  // ----------
  // Function: getChild
  // Returns the nth child tab or null if index is out of range.
  //
  // Parameters:
  //  index - the index of the child tab to return, use negative
  //          numbers to index from the end (-1 is the last child)
  getChild: function GroupItem_getChild(index) {
    if (index < 0)
      index = this._children.length + index;
    if (index >= this._children.length || index < 0)
      return null;
    return this._children[index];
  },

  // ----------
  // Function: getChildren
  // Returns all children.
  getChildren: function GroupItem_getChildren() {
    return this._children;
  }
});

// ##########
// Class: GroupItems
// Singelton for managing all <GroupItem>s.
let GroupItems = {
  groupItems: [],
  nextID: 1,
  _inited: false,
  _activeGroupItem: null,
  _activeOrphanTab: null,
  _cleanupFunctions: [],

  // ----------
  // Function: init
  init: function GroupItems_init() {
    let self = this;

    // setup attr modified handler, and prepare for its uninit
    function handleAttrModified(xulTab) {
      self._handleAttrModified(xulTab);
    }

    AllTabs.register("attrModified", handleAttrModified);
    this._cleanupFunctions.push(function() {
      AllTabs.unregister("attrModified", handleAttrModified);
    });
  },

  // ----------
  // Function: uninit
  uninit : function GroupItems_uninit () {
    this._cleanupFunctions.forEach(function(func) {
      func();
    });

    this._cleanupFunctions = [];

    this.groupItems = null;
  },

  // ----------
  // watch for icon changes on app tabs
  _handleAttrModified: function GroupItems__handleAttrModified(xulTab) {
    if (xulTab.ownerDocument.defaultView != gWindow || !xulTab.pinned)
      return;

    let iconUrl = xulTab.image || Utils.defaultFaviconURL;
    this.groupItems.forEach(function(groupItem) {
      iQ(".appTabIcon", groupItem.$appTabTray).each(function(icon) {
        let $icon = iQ(icon);
        if ($icon.data("xulTab") != xulTab)
          return;

        if (iconUrl != $icon.attr("src"))
          $icon.attr("src", iconUrl);
      });
    });
  },

  // ----------
  // Function: getNextID
  // Returns the next unused groupItem ID.
  getNextID: function GroupItems_getNextID() {
    var result = this.nextID;
    this.nextID++;
    this.save();
    return result;
  },

  // ----------
  // Function: getStorageData
  // Returns an object for saving GroupItems state to persistent storage.
  getStorageData: function GroupItems_getStorageData() {
    var data = {nextID: this.nextID, groupItems: []};
    this.groupItems.forEach(function(groupItem) {
      data.groupItems.push(groupItem.getStorageData());
    });

    return data;
  },

  // ----------
  // Function: saveAll
  // Saves GroupItems state, as well as the state of all of the groupItems.
  saveAll: function GroupItems_saveAll() {
    this.save();
    this.groupItems.forEach(function(groupItem) {
      groupItem.save();
    });
  },

  // ----------
  // Function: save
  // Saves GroupItems state.
  save: function GroupItems_save() {
    if (!this._inited) // too soon to save now
      return;

    Storage.saveGroupItemsData(gWindow, {nextID:this.nextID});
  },

  // ----------
  // Function: getBoundingBox
  // Given an array of DOM elements, returns a <Rect> with (roughly) the union of their locations.
  getBoundingBox: function GroupItems_getBoundingBox(els) {
    var bounds = [iQ(el).bounds() for each (el in els)];
    var left   = Math.min.apply({},[ b.left   for each (b in bounds) ]);
    var top    = Math.min.apply({},[ b.top    for each (b in bounds) ]);
    var right  = Math.max.apply({},[ b.right  for each (b in bounds) ]);
    var bottom = Math.max.apply({},[ b.bottom for each (b in bounds) ]);

    return new Rect(left, top, right-left, bottom-top);
  },

  // ----------
  // Function: reconstitute
  // Restores to stored state, creating groupItems as needed.
  // If no data, sets up blank slate (including "new tabs" groupItem).
  reconstitute: function GroupItems_reconstitute(groupItemsData, groupItemData) {
    try {
      if (groupItemsData && groupItemsData.nextID)
        this.nextID = groupItemsData.nextID;

      if (groupItemData) {
        for (var id in groupItemData) {
          var groupItem = groupItemData[id];
          if (this.groupItemStorageSanity(groupItem)) {
            var options = {
              dontPush: true
            };

            new GroupItem([], Utils.extend({}, groupItem, options));
          }
        }
      }

      this._inited = true;
      this.save(); // for nextID
    } catch(e) {
      Utils.log("error in recons: "+e);
    }
  },

  // ----------
  // Function: groupItemStorageSanity
  // Given persistent storage data for a groupItem, returns true if it appears to not be damaged.
  groupItemStorageSanity: function GroupItems_groupItemStorageSanity(groupItemData) {
    // TODO: check everything
    // Bug 586555
    var sane = true;
    if (!Utils.isRect(groupItemData.bounds)) {
      Utils.log('GroupItems.groupItemStorageSanity: bad bounds', groupItemData.bounds);
      sane = false;
    }

    return sane;
  },

  // ----------
  // Function: register
  // Adds the given <GroupItem> to the list of groupItems we're tracking.
  register: function GroupItems_register(groupItem) {
    Utils.assert(groupItem, 'groupItem');
    Utils.assert(this.groupItems.indexOf(groupItem) == -1, 'only register once per groupItem');
    this.groupItems.push(groupItem);
    UI.updateTabButton();
  },

  // ----------
  // Function: unregister
  // Removes the given <GroupItem> from the list of groupItems we're tracking.
  unregister: function GroupItems_unregister(groupItem) {
    var index = this.groupItems.indexOf(groupItem);
    if (index != -1)
      this.groupItems.splice(index, 1);

    if (groupItem == this._activeGroupItem)
      this._activeGroupItem = null;

    UI.updateTabButton();
  },

  // ----------
  // Function: groupItem
  // Given some sort of identifier, returns the appropriate groupItem.
  // Currently only supports groupItem ids.
  groupItem: function GroupItems_groupItem(a) {
    var result = null;
    this.groupItems.forEach(function(candidate) {
      if (candidate.id == a)
        result = candidate;
    });

    return result;
  },

  // ----------
  // Function: arrange
  // Arranges all of the groupItems into a grid.
  arrange: function GroupItems_arrange() {
    var bounds = Items.getPageBounds();
    bounds.bottom -= 20; // for the dev menu

    var count = this.groupItems.length - 1;
    var columns = Math.ceil(Math.sqrt(count));
    var rows = ((columns * columns) - count >= columns ? columns - 1 : columns);
    var padding = 12;
    var startX = bounds.left + padding;
    var startY = bounds.top + padding;
    var totalWidth = bounds.width - padding;
    var totalHeight = bounds.height - padding;
    var box = new Rect(startX, startY,
        (totalWidth / columns) - padding,
        (totalHeight / rows) - padding);

    var i = 0;
    this.groupItems.forEach(function(groupItem) {
      if (groupItem.locked.bounds)
        return;

      groupItem.setBounds(box, true);

      box.left += box.width + padding;
      i++;
      if (i % columns == 0) {
        box.left = startX;
        box.top += box.height + padding;
      }
    });
  },

  // ----------
  // Function: removeAll
  // Removes all tabs from all groupItems (which automatically closes all unnamed groupItems).
  removeAll: function GroupItems_removeAll() {
    var toRemove = this.groupItems.concat();
    toRemove.forEach(function(groupItem) {
      groupItem.removeAll();
    });
  },

  // ----------
  // Function: newTab
  // Given a <TabItem>, files it in the appropriate groupItem.
  newTab: function GroupItems_newTab(tabItem) {
    let activeGroupItem = this.getActiveGroupItem();
    let orphanTab = this.getActiveOrphanTab();
//    Utils.log('newTab', activeGroupItem, orphanTab);
    if (activeGroupItem) {
      activeGroupItem.add(tabItem);
    } else if (orphanTab) {
      let newGroupItemBounds = orphanTab.getBoundsWithTitle();
      newGroupItemBounds.inset(-40,-40);
      let newGroupItem = new GroupItem([orphanTab, tabItem], {bounds: newGroupItemBounds});
      newGroupItem.snap();
      this.setActiveGroupItem(newGroupItem);
    } else {
      this.positionNewTabAtBottom(tabItem);
    }
  },

  // ----------
  // Function: positionNewTabAtBottom
  // Does what it says on the tin.
  // TODO: Make more robust and improve documentation,
  // Also, this probably belongs in tabitems.js
  // Bug 586558
  positionNewTabAtBottom: function GroupItems_positionNewTabAtBottom(tabItem) {
    let windowBounds = Items.getSafeWindowBounds();

    let itemBounds = new Rect(
      windowBounds.right - TabItems.tabWidth,
      windowBounds.bottom - TabItems.tabHeight,
      TabItems.tabWidth,
      TabItems.tabHeight
    );

    tabItem.setBounds(itemBounds);
  },

  // ----------
  // Function: getActiveGroupItem
  // Returns the active groupItem. Active means its tabs are
  // shown in the tab bar when not in the TabView interface.
  getActiveGroupItem: function GroupItems_getActiveGroupItem() {
    return this._activeGroupItem;
  },

  // ----------
  // Function: setActiveGroupItem
  // Sets the active groupItem, thereby showing only the relevant tabs and
  // setting the groupItem which will receive new tabs.
  //
  // Paramaters:
  //  groupItem - the active <GroupItem> or <null> if no groupItem is active
  //          (which means we have an orphaned tab selected)
  setActiveGroupItem: function GroupItems_setActiveGroupItem(groupItem) {
    if (this._activeGroupItem)
      iQ(this._activeGroupItem.container).removeClass('activeGroupItem');

    if (groupItem !== null) {
      if (groupItem)
        iQ(groupItem.container).addClass('activeGroupItem');
      // if a groupItem is active, we surely are not in an orphaned tab.
      this.setActiveOrphanTab(null);
    }

    this._activeGroupItem = groupItem;
  },

  // ----------
  // Function: getActiveOrphanTab
  // Returns the active orphan tab, in cases when there is no active groupItem.
  getActiveOrphanTab: function GroupItems_getActiveOrphanTab() {
    return this._activeOrphanTab;
  },

  // ----------
  // Function: setActiveOrphanTab
  // In cases where an orphan tab (not in a groupItem) is active by itself,
  // this function is called and the "active orphan tab" is set.
  //
  // Paramaters:
  //  groupItem - the active <TabItem> or <null>
  setActiveOrphanTab: function GroupItems_setActiveOrphanTab(tabItem) {
    this._activeOrphanTab = tabItem;
  },

  // ----------
  // Function: _updateTabBar
  // Hides and shows tabs in the tab bar based on the active groupItem or
  // currently active orphan tabItem
  _updateTabBar: function GroupItems__updateTabBar() {
    if (!window.UI)
      return; // called too soon

    if (!this._activeGroupItem && !this._activeOrphanTab) {
      Utils.assert(false, "There must be something to show in the tab bar!");
      return;
    }

    let tabItems = this._activeGroupItem == null ?
      [this._activeOrphanTab] : this._activeGroupItem._children;
    gBrowser.showOnlyTheseTabs(tabItems.map(function(item) item.tab));
  },

  // ----------
  // Function: updateActiveGroupItemAndTabBar
  // Sets active TabItem and GroupItem, and updates tab bar appropriately.
  updateActiveGroupItemAndTabBar: function GroupItems_updateActiveGroupItemAndTabBar(tabItem) {
    Utils.assertThrow(tabItem && tabItem.isATabItem, "tabItem must be a TabItem");

    let groupItem = tabItem.parent;
    this.setActiveGroupItem(groupItem);

    if (groupItem)
      groupItem.setActiveTab(tabItem);
    else
      this.setActiveOrphanTab(tabItem);

    this._updateTabBar();
  },

  // ----------
  // Function: getOrphanedTabs
  // Returns an array of all tabs that aren't in a groupItem.
  getOrphanedTabs: function GroupItems_getOrphanedTabs() {
    var tabs = TabItems.getItems();
    tabs = tabs.filter(function(tab) {
      return tab.parent == null;
    });
    return tabs;
  },

  // ----------
  // Function: getNextGroupItemTab
  // Paramaters:
  //  reverse - the boolean indicates the direction to look for the next groupItem.
  // Returns the <tabItem>. If nothing is found, return null.
  getNextGroupItemTab: function GroupItems_getNextGroupItemTab(reverse) {
    var groupItems = Utils.copy(GroupItems.groupItems);
    var activeGroupItem = GroupItems.getActiveGroupItem();
    var activeOrphanTab = GroupItems.getActiveOrphanTab();
    var tabItem = null;

    if (reverse)
      groupItems = groupItems.reverse();

    if (!activeGroupItem) {
      if (groupItems.length > 0) {
        groupItems.some(function(groupItem) {
          if (!groupItem.hidden) {
            var child = groupItem.getChild(0);
            if (child) {
              tabItem = child;
              return true;
            }
          }
          return false;
        });
      }
    } else {
      var currentIndex;
      groupItems.some(function(groupItem, index) {
        if (!groupItem.hidden && groupItem == activeGroupItem) {
          currentIndex = index;
          return true;
        }
        return false;
      });
      var firstGroupItems = groupItems.slice(currentIndex + 1);
      firstGroupItems.some(function(groupItem) {
        if (!groupItem.hidden) {
          var child = groupItem.getChild(0);
          if (child) {
            tabItem = child;
            return true;
          }
        }
        return false;
      });
      if (!tabItem) {
        var orphanedTabs = GroupItems.getOrphanedTabs();
        if (orphanedTabs.length > 0)
          tabItem = orphanedTabs[0];
      }
      if (!tabItem) {
        var secondGroupItems = groupItems.slice(0, currentIndex);
        secondGroupItems.some(function(groupItem) {
          if (!groupItem.hidden) {
            var child = groupItem.getChild(0);
            if (child) {
              tabItem = child;
              return true;
            }
          }
          return false;
        });
      }
    }
    return tabItem;
  },

  // ----------
  // Function: moveTabToGroupItem
  // Used for the right click menu in the tab strip; moves the given tab
  // into the given group. Does nothing if the tab is an app tab.
  // Paramaters:
  //  tab - the <xul:tab>.
  //  groupItemId - the <groupItem>'s id.  If nothing, create a new <groupItem>.
  moveTabToGroupItem : function GroupItems_moveTabToGroupItem (tab, groupItemId) {
    if (tab.pinned)
      return;

    Utils.assertThrow(tab.tabItem, "tab must be linked to a TabItem");

    let shouldUpdateTabBar = false;
    let shouldShowTabView = false;
    let groupItem;

    // switch to the appropriate tab first.
    if (gBrowser.selectedTab == tab) {
      let list = gBrowser.visibleTabs;
      let listLength = list.length;

      if (listLength > 1) {
        let index = list.indexOf(tab);
        if (index == 0 || (index + 1) < listLength)
          gBrowser.selectTabAtIndex(index + 1);
        else
          gBrowser.selectTabAtIndex(index - 1);
        shouldUpdateTabBar = true;
      } else {
        shouldShowTabView = true;
      }
    } else
      shouldUpdateTabBar = true

    // remove tab item from a groupItem
    if (tab.tabItem.parent)
      tab.tabItem.parent.remove(tab.tabItem);

    // add tab item to a groupItem
    if (groupItemId) {
      groupItem = GroupItems.groupItem(groupItemId);
      groupItem.add(tab.tabItem);
      UI.setReorderTabItemsOnShow(groupItem);
    } else {
      let pageBounds = Items.getPageBounds();
      pageBounds.inset(20, 20);

      let box = new Rect(pageBounds);
      box.width = 250;
      box.height = 200;

      new GroupItem([ tab.tabItem ], { bounds: box });
    }

    if (shouldUpdateTabBar)
      this._updateTabBar();
    else if (shouldShowTabView) {
      tab.tabItem.setZoomPrep(false);
      UI.showTabView();
    }
  },

  // ----------
  // Function: killNewTabGroup
  // Removes the New Tab Group, which is now defunct. See bug 575851 and comments therein.
  killNewTabGroup: function GroupItems_killNewTabGroup() {
    // not localized as the original "New Tabs" group title was never localized
    // to begin with
    let newTabGroupTitle = "New Tabs";
    this.groupItems.forEach(function(groupItem) {
      if (groupItem.getTitle() == newTabGroupTitle && groupItem.locked.title) {
        groupItem.removeAll();
        groupItem.close();
      }
    });
  },

  // ----------
  // Function: removeHiddenGroups
  // Removes all hidden groups' data and its browser tabs.
  removeHiddenGroups: function GroupItems_removeHiddenGroups() {
    iQ(".undo").remove();
    
    // ToDo: encapsulate this in the group item. bug 594863
    this.groupItems.forEach(function(groupItem) {
      if (groupItem.hidden) {
        let toClose = groupItem._children.concat();
        toClose.forEach(function(child) {
          child.removeSubscriber(groupItem, "close");
          child.close();
        });

        Storage.deleteGroupItem(gWindow, groupItem.id);
      }
    });
  }
};
