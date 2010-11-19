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
 * The Original Code is tabitems.js.
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
// Title: tabitems.js

// ##########
// Class: TabItem
// An <Item> that represents a tab. Also implements the <Subscribable> interface.
//
// Parameters:
//   tab - a xul:tab
function TabItem(tab, options) {
  Utils.assert(tab, "tab");

  this.tab = tab;
  // register this as the tab's tabItem
  this.tab.tabItem = this;

  if (!options)
    options = {};

  // ___ set up div
  var $div = iQ('<div>')
    .addClass('tab')
    .html("<div class='thumb'>" +
          "<img class='cached-thumb' style='display:none'/><canvas/></div>" +
          "<div class='favicon'><img/></div>" +
          "<span class='tab-title'>&nbsp;</span>"
    )
    .appendTo('body');

  this.canvasSizeForced = false;
  this.isShowingCachedData = false;
  this.favEl = (iQ('.favicon>img', $div))[0];
  this.nameEl = (iQ('.tab-title', $div))[0];
  this.canvasEl = (iQ('.thumb canvas', $div))[0];
  this.cachedThumbEl = (iQ('img.cached-thumb', $div))[0];

  this.tabCanvas = new TabCanvas(this.tab, this.canvasEl);

  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  this.locked = {};
  this.isATabItem = true;
  this._zoomPrep = false;
  this.sizeExtra = new Point();
  this.keepProportional = true;

  var self = this;

  this.isDragging = false;

  this.sizeExtra.x = parseInt($div.css('padding-left'))
      + parseInt($div.css('padding-right'));

  this.sizeExtra.y = parseInt($div.css('padding-top'))
      + parseInt($div.css('padding-bottom'));

  this.bounds = $div.bounds();

  // ___ superclass setup
  this._init($div[0]);

  // ___ reconnect to data from Storage
  this._hasBeenDrawn = false;
  let reconnected = TabItems.reconnect(this);

  // ___ drag/drop
  // override dropOptions with custom tabitem methods
  // This is mostly to support the phantom groupItems.
  this.dropOptions.drop = function(e) {
    var $target = iQ(this.container);
    this.isDropTarget = false;

    var phantom = $target.data("phantomGroupItem");

    var groupItem = drag.info.item.parent;
    if (groupItem) {
      groupItem.add(drag.info.$el);
    } else {
      phantom.removeClass("phantom acceptsDrop");
      new GroupItem([$target, drag.info.$el], {container:phantom, bounds:phantom.bounds()});
    }
  };

  this.dropOptions.over = function(e) {
    var $target = iQ(this.container);
    this.isDropTarget = true;

    $target.removeClass("acceptsDrop");

    var phantomMargin = 40;

    var groupItemBounds = this.getBoundsWithTitle();
    groupItemBounds.inset(-phantomMargin, -phantomMargin);

    iQ(".phantom").remove();
    var phantom = iQ("<div>")
      .addClass("groupItem phantom acceptsDrop")
      .css({
        position: "absolute",
        zIndex: -99
      })
      .css(groupItemBounds.css())
      .hide()
      .appendTo("body");

    var defaultRadius = Trenches.defaultRadius;
    // Extend the margin so that it covers the case where the target tab item
    // is right next to a trench.
    Trenches.defaultRadius = phantomMargin + 1;
    var updatedBounds = drag.info.snapBounds(groupItemBounds,'none');
    Trenches.defaultRadius = defaultRadius;

    // Utils.log('updatedBounds:',updatedBounds);
    if (updatedBounds)
      phantom.css(updatedBounds.css());

    phantom.fadeIn();

    $target.data("phantomGroupItem", phantom);
  };

  this.dropOptions.out = function(e) {
    this.isDropTarget = false;
    var phantom = iQ(this.container).data("phantomGroupItem");
    if (phantom) {
      phantom.fadeOut(function() {
        iQ(this).remove();
      });
    }
  };

  this.draggable();

  // ___ more div setup
  $div.mousedown(function(e) {
    if (!Utils.isRightClick(e))
      self.lastMouseDownTarget = e.target;
  });

  $div.mouseup(function(e) {
    var same = (e.target == self.lastMouseDownTarget);
    self.lastMouseDownTarget = null;
    if (!same)
      return;

    if (iQ(e.target).hasClass("close"))
      self.close();
    else {
      if (!Items.item(this).isDragging)
        self.zoomIn();
    }
  });

  iQ("<div>")
    .addClass('close')
    .appendTo($div);

  iQ("<div>")
    .addClass('expander')
    .appendTo($div);

  this._updateDebugBounds();

  TabItems.register(this);

  if (!this.reconnected)
    GroupItems.newTab(this, options);

  // tabs which were not reconnected at all or were not immediately added
  // to a group get the same treatment.
  if (!this.reconnected || (reconnected && !reconnected.addedToGroup) ) {
    this.setResizable(true, options.immediately);
    this.droppable(true);
  }
};

