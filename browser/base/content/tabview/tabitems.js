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
  this.tab._tabViewTabItem = this;

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

  this._cachedImageData = null;
  this.shouldHideCachedData = false;
  this.canvasSizeForced = false;
  this.favEl = (iQ('.favicon', $div))[0];
  this.favImgEl = (iQ('.favicon>img', $div))[0];
  this.nameEl = (iQ('.tab-title', $div))[0];
  this.thumbEl = (iQ('.thumb', $div))[0];
  this.canvasEl = (iQ('.thumb canvas', $div))[0];
  this.cachedThumbEl = (iQ('img.cached-thumb', $div))[0];

  this.tabCanvas = new TabCanvas(this.tab, this.canvasEl);

  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  this.locked = {};
  this.isATabItem = true;
  this._zoomPrep = false;
  this.sizeExtra = new Point();
  this.keepProportional = true;
  this._hasBeenDrawn = false;
  this._reconnected = false;

  var self = this;

  this.isDragging = false;

  this.sizeExtra.x = parseInt($div.css('padding-left'))
      + parseInt($div.css('padding-right'));

  this.sizeExtra.y = parseInt($div.css('padding-top'))
      + parseInt($div.css('padding-bottom'));

  this.bounds = $div.bounds();

  this._lastTabUpdateTime = Date.now();

  // ___ superclass setup
  this._init($div[0]);

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
      .css(groupItemBounds)
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
      phantom.css(updatedBounds);

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

    // press close button or middle mouse click
    if (iQ(e.target).hasClass("close") || e.button == 1) {
      self.close();
    } else {
      if (!Items.item(this).isDragging)
        self.zoomIn();
    }
  });

  iQ("<div>")
    .addClass('close')
    .appendTo($div);
  this.closeEl = (iQ(".close", $div))[0];

  iQ("<div>")
    .addClass('expander')
    .appendTo($div);

  this.setResizable(true, options.immediately);
  this.droppable(true);
  this._updateDebugBounds();

  TabItems.register(this);

  // ___ reconnect to data from Storage
  if (!TabItems.reconnectingPaused())
    this._reconnect();
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
  // Function: isShowingCachedData
  // Returns a boolean indicates whether the cached data is being displayed or
  // not. 
  isShowingCachedData: function() {
    return (this._cachedImageData != null);
  },

  // ----------
  // Function: showCachedData
  // Shows the cached data i.e. image and title.  Note: this method should only
  // be called at browser startup with the cached data avaliable.
  //
  // Parameters:
  //   tabData - the tab data
  showCachedData: function TabItem_showCachedData(tabData) {
    if (!this._cachedImageData) {
      TabItems.cachedDataCounter++;
      this.tab.linkedBrowser._tabViewTabItemWithCachedData = this;
      if (TabItems.cachedDataCounter == 1)
        gBrowser.addTabsProgressListener(TabItems.tabsProgressListener);
    }
    this._cachedImageData = tabData.imageData;
    let $nameElement = iQ(this.nameEl);
    let $canvasElement = iQ(this.canvasEl);
    let $cachedThumbElement = iQ(this.cachedThumbEl);
    $cachedThumbElement.attr("src", this._cachedImageData).show();
    $canvasElement.css({opacity: 0.0});
    $nameElement.text(tabData.title ? tabData.title : "");
  },

  // ----------
  // Function: hideCachedData
  // Hides the cached data i.e. image and title and show the canvas.
  hideCachedData: function TabItem_hideCachedData() {
    let $canvasElement = iQ(this.canvasEl);
    let $cachedThumbElement = iQ(this.cachedThumbEl);
    $cachedThumbElement.hide();
    $canvasElement.css({opacity: 1.0});
    if (this._cachedImageData) {
      TabItems.cachedDataCounter--;
      this._cachedImageData = null;
      this.tab.linkedBrowser._tabViewTabItemWithCachedData = null;
      if (TabItems.cachedDataCounter == 0)
        gBrowser.removeTabsProgressListener(TabItems.tabsProgressListener);
    }
  },

  // ----------
  // Function: getStorageData
  // Get data to be used for persistent storage of this object.
  //
  // Parameters:
  //   getImageData - true to include thumbnail pixels (and page title as well); default false
  getStorageData: function TabItem_getStorageData(getImageData) {
    let imageData = null;

    if (getImageData) { 
      if (this._cachedImageData)
        imageData = this._cachedImageData;
      else if (this.tabCanvas)
        imageData = this.tabCanvas.toImageData();
    }

    return {
      bounds: this.getBounds(),
      userSize: (Utils.isPoint(this.userSize) ? new Point(this.userSize) : null),
      url: this.tab.linkedBrowser.currentURI.spec,
      groupID: (this.parent ? this.parent.id : 0),
      imageData: imageData,
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
      if (!this.tab || this.tab.parentNode == null || !this._reconnected) // too soon/late to save
        return;

      var data = this.getStorageData(saveImageData);
      if (TabItems.storageSanity(data))
        Storage.saveTab(this.tab, data);
    } catch(e) {
      Utils.log("Error in saving tab value: "+e);
    }
  },

  // ----------
  // Function: _reconnect
  // Load the reciever's persistent data from storage. If there is none, 
  // treats it as a new tab. 
  _reconnect: function TabItem__reconnect() {
    Utils.assertThrow(!this._reconnected, "shouldn't already be reconnected");
    Utils.assertThrow(this.tab, "should have a xul:tab");
    
    let tabData = Storage.getTabData(this.tab);
    if (tabData && TabItems.storageSanity(tabData)) {
      if (this.parent)
        this.parent.remove(this, {immediately: true});

      this.setBounds(tabData.bounds, true);

      if (Utils.isPoint(tabData.userSize))
        this.userSize = new Point(tabData.userSize);

      if (tabData.groupID) {
        var groupItem = GroupItems.groupItem(tabData.groupID);
        if (groupItem) {
          groupItem.add(this, {immediately: true});

          // if it matches the selected tab or no active tab and the browser 
          // tab is hidden, the active group item would be set.
          if (this.tab == gBrowser.selectedTab || 
              (!GroupItems.getActiveGroupItem() && !this.tab.hidden))
            GroupItems.setActiveGroupItem(this.parent);
        }
      }

      if (tabData.imageData)
        this.showCachedData(tabData);
    } else {
      // create tab by double click is handled in UI_init().
      if (!TabItems.creatingNewOrphanTab)
        GroupItems.newTab(this, {immediately: true});
    }

    this._reconnected = true;  
    this.save();
    this._sendToSubscribers("reconnected");
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

    TabItems.enforceMinSize(rect);

    if (this._zoomPrep)
      this.bounds.copy(rect);
    else {
      var $container = iQ(this.container);
      var $title = iQ(this.nameEl);
      var $thumb = iQ(this.thumbEl);
      var $close = iQ(this.closeEl);
      var $fav   = iQ(this.favEl);
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
  // Returns true if this tab is removed.
  close: function TabItem_close() {
    // when "TabClose" event is fired, the browser tab is about to close and our 
    // item "close" is fired before the browser tab actually get closed. 
    // Therefore, we need "tabRemoved" event below.
    gBrowser.removeTab(this.tab);
    let tabNotClosed = 
      Array.some(gBrowser.tabs, function(tab) { return tab == this.tab; }, this);
    if (!tabNotClosed)
      this._sendToSubscribers("tabRemoved");

    // No need to explicitly delete the tab data, becasue sessionstore data
    // associated with the tab will automatically go away
    return !tabNotClosed;
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
      this.resizeOptions.minHeight = TabItems.minTabHeight;
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
      var tab = this.tab;
      var orig = $tabEl.bounds();

      function onZoomDone() {
        UI.goToTab(tab);

        // tab might not be selected because hideTabView() is invoked after 
        // UI.goToTab() so we need to setup everything for the gBrowser.selectedTab
        if (tab != gBrowser.selectedTab) {
          UI.onTabSelect(gBrowser.selectedTab);
        } else { 
          if (isNewBlankTab)
            gWindow.gURLBar.focus();
        }
        if (childHitResult.callback)
          childHitResult.callback();
      }

      let animateZoom = gPrefBranch.getBoolPref("animate_zoom");
      if (animateZoom) {
        TabItems.pausePainting();
        $tabEl.addClass("front")
        .animate(this.getZoomRect(), {
          duration: 230,
          easing: 'fast',
          complete: function() {
            TabItems.resumePainting();
    
            $tabEl
              .css(orig)
              .removeClass("front");

            onZoomDone();
          }
        });
      } else {
        setTimeout(onZoomDone, 0);
      } 
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
  // Function: getZoomRect
  // Returns a faux rect (just an object with top, left, width, height)
  // which represents the maximum bounds of the tab thumbnail in the zoom
  // animation. Note that this is not just the rect of the window itself,
  // due to scaleCheat.
  getZoomRect: function TabItem_getZoomRect(scaleCheat) {
    let $tabEl = iQ(this.container);
    let orig = $tabEl.bounds();
    // The scaleCheat is a clever way to speed up the zoom-in code.
    // Because image scaling is slowest on big images, we cheat and stop
    // the image at scaled-down size and placed accordingly. Because the
    // animation is fast, you can't see the difference but it feels a lot
    // zippier. The only trick is choosing the right animation function so
    // that you don't see a change in percieved animation speed.
    if (!scaleCheat)
      scaleCheat = 1.7;

    let zoomWidth = orig.width + (window.innerWidth - orig.width) / scaleCheat;
    return {
      top:    orig.top    * (1 - 1/scaleCheat),
      left:   orig.left   * (1 - 1/scaleCheat),
      width:  zoomWidth,
      height: orig.height * zoomWidth / orig.width
    };
  },

  // ----------
  // Function: setZoomPrep
  // Either go into or return from (depending on <value>) "zoom prep" mode,
  // where the tab fills a large portion of the screen in anticipation of
  // the zoom out animation.
  setZoomPrep: function TabItem_setZoomPrep(value) {
    let animateZoom = gPrefBranch.getBoolPref("animate_zoom");

    var $div = iQ(this.container);

    if (value && animateZoom) {
      this._zoomPrep = true;

      // The scaleCheat of 2 here is a clever way to speed up the zoom-out code.
      // Because image scaling is slowest on big images, we cheat and start the image
      // at half-size and placed accordingly. Because the animation is fast, you can't
      // see the difference but it feels a lot zippier. The only trick is choosing the
      // right animation function so that you don't see a change in percieved
      // animation speed from frame #1 (the tab) to frame #2 (the half-size image) to
      // frame #3 (the first frame of real animation). Choosing an animation that starts
      // fast is key.

      $div
        .addClass('front')
        .css(this.getZoomRect(2));
    } else {
      let box = this.getBounds();
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
  cachedDataCounter: 0,  // total number of cached data being displayed.
  tabsProgressListener: null,
  _tabsWaitingForUpdate: [],
  _heartbeatOn: false, // see explanation at startHeartbeat() below
  _heartbeatTiming: 100, // milliseconds between _checkHeartbeat() calls
  _lastUpdateTime: Date.now(),
  _eventListeners: [],
  _pauseUpdateForTest: false,
  creatingNewOrphanTab: false,
  tempCanvas: null,
  _reconnectingPaused: false,

  // ----------
  // Function: init
  // Set up the necessary tracking to maintain the <TabItems>s.
  init: function TabItems_init() {
    Utils.assert(window.AllTabs, "AllTabs must be initialized first");
    let self = this;
    
    this.minTabHeight = this.minTabWidth * this.tabHeight / this.tabWidth;

    let $canvas = iQ("<canvas>");
    $canvas.appendTo(iQ("body"));
    $canvas.hide();
    this.tempCanvas = $canvas[0];
    // 150 pixels is an empirical size, below which FF's drawWindow()
    // algorithm breaks down
    this.tempCanvas.width = 150;
    this.tempCanvas.height = 150;

    this.tabsProgressListener = {
      onStateChange: function(browser, webProgress, request, stateFlags, status) {
        if ((stateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
            (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)) {
          // browser would only has _tabViewTabItemWithCachedData if 
          // it's showing cached data.
          if (browser._tabViewTabItemWithCachedData)
            browser._tabViewTabItemWithCachedData.shouldHideCachedData = true;
        }
      }
    };

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
    if (this.tabsProgressListener)
      gBrowser.removeTabsProgressListener(this.tabsProgressListener);

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
      Utils.assertThrow(tab._tabViewTabItem, "should already be linked");

      let shouldDefer = (
        this.isPaintingPaused() ||
        this._tabsWaitingForUpdate.length ||
        Date.now() - this._lastUpdateTime < this._heartbeatTiming
      );

      let isCurrentTab = (
        !UI.isTabViewVisible() &&
        tab == gBrowser.selectedTab
      );

      if (shouldDefer && !isCurrentTab) {
        if (this._tabsWaitingForUpdate.indexOf(tab) == -1)
          this._tabsWaitingForUpdate.push(tab);
        this.startHeartbeat();
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
      if (this._pauseUpdateForTest)
        return;

      Utils.assertThrow(tab, "tab");

      // ___ remove from waiting list if needed
      let index = this._tabsWaitingForUpdate.indexOf(tab);
      if (index != -1)
        this._tabsWaitingForUpdate.splice(index, 1);

      // ___ get the TabItem
      Utils.assertThrow(tab._tabViewTabItem, "must already be linked");
      let tabItem = tab._tabViewTabItem;

      // ___ icon
      if (this.shouldLoadFavIcon(tab.linkedBrowser)) {
        let iconUrl = tab.image;
        if (!iconUrl)
          iconUrl = Utils.defaultFaviconURL;

        if (iconUrl != tabItem.favImgEl.src)
          tabItem.favImgEl.src = iconUrl;

        iQ(tabItem.favEl).show();
      } else {
        if (tabItem.favImgEl.hasAttribute("src"))
          tabItem.favImgEl.removeAttribute("src");
        iQ(tabItem.favEl).hide();
      }

      // ___ URL
      let tabUrl = tab.linkedBrowser.currentURI.spec;
      if (tabUrl != tabItem.url) {
        let oldURL = tabItem.url;
        tabItem.url = tabUrl;
        tabItem.save();
      }

      // ___ label
      let label = tab.label;
      let $name = iQ(tabItem.nameEl);
      if (!tabItem.isShowingCachedData() && $name.text() != label)
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

      this._lastUpdateTime = Date.now();
      tabItem._lastTabUpdateTime = this._lastUpdateTime;

      tabItem.tabCanvas.paint();

      // ___ cache
      if (tabItem.isShowingCachedData() && tabItem.shouldHideCachedData)
        tabItem.hideCachedData();
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: shouldLoadFavIcon
  // Takes a xul:browser and checks whether we should display a favicon for it.
  shouldLoadFavIcon: function TabItems_shouldLoadFavIcon(browser) {
    return !(browser.contentDocument instanceof window.ImageDocument) &&
           gBrowser.shouldLoadFavIcon(browser.contentDocument.documentURIObject);
  },

  // ----------
  // Function: link
  // Takes in a xul:tab, creates a TabItem for it and adds it to the scene. 
  link: function TabItems_link(tab, options) {
    try {
      Utils.assertThrow(tab, "tab");
      Utils.assertThrow(!tab.pinned, "shouldn't be an app tab");
      Utils.assertThrow(!tab._tabViewTabItem, "shouldn't already be linked");
      new TabItem(tab, options); // sets tab._tabViewTabItem to itself
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
      Utils.assertThrow(tab._tabViewTabItem, "should already be linked");
      // note that it's ok to unlink an app tab; see .handleTabUnpin

      this.unregister(tab._tabViewTabItem);
      tab._tabViewTabItem._sendToSubscribers("close");
      iQ(tab._tabViewTabItem.container).remove();
      tab._tabViewTabItem.removeTrenches();
      Items.unsquish(null, tab._tabViewTabItem);

      tab._tabViewTabItem = null;
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
  // Function: startHeartbeat
  // Start a new heartbeat if there isn't one already started.
  // The heartbeat is a chain of setTimeout calls that allows us to spread
  // out update calls over a period of time.
  // _heartbeatOn is used to make sure that we don't add multiple 
  // setTimeout chains.
  startHeartbeat: function TabItems_startHeartbeat() {
    if (!this._heartbeatOn) {
      this._heartbeatOn = true;
      let self = this;
      setTimeout(function() {
        self._checkHeartbeat();
      }, this._heartbeatTiming);
    }
  },
  
  // ----------
  // Function: _checkHeartbeat
  // This periodically checks for tabs waiting to be updated, and calls
  // _update on them.
  // Should only be called by startHeartbeat and resumePainting.
  _checkHeartbeat: function TabItems__checkHeartbeat() {
    this._heartbeatOn = false;
    
    if (this.isPaintingPaused())
      return;

    if (this._tabsWaitingForUpdate.length && UI.isIdle()) {
      this._update(this._tabsWaitingForUpdate[0]);
      //_update will remove the tab from the waiting list
    }

    if (this._tabsWaitingForUpdate.length) {
      this.startHeartbeat();
    }
  },

   // ----------
   // Function: pausePainting
   // Tells TabItems to stop updating thumbnails (so you can do
   // animations without thumbnail paints causing stutters).
   // pausePainting can be called multiple times, but every call to
   // pausePainting needs to be mirrored with a call to <resumePainting>.
   pausePainting: function TabItems_pausePainting() {
     this.paintingPaused++;
   },
 
   // ----------
   // Function: resumePainting
   // Undoes a call to <pausePainting>. For instance, if you called
   // pausePainting three times in a row, you'll need to call resumePainting
   // three times before TabItems will start updating thumbnails again.
   resumePainting: function TabItems_resumePainting() {
     this.paintingPaused--;
     if (!this.isPaintingPaused())
       this.startHeartbeat();
   },

  // ----------
  // Function: isPaintingPaused
  // Returns a boolean indicating whether painting
  // is paused or not.
  isPaintingPaused: function TabItems_isPaintingPaused() {
    return this.paintingPaused > 0;
  },

  // ----------
  // Function: pauseReconnecting
  // Don't reconnect any new tabs until resume is called.
  pauseReconnecting: function TabItems_pauseReconnecting() {
    Utils.assertThrow(!this._reconnectingPaused, "shouldn't already be paused");

    this._reconnectingPaused = true;
  },
  
  // ----------
  // Function: resumeReconnecting
  // Reconnect all of the tabs that were created since we paused.
  resumeReconnecting: function TabItems_resumeReconnecting() {
    Utils.assertThrow(this._reconnectingPaused, "should already be paused");

    this._reconnectingPaused = false;
    this.items.forEach(function(item) {
      if (!item._reconnected)
        item._reconnect();
    });
  },
  
  // ----------
  // Function: reconnectingPaused
  // Returns true if reconnecting is paused.
  reconnectingPaused: function TabItems_reconnectingPaused() {
    return this._reconnectingPaused;
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
  // Function: enforceMinSize
  // Takes a <Rect> and modifies that <Rect> in case it is too small to be
  // the bounds of a <TabItem>.
  //
  // Parameters:
  //   bounds - (<Rect>) the target bounds of a <TabItem>
  enforceMinSize: function TabItems_enforceMinSize(bounds) {
    bounds.width = Math.max(bounds.width, this.minTabWidth);
    bounds.height = Math.max(bounds.height, this.minTabHeight);
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
    var w = this.canvas.width;
    var h = this.canvas.height;
    if (!w || !h)
      return;

    let fromWin = this.tab.linkedBrowser.contentWindow;
    if (fromWin == null) {
      Utils.log('null fromWin in paint');
      return;
    }

    let tempCanvas = TabItems.tempCanvas;
    if (w < tempCanvas.width) {
      // Small draw case where nearest-neighbor algorithm breaks down in Windows
      // First draw to a larger canvas (150px wide), and then draw that image
      // to the destination canvas.
      
      var tempCtx = tempCanvas.getContext("2d");
      
      let canvW = tempCanvas.width;
      let canvH = (h/w) * canvW;
      
      var scaler = canvW/fromWin.innerWidth;
  
      tempCtx.save();
      tempCtx.clearRect(0,0,tempCanvas.width,tempCanvas.height);
      tempCtx.scale(scaler, scaler);
      try{
        tempCtx.drawWindow(fromWin, fromWin.scrollX, fromWin.scrollY, 
          canvW/scaler, canvH/scaler, "#fff");
      } catch(e) {
        Utils.error('paint', e);
      }  
      tempCtx.restore();
      
      // Now copy to tabitem canvas. No save/restore necessary.      
      var destCtx = this.canvas.getContext("2d");      
      try{
        // the tempcanvas is square, so draw it as a square.
        destCtx.drawImage(tempCanvas, 0, 0, w, w);
      } catch(e) {
        Utils.error('paint', e);
      }  
      
    } else {
      // General case where nearest neighbor algorithm looks good
      // Draw directly to the destination canvas
      
      var ctx = this.canvas.getContext("2d");
      
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
    }
  },

  // ----------
  // Function: toImageData
  toImageData: function TabCanvas_toImageData() {
    return this.canvas.toDataURL("image/png", "");
  }
};