TabItem.prototype = Utils.extend(new Item(), new Subscribable(), {
  // ----------
  // Function: forceCanvasSize
  // Repaints the thumbnail with the given resolution, and forces it
  // to stay that resolution until unforceCanvasSize is called.
  forceCanvasSize: function TabItem_forceCanvasSize(w, h) {
    this.canvasSizeForced = true;
    this.canvasEl.width = w;
    this.canvasEl.height = h;
    this.tabCanvas.paint();
  },

  // ----------
  // Function: unforceCanvasSize
  // Stops holding the thumbnail resolution; allows it to shift to the
  // size of thumbnail on screen. Note that this call does not nest, unlike
  // <TabItems.resumePainting>; if you call forceCanvasSize multiple
  // times, you just need a single unforce to clear them all.
  unforceCanvasSize: function TabItem_unforceCanvasSize() {
    this.canvasSizeForced = false;
  },

  // ----------
  // Function: showCachedData
  // Shows the cached data i.e. image and title.  Note: this method should only
  // be called at browser startup with the cached data avaliable.
  showCachedData: function TabItem_showCachedData(tabData) {
    this.isShowingCachedData = true;
    var $nameElement = iQ(this.nameEl);
    var $canvasElement = iQ(this.canvasEl);
    var $cachedThumbElement = iQ(this.cachedThumbEl);
    $cachedThumbElement.attr("src", tabData.imageData).show();
    $canvasElement.css({opacity: 0.0});
    $nameElement.text(tabData.title ? tabData.title : "");
  },

  // ----------
  // Function: hideCachedData
  // Hides the cached data i.e. image and title and show the canvas.
  hideCachedData: function TabItem_hideCachedData() {
    var $canvasElement = iQ(this.canvasEl);
    var $cachedThumbElement = iQ(this.cachedThumbEl);
    $cachedThumbElement.hide();
    $canvasElement.css({opacity: 1.0});
    this.isShowingCachedData = false;
  },

  // ----------
  // Function: getStorageData
  // Get data to be used for persistent storage of this object.
  //
  // Parameters:
  //   getImageData - true to include thumbnail pixels (and page title as well); default false
  getStorageData: function TabItem_getStorageData(getImageData) {
    return {
      bounds: this.getBounds(),
      userSize: (Utils.isPoint(this.userSize) ? new Point(this.userSize) : null),
      url: this.tab.linkedBrowser.currentURI.spec,
      groupID: (this.parent ? this.parent.id : 0),
      imageData: (getImageData && this.tabCanvas ?
                  this.tabCanvas.toImageData() : null),
      title: getImageData && this.tab.label || null
    };
  },

  // ----------
  // Function: save
  // Store persistent for this object.
  //
  // Parameters:
  //   saveImageData - true to include thumbnail pixels (and page title as well); default false
  save: function TabItem_save(saveImageData) {
    try{
      if (!this.tab || this.tab.parentNode == null || !this.reconnected) // too soon/late to save
        return;

      var data = this.getStorageData(saveImageData);
      if (TabItems.storageSanity(data))
        Storage.saveTab(this.tab, data);
    } catch(e) {
      Utils.log("Error in saving tab value: "+e);
    }
  },

  // ----------
  // Function: setBounds
  // Moves this item to the specified location and size.
  //
  // Parameters:
  //   rect - a <Rect> giving the new bounds
  //   immediately - true if it should not animate; default false
  //   options - an object with additional parameters, see below
  //
  // Possible options:
  //   force - true to always update the DOM even if the bounds haven't changed; default false
  setBounds: function TabItem_setBounds(rect, immediately, options) {
    if (!Utils.isRect(rect)) {
      Utils.trace('TabItem.setBounds: rect is not a real rectangle!', rect);
      return;
    }

    if (!options)
      options = {};

    if (this._zoomPrep)
      this.bounds.copy(rect);
    else {
      var $container = iQ(this.container);
      var $title = iQ('.tab-title', $container);
      var $thumb = iQ('.thumb', $container);
      var $close = iQ('.close', $container);
      var $fav   = iQ('.favicon', $container);
      var css = {};

      const fontSizeRange = new Range(8,15);

      if (rect.left != this.bounds.left || options.force)
        css.left = rect.left;

      if (rect.top != this.bounds.top || options.force)
        css.top = rect.top;

      if (rect.width != this.bounds.width || options.force) {
        css.width = rect.width - this.sizeExtra.x;
        let widthRange = new Range(0,TabItems.tabWidth);
        let proportion = widthRange.proportion(css.width, true); // in [0,1]

        css.fontSize = fontSizeRange.scale(proportion); // returns a value in the fontSizeRange
        css.fontSize += 'px';
      }

      if (rect.height != this.bounds.height || options.force)
        css.height = rect.height - this.sizeExtra.y;

      if (Utils.isEmptyObject(css))
        return;

      this.bounds.copy(rect);

      // If this is a brand new tab don't animate it in from
      // a random location (i.e., from [0,0]). Instead, just
      // have it appear where it should be.
      if (immediately || (!this._hasBeenDrawn)) {
        $container.css(css);
      } else {
        TabItems.pausePainting();
        $container.animate(css, {
          duration: 200,
          easing: "tabviewBounce",
          complete: function() {
            TabItems.resumePainting();
          }
        });
      }

      if (css.fontSize && !this.inStack()) {
        if (css.fontSize < fontSizeRange.min)
          immediately ? $title.hide() : $title.fadeOut();
        else
          immediately ? $title.show() : $title.fadeIn();
      }

      if (css.width) {
        TabItems.update(this.tab);

        let widthRange, proportion;

        if (this.inStack()) {
          if (UI.rtl) {
            $fav.css({top:0, right:0});
          } else {
            $fav.css({top:0, left:0});
          }
          widthRange = new Range(70, 90);
          proportion = widthRange.proportion(css.width); // between 0 and 1
        } else {
          if (UI.rtl) {
            $fav.css({top:4, right:2});
          } else {
            $fav.css({top:4, left:4});
          }
          widthRange = new Range(40, 45);
          proportion = widthRange.proportion(css.width); // between 0 and 1
        }

        if (proportion <= .1)
          $close.hide();
        else
          $close.show().css({opacity:proportion});

        var pad = 1 + 5 * proportion;
        var alphaRange = new Range(0.1,0.2);
        $fav.css({
         "-moz-padding-start": pad + "px",
         "-moz-padding-end": pad + 2 + "px",
         "padding-top": pad + "px",
         "padding-bottom": pad + "px",
         "border-color": "rgba(0,0,0,"+ alphaRange.scale(proportion) +")",
        });
      }

      this._hasBeenDrawn = true;
    }

    this._updateDebugBounds();
    rect = this.getBounds(); // ensure that it's a <Rect>

    if (!Utils.isRect(this.bounds))
      Utils.trace('TabItem.setBounds: this.bounds is not a real rectangle!', this.bounds);

    if (!this.parent && this.tab.parentNode != null)
      this.setTrenches(rect);

    this.save();
  },

  // ----------
  // Function: getBoundsWithTitle
  // Returns a <Rect> for the groupItem's bounds, including the title
  getBoundsWithTitle: function TabItem_getBoundsWithTitle() {
    var b = this.getBounds();
    var $title = iQ(this.container).find('.tab-title');
    var height = b.height;
    if ( Utils.isNumber($title.height()) )
      height += $title.height();
    return new Rect(b.left, b.top, b.width, height);
  },

  // ----------
  // Function: inStack
  // Returns true if this item is in a stacked groupItem.
  inStack: function TabItem_inStack() {
    return iQ(this.container).hasClass("stacked");
  },

  // ----------
  // Function: setZ
  // Sets the z-index for this item.
  setZ: function TabItem_setZ(value) {
    this.zIndex = value;
    iQ(this.container).css({zIndex: value});
  },

  // ----------
  // Function: close
  // Closes this item (actually closes the tab associated with it, which automatically
  // closes the item.
  close: function TabItem_close() {
    gBrowser.removeTab(this.tab);
    this._sendToSubscribers("tabRemoved");

    // No need to explicitly delete the tab data, becasue sessionstore data
    // associated with the tab will automatically go away
  },

  // ----------
  // Function: addClass
  // Adds the specified CSS class to this item's container DOM element.
  addClass: function TabItem_addClass(className) {
    iQ(this.container).addClass(className);
  },

  // ----------
  // Function: removeClass
  // Removes the specified CSS class from this item's container DOM element.
  removeClass: function TabItem_removeClass(className) {
    iQ(this.container).removeClass(className);
  },

  // ----------
  // Function: setResizable
  // If value is true, makes this item resizable, otherwise non-resizable.
  // Shows/hides a visible resize handle as appropriate.
  setResizable: function TabItem_setResizable(value, immediately) {
    var $resizer = iQ('.expander', this.container);

    if (value) {
      this.resizeOptions.minWidth = TabItems.minTabWidth;
      this.resizeOptions.minHeight = TabItems.minTabWidth * (TabItems.tabHeight / TabItems.tabWidth);
      immediately ? $resizer.show() : $resizer.fadeIn();
      this.resizable(true);
    } else {
      immediately ? $resizer.hide() : $resizer.fadeOut();
      this.resizable(false);
    }
  },

  // ----------
  // Function: makeActive
  // Updates this item to visually indicate that it's active.
  makeActive: function TabItem_makeActive() {
    iQ(this.container).addClass("focus");

    if (this.parent)
      this.parent.setActiveTab(this);
  },

  // ----------
  // Function: makeDeactive
  // Updates this item to visually indicate that it's not active.
  makeDeactive: function TabItem_makeDeactive() {
    iQ(this.container).removeClass("focus");
  },

  // ----------
  // Function: zoomIn
  // Allows you to select the tab and zoom in on it, thereby bringing you
  // to the tab in Firefox to interact with.
  // Parameters:
  //   isNewBlankTab - boolean indicates whether it is a newly opened blank tab.
  zoomIn: function TabItem_zoomIn(isNewBlankTab) {
    // don't allow zoom in if its group is hidden
    if (this.parent && this.parent.hidden)
      return;

    var self = this;
    var $tabEl = iQ(this.container);
    var childHitResult = { shouldZoom: true };
    if (this.parent)
      childHitResult = this.parent.childHit(this);

    if (childHitResult.shouldZoom) {
      // Zoom in!
      var orig = $tabEl.bounds();
      var scale = window.innerWidth/orig.width;
      var tab = this.tab;

      function onZoomDone() {
        UI.goToTab(tab);

        if (isNewBlankTab)
          gWindow.gURLBar.focus();

        if (childHitResult.callback)
          childHitResult.callback();
      }

      // The scaleCheat is a clever way to speed up the zoom-in code.
      // Because image scaling is slowest on big images, we cheat and stop the image
      // at scaled-down size and placed accordingly. Because the animation is fast, you can't
      // see the difference but it feels a lot zippier. The only trick is choosing the
      // right animation function so that you don't see a change in percieved
      // animation speed.
      var scaleCheat = 1.7;

      let animateZoom = gPrefBranch.getBoolPref("animate_zoom");
      if (animateZoom) {
        TabItems.pausePainting();
        $tabEl.addClass("front")
        .animate({
          top:    orig.top    * (1 - 1/scaleCheat),
          left:   orig.left   * (1 - 1/scaleCheat),
          width:  orig.width  * scale/scaleCheat,
          height: orig.height * scale/scaleCheat
        }, {
          duration: 230,
          easing: 'fast',
          complete: function() {
            TabItems.resumePainting();
    
            $tabEl
              .css(orig.css())
              .removeClass("front");

            onZoomDone();
          }
        });
      } else
        setTimeout(onZoomDone, 0);
    }
  },

  // ----------
  // Function: zoomOut
  // Handles the zoom down animation after returning to TabView.
  // It is expected that this routine will be called from the chrome thread
  //
  // Parameters:
  //   complete - a function to call after the zoom down animation
  zoomOut: function TabItem_zoomOut(complete) {
    var $tab = iQ(this.container);

    var box = this.getBounds();
    box.width -= this.sizeExtra.x;
    box.height -= this.sizeExtra.y;

    var self = this;
    
    let onZoomDone = function onZoomDone() {
      self.setZoomPrep(false);

      GroupItems.setActiveOrphanTab(null);

      if (typeof complete == "function")
        complete();
    };
    
    let animateZoom = gPrefBranch.getBoolPref("animate_zoom");
    if (animateZoom) {
      TabItems.pausePainting();
      $tab.animate({
        left: box.left,
        top: box.top,
        width: box.width,
        height: box.height
      }, {
        duration: 300,
        easing: 'cubic-bezier', // note that this is legal easing, even without parameters
        complete: function() {
          TabItems.resumePainting();
          onZoomDone();
        }
      });
    } else {
      onZoomDone();
    }
  },

  // ----------
  // Function: setZoomPrep
  // Either go into or return from (depending on <value>) "zoom prep" mode,
  // where the tab fills a large portion of the screen in anticipation of
  // the zoom out animation.
  setZoomPrep: function TabItem_setZoomPrep(value) {
    let animateZoom = gPrefBranch.getBoolPref("animate_zoom");

    var $div = iQ(this.container);
    var data;

    var box = this.getBounds();
    if (value && animateZoom) {
      this._zoomPrep = true;

      // The divide by two part here is a clever way to speed up the zoom-out code.
      // Because image scaling is slowest on big images, we cheat and start the image
      // at half-size and placed accordingly. Because the animation is fast, you can't
      // see the difference but it feels a lot zippier. The only trick is choosing the
      // right animation function so that you don't see a change in percieved
      // animation speed from frame #1 (the tab) to frame #2 (the half-size image) to
      // frame #3 (the first frame of real animation). Choosing an animation that starts
      // fast is key.
      var scaleCheat = 2;
      $div
        .addClass('front')
        .css({
          left: box.left * (1-1/scaleCheat),
          top: box.top * (1-1/scaleCheat),
          width: window.innerWidth/scaleCheat,
          height: box.height * (window.innerWidth / box.width)/scaleCheat
        });
    } else {
      this._zoomPrep = false;
      $div.removeClass('front');

      this.setBounds(box, true, {force: true});
    }
  }
});

// ##########
// Class: TabItems
// Singleton for managing <TabItem>s
let TabItems = {
  minTabWidth: 40,
  tabWidth: 160,
  tabHeight: 120,
  fontSize: 9,
  items: [],
  paintingPaused: 0,
  _tabsWaitingForUpdate: [],
  _heartbeatOn: false,
  _heartbeatTiming: 100, // milliseconds between beats
  _lastUpdateTime: Date.now(),
  _eventListeners: [],

  // ----------
  // Function: init
  // Set up the necessary tracking to maintain the <TabItems>s.
  init: function TabItems_init() {
    Utils.assert(window.AllTabs, "AllTabs must be initialized first");
    var self = this;

    // When a tab is opened, create the TabItem
    this._eventListeners["open"] = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow || tab.pinned)
        return;

      self.link(tab);
    }
    // When a tab's content is loaded, show the canvas and hide the cached data
    // if necessary.
    this._eventListeners["attrModified"] = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow || tab.pinned)
        return;

      self.update(tab);
    }
    // When a tab is closed, unlink.
    this._eventListeners["close"] = function(tab) {
      if (tab.ownerDocument.defaultView != gWindow || tab.pinned)
        return;

      self.unlink(tab);
    }
    for (let name in this._eventListeners) {
      AllTabs.register(name, this._eventListeners[name]);
    }

    // For each tab, create the link.
    AllTabs.tabs.forEach(function(tab) {
      if (tab.ownerDocument.defaultView != gWindow || tab.pinned)
        return;

      self.link(tab, {immediately: true});
      self.update(tab);
    });
  },

  // ----------
  // Function: uninit
  uninit: function TabItems_uninit() {
    for (let name in this._eventListeners) {
      AllTabs.unregister(name, this._eventListeners[name]);
    }
    this.items.forEach(function(tabItem) {
      for (let x in tabItem) {
        if (typeof tabItem[x] == "object")
          tabItem[x] = null;
      }
    });

    this.items = null;
    this._eventListeners = null;
    this._lastUpdateTime = null;
    this._tabsWaitingForUpdate = null;
  },

  // ----------
  // Function: update
  // Takes in a xul:tab.
  update: function TabItems_update(tab) {
    try {
      Utils.assertThrow(tab, "tab");
      Utils.assertThrow(!tab.pinned, "shouldn't be an app tab");
      Utils.assertThrow(tab.tabItem, "should already be linked");

      let shouldDefer = (
        this.isPaintingPaused() ||
        this._tabsWaitingForUpdate.length ||
        Date.now() - this._lastUpdateTime < this._heartbeatTiming
      );

      let isCurrentTab = (
        !UI._isTabViewVisible() &&
        tab == gBrowser.selectedTab
      );

      if (shouldDefer && !isCurrentTab) {
        if (this._tabsWaitingForUpdate.indexOf(tab) == -1)
          this._tabsWaitingForUpdate.push(tab);
      } else
        this._update(tab);
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: _update
  // Takes in a xul:tab.
  _update: function TabItems__update(tab) {
    try {
      Utils.assertThrow(tab, "tab");

      // ___ remove from waiting list if needed
      let index = this._tabsWaitingForUpdate.indexOf(tab);
      if (index != -1)
        this._tabsWaitingForUpdate.splice(index, 1);

      // ___ get the TabItem
      Utils.assertThrow(tab.tabItem, "must already be linked");
      let tabItem = tab.tabItem;

      // ___ icon
      let iconUrl = tab.image;
      if (iconUrl == null)
        iconUrl = Utils.defaultFaviconURL;

      if (iconUrl != tabItem.favEl.src)
        tabItem.favEl.src = iconUrl;

      // ___ URL
      let tabUrl = tab.linkedBrowser.currentURI.spec;
      if (tabUrl != tabItem.url) {
        let oldURL = tabItem.url;
        tabItem.url = tabUrl;

        if (!tabItem.reconnected)
          this.reconnect(tabItem);

        tabItem.save();
      }

      // ___ label
      let label = tab.label;
      let $name = iQ(tabItem.nameEl);
      if (!tabItem.isShowingCachedData && $name.text() != label)
        $name.text(label);

      // ___ thumbnail
      let $canvas = iQ(tabItem.canvasEl);
      if (!tabItem.canvasSizeForced) {
        let w = $canvas.width();
        let h = $canvas.height();
        if (w != tabItem.canvasEl.width || h != tabItem.canvasEl.height) {
          tabItem.canvasEl.width = w;
          tabItem.canvasEl.height = h;
        }
      }

      tabItem.tabCanvas.paint();

      // ___ cache
      // TODO: this logic needs to be better; hiding too soon now
      if (tabItem.isShowingCachedData && !tab.hasAttribute("busy"))
        tabItem.hideCachedData();
    } catch(e) {
      Utils.log(e);
    }

    this._lastUpdateTime = Date.now();
  },

  // ----------
  // Function: link
  // Takes in a xul:tab, creates a TabItem for it and adds it to the scene. 
  link: function TabItems_link(tab, options) {
    try {
      Utils.assertThrow(tab, "tab");
      Utils.assertThrow(!tab.pinned, "shouldn't be an app tab");
      Utils.assertThrow(!tab.tabItem, "shouldn't already be linked");
      new TabItem(tab, options); // sets tab.tabItem to itself
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: unlink
  // Takes in a xul:tab and destroys the TabItem associated with it. 
  unlink: function TabItems_unlink(tab) {
    try {
      Utils.assertThrow(tab, "tab");
      Utils.assertThrow(tab.tabItem, "should already be linked");
      // note that it's ok to unlink an app tab; see .handleTabUnpin

      this.unregister(tab.tabItem);
      tab.tabItem._sendToSubscribers("close");
      iQ(tab.tabItem.container).remove();
      tab.tabItem.removeTrenches();
      Items.unsquish(null, tab.tabItem);

      tab.tabItem = null;
      Storage.saveTab(tab, null);

      let index = this._tabsWaitingForUpdate.indexOf(tab);
      if (index != -1)
        this._tabsWaitingForUpdate.splice(index, 1);
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // when a tab becomes pinned, destroy its TabItem
  handleTabPin: function TabItems_handleTabPin(xulTab) {
    this.unlink(xulTab);
  },

  // ----------
  // when a tab becomes unpinned, create a TabItem for it
  handleTabUnpin: function TabItems_handleTabUnpin(xulTab) {
    this.link(xulTab);
    this.update(xulTab);
  },

  // ----------
  // Function: heartbeat
  // Allows us to spreadout update calls over a period of time.
  heartbeat: function TabItems_heartbeat() {
    if (!this._heartbeatOn)
      return;

    if (this._tabsWaitingForUpdate.length) {
      this._update(this._tabsWaitingForUpdate[0]);
      // _update will remove the tab from the waiting list
    }

    let self = this;
    if (this._tabsWaitingForUpdate.length) {
      setTimeout(function() {
        self.heartbeat();
      }, this._heartbeatTiming);
    } else
      this._hearbeatOn = false;
  },

  // ----------
  // Function: pausePainting
  // Tells TabItems to stop updating thumbnails (so you can do
  // animations without thumbnail paints causing stutters).
  // pausePainting can be called multiple times, but every call to
  // pausePainting needs to be mirrored with a call to <resumePainting>.
  pausePainting: function TabItems_pausePainting() {
    this.paintingPaused++;

    if (this.isPaintingPaused() && this._heartbeatOn)
      this._heartbeatOn = false;
  },

  // ----------
  // Function: resumePainting
  // Undoes a call to <pausePainting>. For instance, if you called
  // pausePainting three times in a row, you'll need to call resumePainting
  // three times before TabItems will start updating thumbnails again.
  resumePainting: function TabItems_resumePainting() {
    this.paintingPaused--;

    if (!this.isPaintingPaused() &&
        this._tabsWaitingForUpdate.length &&
        !this._heartbeatOn) {
      this._heartbeatOn = true;
      this.heartbeat();
    }
  },

  // ----------
  // Function: isPaintingPaused
  // Returns a boolean indicating whether painting
  // is paused or not.
  isPaintingPaused: function TabItems_isPaintingPaused() {
    return this.paintingPaused > 0;
  },

  // ----------
  // Function: register
  // Adds the given <TabItem> to the master list.
  register: function TabItems_register(item) {
    Utils.assert(item && item.isAnItem, 'item must be a TabItem');
    Utils.assert(this.items.indexOf(item) == -1, 'only register once per item');
    this.items.push(item);
  },

  // ----------
  // Function: unregister
  // Removes the given <TabItem> from the master list.
  unregister: function TabItems_unregister(item) {
    var index = this.items.indexOf(item);
    if (index != -1)
      this.items.splice(index, 1);
  },

  // ----------
  // Function: getItems
  // Returns a copy of the master array of <TabItem>s.
  getItems: function TabItems_getItems() {
    return Utils.copy(this.items);
  },

  // ----------
  // Function: saveAll
  // Saves all open <TabItem>s.
  //
  // Parameters:
  //   saveImageData - true to include thumbnail pixels (and page title as well); default false
  saveAll: function TabItems_saveAll(saveImageData) {
    var items = this.getItems();
    items.forEach(function(item) {
      item.save(saveImageData);
    });
  },

  // ----------
  // Function: storageSanity
  // Checks the specified data (as returned by TabItem.getStorageData or loaded from storage)
  // and returns true if it looks valid.
  // TODO: check everything
  storageSanity: function TabItems_storageSanity(data) {
    var sane = true;
    if (!Utils.isRect(data.bounds)) {
      Utils.log('TabItems.storageSanity: bad bounds', data.bounds);
      sane = false;
    }

    return sane;
  },

  // ----------
  // Function: reconnect
  // Given a <TabItem>, attempts to load its persistent data from storage.
  reconnect: function TabItems_reconnect(item) {
    var found = false;

    try{
      Utils.assert(item, 'item');
      Utils.assert(item.tab, 'item.tab');

      if (item.reconnected)
        return true;

      if (!item.tab)
        return false;

      let tabData = Storage.getTabData(item.tab);
      if (tabData && this.storageSanity(tabData)) {
        if (item.parent)
          item.parent.remove(item, {immediately: true});

        item.setBounds(tabData.bounds, true);

        if (Utils.isPoint(tabData.userSize))
          item.userSize = new Point(tabData.userSize);

        if (tabData.groupID) {
          var groupItem = GroupItems.groupItem(tabData.groupID);
          if (groupItem) {
            groupItem.add(item, null, {immediately: true});

            // if it matches the selected tab or no active tab and the browser 
            // tab is hidden, the active group item would be set.
            if (item.tab == gBrowser.selectedTab || 
                (!GroupItems.getActiveGroupItem() && !item.tab.hidden))
              GroupItems.setActiveGroupItem(item.parent);
          }
        }

        if (tabData.imageData) {
          item.showCachedData(tabData);
          // the code in the progress listener doesn't fire sometimes because
          // tab is being restored so need to catch that.
          setTimeout(function() {
            if (item && item.isShowingCachedData) {
              item.hideCachedData();
            }
          }, 15000);
        }

        item.reconnected = true;
        found = {addedToGroup: tabData.groupID};
      } else {
        // We should never have any orphaned tabs. Therefore, item is not 
        // connected if it has no parent and GroupItems.newTab() would handle 
        // the group creation.
        item.reconnected = (item.parent != null);
      }
      item.save();

      if (item.reconnected)
        item._sendToSubscribers("reconnected");
    } catch(e) {
      Utils.log(e);
    }

    return found;
  }
};

// ##########
// Class: TabCanvas
// Takes care of the actual canvas for the tab thumbnail
// Does not need to be accessed from outside of tabitems.js
function TabCanvas(tab, canvas) {
  this.init(tab, canvas);
};

TabCanvas.prototype = {
  // ----------
  // Function: init
  init: function TabCanvas_init(tab, canvas) {
    this.tab = tab;
    this.canvas = canvas;

    var $canvas = iQ(canvas);
    var w = $canvas.width();
    var h = $canvas.height();
    canvas.width = w;
    canvas.height = h;
  },

  // ----------
  // Function: paint
  paint: function TabCanvas_paint(evt) {
    var ctx = this.canvas.getContext("2d");

    var w = this.canvas.width;
    var h = this.canvas.height;
    if (!w || !h)
      return;

    let fromWin = this.tab.linkedBrowser.contentWindow;
    if (fromWin == null) {
      Utils.log('null fromWin in paint');
      return;
    }

    var scaler = w/fromWin.innerWidth;

    // TODO: Potentially only redraw the dirty rect? (Is it worth it?)

    ctx.save();
    ctx.scale(scaler, scaler);
    try{
      ctx.drawWindow(fromWin, fromWin.scrollX, fromWin.scrollY, w/scaler, h/scaler, "#fff");
    } catch(e) {
      Utils.error('paint', e);
    }

    ctx.restore();
  },

  // ----------
  // Function: toImageData
  toImageData: function TabCanvas_toImageData() {
    return this.canvas.toDataURL("image/png", "");
  }
};
