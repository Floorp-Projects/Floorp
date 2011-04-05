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
 * Tim Taubert <tim.taubert@gmx.de>
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
  document.body.appendChild(TabItems.fragment().cloneNode(true));
  
  // The document fragment contains just one Node
  // As per DOM3 appendChild: it will then be the last child
  let div = document.body.lastChild;
  let $div = iQ(div);

  this._cachedImageData = null;
  this.canvasSizeForced = false;
  this.$thumb = iQ('.thumb', $div);
  this.$fav   = iQ('.favicon', $div);
  this.$tabTitle = iQ('.tab-title', $div);
  this.$canvas = iQ('.thumb canvas', $div);
  this.$cachedThumb = iQ('img.cached-thumb', $div);
  this.$favImage = iQ('.favicon>img', $div);
  this.$close = iQ('.close', $div);

  this.tabCanvas = new TabCanvas(this.tab, this.$canvas[0]);

  this.defaultSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
  this._hidden = false;
  this.isATabItem = true;
  this.keepProportional = true;
  this._hasBeenDrawn = false;
  this._reconnected = false;
  this.isStacked = false;
  this.url = "";

  var self = this;

  this.isDragging = false;

  // Read off the total vertical and horizontal padding on the tab container
  // and cache this value, as it must be the same for every TabItem.
  if (Utils.isEmptyObject(TabItems.tabItemPadding)) {
    TabItems.tabItemPadding.x = parseInt($div.css('padding-left'))
        + parseInt($div.css('padding-right'));
  
    TabItems.tabItemPadding.y = parseInt($div.css('padding-top'))
        + parseInt($div.css('padding-bottom'));
  }
  
  this.bounds = new Rect(0,0,1,1);

  this._lastTabUpdateTime = Date.now();

  // ___ superclass setup
  this._init(div);

  // ___ drag/drop
  // override dropOptions with custom tabitem methods
  // This is mostly to support the phantom groupItems.
  this.dropOptions.drop = function(e) {
    var $target = this.$container;
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
    var $target = this.$container;
    this.isDropTarget = true;

    $target.removeClass("acceptsDrop");

    var phantomMargin = 40;

    var groupItemBounds = this.getBounds();
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
    var phantom = this.$container.data("phantomGroupItem");
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
    if (iQ(e.target).hasClass("close") || Utils.isMiddleClick(e)) {
      self.closedManually = true;
      self.close();
    } else {
      if (!Items.item(this).isDragging)
        self.zoomIn();
    }
  });

  this.setResizable(true, options.immediately);
  this.droppable(true);

  TabItems.register(this);

  // ___ reconnect to data from Storage
  if (!TabItems.reconnectingPaused())
    this._reconnect();
};

TabItem.prototype = Utils.extend(new Item(), new Subscribable(), {
  // ----------
  // Function: toString
  // Prints [TabItem (tab)] for debug use
  toString: function TabItem_toString() {
    return "[TabItem (" + this.tab + ")]";
  },

  // ----------
  // Function: forceCanvasSize
  // Repaints the thumbnail with the given resolution, and forces it
  // to stay that resolution until unforceCanvasSize is called.
  forceCanvasSize: function TabItem_forceCanvasSize(w, h) {
    this.canvasSizeForced = true;
    this.$canvas[0].width = w;
    this.$canvas[0].height = h;
    this.tabCanvas.paint();
  },

  // ----------
  // Function: _getFontSizeFromWidth
  // Private method that returns the fontsize to use given the tab's width
  _getFontSizeFromWidth: function TabItem__getFontSizeFromWidth(width) {
    let widthRange = new Range(0,TabItems.tabWidth);
    let proportion = widthRange.proportion(width-TabItems.tabItemPadding.x, true);
    // proportion is in [0,1]
    return TabItems.fontSizeRange.scale(proportion);
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
    this._cachedImageData = tabData.imageData;
    this.$cachedThumb.attr("src", this._cachedImageData).show();
    this.$canvas.css({opacity: 0.0});
    this.$tabTitle.text(tabData.title ? tabData.title : "");
  },

  // ----------
  // Function: hideCachedData
  // Hides the cached data i.e. image and title and show the canvas.
  hideCachedData: function TabItem_hideCachedData() {
    this.$cachedThumb.hide();
    this.$canvas.css({opacity: 1.0});
    if (this._cachedImageData)
      this._cachedImageData = null;
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

    let tabData = null;
    let self = this;
    let imageDataCb = function(imageData) {
      Utils.assertThrow(tabData, "tabData");
      
      tabData.imageData = imageData;

      let currentUrl = self.tab.linkedBrowser.currentURI.spec;
      // If we have a cached image, then show it if the loaded URL matches
      // what the cache is from, OR the loaded URL is blank, which means
      // that the page hasn't loaded yet.
      if (tabData.imageData &&
          (tabData.url == currentUrl || currentUrl == 'about:blank')) {
        self.showCachedData(tabData);
      }
    };
    // getTabData returns the sessionstore contents, but passes
    // a callback to run when the thumbnail is finally loaded.
    tabData = Storage.getTabData(this.tab, imageDataCb);
    if (tabData && TabItems.storageSanity(tabData)) {
      if (self.parent)
        self.parent.remove(self, {immediately: true});

      self.setBounds(tabData.bounds, true);

      if (Utils.isPoint(tabData.userSize))
        self.userSize = new Point(tabData.userSize);

      if (tabData.groupID) {
        var groupItem = GroupItems.groupItem(tabData.groupID);
        if (groupItem) {
          groupItem.add(self, {immediately: true});

          // if it matches the selected tab or no active tab and the browser
          // tab is hidden, the active group item would be set.
          if (self.tab == gBrowser.selectedTab ||
              (!GroupItems.getActiveGroupItem() && !self.tab.hidden))
            GroupItems.setActiveGroupItem(self.parent);
        }
      }
    } else {
      // create tab by double click is handled in UI_init().
      if (!TabItems.creatingNewOrphanTab)
        GroupItems.newTab(self, {immediately: true});
    }

    self._reconnected = true;
    self.save();
    self._sendToSubscribers("reconnected");
  },
  
  // ----------
  // Function: setHidden
  // Hide/unhide this item
  setHidden: function TabItem_setHidden(val) {
    if (val)
      this.addClass("tabHidden");
    else
      this.removeClass("tabHidden");
    this._hidden = val;
  },

  // ----------
  // Function: getHidden
  // Return hide state of item
  getHidden: function TabItem_getHidden() {
    return this._hidden;
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
  setBounds: function TabItem_setBounds(inRect, immediately, options) {
    if (!Utils.isRect(inRect)) {
      Utils.trace('TabItem.setBounds: rect is not a real rectangle!', inRect);
      return;
    }

    if (!options)
      options = {};

    // force the input size to be valid
    let validSize = TabItems.calcValidSize(
      new Point(inRect.width, inRect.height), 
      {hideTitle: (this.isStacked || options.hideTitle === true)});
    let rect = new Rect(inRect.left, inRect.top, 
      validSize.x, validSize.y);

    var css = {};

    if (rect.left != this.bounds.left || options.force)
      css.left = rect.left;

    if (rect.top != this.bounds.top || options.force)
      css.top = rect.top;

    if (rect.width != this.bounds.width || options.force) {
      css.width = rect.width - TabItems.tabItemPadding.x;
      css.fontSize = this._getFontSizeFromWidth(rect.width);
      css.fontSize += 'px';
    }

    if (rect.height != this.bounds.height || options.force) {
      if (!this.isStacked)
          css.height = rect.height - TabItems.tabItemPadding.y -
                       TabItems.fontSizeRange.max;
      else
        css.height = rect.height - TabItems.tabItemPadding.y;
    }

    if (Utils.isEmptyObject(css))
      return;

    this.bounds.copy(rect);

    // If this is a brand new tab don't animate it in from
    // a random location (i.e., from [0,0]). Instead, just
    // have it appear where it should be.
    if (immediately || (!this._hasBeenDrawn)) {
      this.$container.css(css);
    } else {
      TabItems.pausePainting();
      this.$container.animate(css, {
          duration: 200,
        easing: "tabviewBounce",
        complete: function() {
          TabItems.resumePainting();
        }
      });
    }

    if (css.fontSize && !(this.parent && this.parent.isStacked())) {
      if (css.fontSize < TabItems.fontSizeRange.min)
        immediately ? this.$tabTitle.hide() : this.$tabTitle.fadeOut();
      else
        immediately ? this.$tabTitle.show() : this.$tabTitle.fadeIn();
    }

    if (css.width) {
      TabItems.update(this.tab);

      let widthRange, proportion;

      if (this.parent && this.parent.isStacked()) {
        if (UI.rtl) {
          this.$fav.css({top:0, right:0});
        } else {
          this.$fav.css({top:0, left:0});
        }
        widthRange = new Range(70, 90);
        proportion = widthRange.proportion(css.width); // between 0 and 1
      } else {
        if (UI.rtl) {
          this.$fav.css({top:4, right:2});
        } else {
          this.$fav.css({top:4, left:4});
        }
        widthRange = new Range(40, 45);
        proportion = widthRange.proportion(css.width); // between 0 and 1
      }

      if (proportion <= .1)
        this.$close.hide();
      else
        this.$close.show().css({opacity:proportion});

      var pad = 1 + 5 * proportion;
      var alphaRange = new Range(0.1,0.2);
      this.$fav.css({
       "-moz-padding-start": pad + "px",
       "-moz-padding-end": pad + 2 + "px",
       "padding-top": pad + "px",
       "padding-bottom": pad + "px",
       "border-color": "rgba(0,0,0,"+ alphaRange.scale(proportion) +")",
      });
    }

    this._hasBeenDrawn = true;

    UI.clearShouldResizeItems();

    rect = this.getBounds(); // ensure that it's a <Rect>

    if (!Utils.isRect(this.bounds))
      Utils.trace('TabItem.setBounds: this.bounds is not a real rectangle!', this.bounds);

    if (!this.parent && this.tab.parentNode != null)
      this.setTrenches(rect);

    this.save();
  },

  // ----------
  // Function: setZ
  // Sets the z-index for this item.
  setZ: function TabItem_setZ(value) {
    this.zIndex = value;
    this.$container.css({zIndex: value});
  },

  // ----------
  // Function: close
  // Closes this item (actually closes the tab associated with it, which automatically
  // closes the item.
  // Parameters:
  //   groupClose - true if this method is called by group close action.
  // Returns true if this tab is removed.
  close: function TabItem_close(groupClose) {
    // When the last tab is closed, put a new tab into closing tab's group. If
    // closing tab doesn't belong to a group and no empty group, create a new 
    // one for the new tab.
    if (!groupClose && gBrowser.tabs.length == 1) {
      if (this.tab._tabViewTabItem.parent) {
        group = this.tab._tabViewTabItem.parent;
      } else {
        let emptyGroups = GroupItems.groupItems.filter(function (groupItem) {
          return (!groupItem.getChildren().length);
        });
        group = (emptyGroups.length ? emptyGroups[0] : GroupItems.newGroup());
      }
      group.newTab();
    }
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
    this.$container.addClass(className);
  },

  // ----------
  // Function: removeClass
  // Removes the specified CSS class from this item's container DOM element.
  removeClass: function TabItem_removeClass(className) {
    this.$container.removeClass(className);
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
    this.$container.addClass("focus");

    if (this.parent)
      this.parent.setActiveTab(this);
  },

  // ----------
  // Function: makeDeactive
  // Updates this item to visually indicate that it's not active.
  makeDeactive: function TabItem_makeDeactive() {
    this.$container.removeClass("focus");
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

    let self = this;
    let $tabEl = this.$container;
    let $canvas = this.$canvas;

    UI.setActiveTab(this);
    if (this.parent) {
      GroupItems.setActiveGroupItem(this.parent);
    } else {
      GroupItems.setActiveOrphanTab(this);
    }

    TabItems._update(this.tab, {force: true});

    // Zoom in!
    let tab = this.tab;

    function onZoomDone() {
      $canvas.css({ '-moz-transform': null });
      $tabEl.removeClass("front");

      UI.goToTab(tab);

      // tab might not be selected because hideTabView() is invoked after 
      // UI.goToTab() so we need to setup everything for the gBrowser.selectedTab
      if (tab != gBrowser.selectedTab) {
        UI.onTabSelect(gBrowser.selectedTab);
      } else { 
        if (isNewBlankTab)
          gWindow.gURLBar.focus();
      }
      if (self.parent && self.parent.expanded)
        self.parent.collapse();
    }

    let animateZoom = gPrefBranch.getBoolPref("animate_zoom");
    if (animateZoom) {
      let transform = this.getZoomTransform();
      TabItems.pausePainting();

      if (this.parent && this.parent.expanded)
        $tabEl.removeClass("stack-trayed");
      $tabEl.addClass("front");
      $canvas
        .css({ '-moz-transform-origin': transform.transformOrigin })
        .animate({ '-moz-transform': transform.transform }, {
          duration: 230,
          easing: 'fast',
          complete: function() {
            onZoomDone();

            setTimeout(function() {
              TabItems.resumePainting();
            }, 0);
          }
        });
    } else {
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
    let $tab = this.$container, $canvas = this.$canvas;
    var self = this;
    
    let onZoomDone = function onZoomDone() {
      $tab.removeClass("front");
      $canvas.css("-moz-transform", null);

      GroupItems.setActiveOrphanTab(null);

      if (typeof complete == "function")
        complete();
    };

    UI.setActiveTab(this);
    TabItems._update(this.tab, {force: true});

    $tab.addClass("front");

    // If we're in a stacked group, make sure we become the
    // topChild now so that we show the zoom animation correctly.
    if (this.parent && this.parent.isStacked())
      this.parent.setTopChild(this);

    let animateZoom = gPrefBranch.getBoolPref("animate_zoom");
    if (animateZoom) {
      // The scaleCheat of 2 here is a clever way to speed up the zoom-out
      // code. See getZoomTransform() below.
      let transform = this.getZoomTransform(2);
      TabItems.pausePainting();

      $canvas.css({
        '-moz-transform': transform.transform,
        '-moz-transform-origin': transform.transformOrigin
      });

      $canvas.animate({ "-moz-transform": "scale(1.0)" }, {
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
  // Function: getZoomTransform
  // Returns the transform function which represents the maximum bounds of the
  // tab thumbnail in the zoom animation.
  getZoomTransform: function TabItem_getZoomTransform(scaleCheat) {
    // Taking the bounds of the container (as opposed to the canvas) makes us
    // immune to any transformations applied to the canvas.
    let { left, top, width, height, right, bottom } = this.$container.bounds();

    let { innerWidth: windowWidth, innerHeight: windowHeight } = window;

    // The scaleCheat is a clever way to speed up the zoom-in code.
    // Because image scaling is slowest on big images, we cheat and stop
    // the image at scaled-down size and placed accordingly. Because the
    // animation is fast, you can't see the difference but it feels a lot
    // zippier. The only trick is choosing the right animation function so
    // that you don't see a change in percieved animation speed from frame #1
    // (the tab) to frame #2 (the half-size image) to frame #3 (the first frame
    // of real animation). Choosing an animation that starts fast is key.

    if (!scaleCheat)
      scaleCheat = 1.7;

    let zoomWidth = width + (window.innerWidth - width) / scaleCheat;
    let zoomScaleFactor = zoomWidth / width;

    let zoomHeight = height * zoomScaleFactor;
    let zoomTop = top * (1 - 1/scaleCheat);
    let zoomLeft = left * (1 - 1/scaleCheat);

    let xOrigin = (left - zoomLeft) / ((left - zoomLeft) + (zoomLeft + zoomWidth - right)) * 100;
    let yOrigin = (top - zoomTop) / ((top - zoomTop) + (zoomTop + zoomHeight - bottom)) * 100;

    return {
      transformOrigin: xOrigin + "% " + yOrigin + "%",
      transform: "scale(" + zoomScaleFactor + ")"
    };
  }
});

// ##########
// Class: TabItems
// Singleton for managing <TabItem>s
let TabItems = {
  minTabWidth: 40,
  tabWidth: 160,
  tabHeight: 120,
  tabAspect: 0, // set in init
  invTabAspect: 0, // set in init  
  fontSize: 9,
  fontSizeRange: new Range(8,15),
  _fragment: null,
  items: [],
  paintingPaused: 0,
  _tabsWaitingForUpdate: null,
  _heartbeat: null, // see explanation at startHeartbeat() below
  _heartbeatTiming: 200, // milliseconds between calls
  _maxTimeForUpdating: 200, // milliseconds that consecutive updates can take
  _lastUpdateTime: Date.now(),
  _eventListeners: [],
  _pauseUpdateForTest: false,
  creatingNewOrphanTab: false,
  tempCanvas: null,
  _reconnectingPaused: false,
  tabItemPadding: {},

  // ----------
  // Function: toString
  // Prints [TabItems count=count] for debug use
  toString: function TabItems_toString() {
    return "[TabItems count=" + this.items.length + "]";
  },

  // ----------
  // Function: init
  // Set up the necessary tracking to maintain the <TabItems>s.
  init: function TabItems_init() {
    Utils.assert(window.AllTabs, "AllTabs must be initialized first");
    let self = this;
    
    // Set up tab priority queue
    this._tabsWaitingForUpdate = new TabPriorityQueue();
    this.minTabHeight = this.minTabWidth * this.tabHeight / this.tabWidth;
    this.tabAspect = this.tabHeight / this.tabWidth;
    this.invTabAspect = 1 / this.tabAspect;

    let $canvas = iQ("<canvas>")
      .attr('moz-opaque', '');
    $canvas.appendTo(iQ("body"));
    $canvas.hide();
    this.tempCanvas = $canvas[0];
    // 150 pixels is an empirical size, below which FF's drawWindow()
    // algorithm breaks down
    this.tempCanvas.width = 150;
    this.tempCanvas.height = 112;

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

      // XXX bug #635975 - don't unlink the tab if the dom window is closing.
      if (!UI.isDOMWindowClosing)
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
    this._tabsWaitingForUpdate.clear();
  },

  // ----------
  // Function: fragment
  // Return a DocumentFragment which has a single <div> child. This child node
  // will act as a template for all TabItem containers.
  // The first call of this function caches the DocumentFragment in _fragment.
  fragment: function TabItems_fragment() {
    if (this._fragment)
      return this._fragment;

    let div = document.createElement("div");
    div.classList.add("tab");
    div.innerHTML = "<div class='thumb'>" +
            "<img class='cached-thumb' style='display:none'/><canvas moz-opaque/></div>" +
            "<div class='favicon'><img/></div>" +
            "<span class='tab-title'>&nbsp;</span>" +
            "<div class='close'></div>" +
            "<div class='expander'></div>";
    this._fragment = document.createDocumentFragment();
    this._fragment.appendChild(div);

    return this._fragment;
  },

  // ----------
  // Function: isComplete
  // Return whether the xul:tab has fully loaded.
  isComplete: function TabItems_update(tab) {
    // If our readyState is complete, but we're showing about:blank,
    // and we're not loading about:blank, it means we haven't really
    // started loading. This can happen to the first few tabs in a
    // page.
    Utils.assertThrow(tab, "tab");
    return (
      tab.linkedBrowser.contentDocument.readyState == 'complete' &&
      !(tab.linkedBrowser.contentDocument.URL == 'about:blank' &&
        tab._tabViewTabItem.url != 'about:blank')
    );
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
        this._tabsWaitingForUpdate.hasItems() ||
        Date.now() - this._lastUpdateTime < this._heartbeatTiming
      );

      if (shouldDefer) {
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
  //
  // Parameters:
  //   tab - a xul tab to update
  //   options - an object with additional parameters, see below
  //
  // Possible options:
  //   force - true to always update the tab item even if it's incomplete
  _update: function TabItems__update(tab, options) {
    try {
      if (this._pauseUpdateForTest)
        return;

      Utils.assertThrow(tab, "tab");

      // ___ get the TabItem
      Utils.assertThrow(tab._tabViewTabItem, "must already be linked");
      let tabItem = tab._tabViewTabItem;

      // Even if the page hasn't loaded, display the favicon and title

      // ___ icon
      if (this.shouldLoadFavIcon(tab.linkedBrowser)) {
        let iconUrl = tab.image;
        if (!iconUrl)
          iconUrl = Utils.defaultFaviconURL;

        if (iconUrl != tabItem.$favImage[0].src)
          tabItem.$favImage[0].src = iconUrl;

        iQ(tabItem.$fav[0]).show();
      } else {
        if (tabItem.$favImage[0].hasAttribute("src"))
          tabItem.$favImage[0].removeAttribute("src");
        iQ(tabItem.$fav[0]).hide();
      }

      // ___ label
      let label = tab.label;
      let $name = tabItem.$tabTitle;
      if ($name.text() != label)
        $name.text(label);

      // ___ remove from waiting list now that we have no other
      // early returns
      this._tabsWaitingForUpdate.remove(tab);

      // ___ URL
      let tabUrl = tab.linkedBrowser.currentURI.spec;
      if (tabUrl != tabItem.url) {
        let oldURL = tabItem.url;
        tabItem.url = tabUrl;
        tabItem.save();
      }

      // ___ Make sure the tab is complete and ready for updating.
      if (!this.isComplete(tab) && (!options || !options.force)) {
        // If it's incomplete, stick it on the end of the queue
        this._tabsWaitingForUpdate.push(tab);
        return;
      }

      // ___ thumbnail
      let $canvas = tabItem.$canvas;
      if (!tabItem.canvasSizeForced) {
        let w = $canvas.width();
        let h = $canvas.height();
        if (w != tabItem.$canvas[0].width || h != tabItem.$canvas[0].height) {
          tabItem.$canvas[0].width = w;
          tabItem.$canvas[0].height = h;
        }
      }

      this._lastUpdateTime = Date.now();
      tabItem._lastTabUpdateTime = this._lastUpdateTime;

      tabItem.tabCanvas.paint();

      // ___ cache
      if (tabItem.isShowingCachedData())
        tabItem.hideCachedData();

      // ___ notify subscribers that a full update has completed.
      tabItem._sendToSubscribers("updated");
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

      if (tab._tabViewTabItem == GroupItems.getActiveOrphanTab())
        GroupItems.setActiveOrphanTab(null);

      this.unregister(tab._tabViewTabItem);
      tab._tabViewTabItem._sendToSubscribers("close");
      tab._tabViewTabItem.$container.remove();
      tab._tabViewTabItem.removeTrenches();
      Items.unsquish(null, tab._tabViewTabItem);

      tab._tabViewTabItem = null;
      Storage.saveTab(tab, null);

      this._tabsWaitingForUpdate.remove(tab);
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
  // _heartbeat is used to make sure that we don't add multiple 
  // setTimeout chains.
  startHeartbeat: function TabItems_startHeartbeat() {
    if (!this._heartbeat) {
      let self = this;
      this._heartbeat = setTimeout(function() {
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
    this._heartbeat = null;

    if (this.isPaintingPaused() || !UI.isIdle)
      return;

    let accumTime = 0;
    let items = this._tabsWaitingForUpdate.getItems();
    // Do as many updates as we can fit into a "perceived" amount
    // of time, which is tunable.
    while (accumTime < this._maxTimeForUpdating && items.length) {
      let updateBegin = Date.now();
      this._update(items.pop());
      let updateEnd = Date.now();

      // Maintain a simple average of time for each tabitem update
      // We can use this as a base by which to delay things like
      // tab zooming, so there aren't any hitches.
      let deltaTime = updateEnd - updateBegin;
      accumTime += deltaTime;
    }

    if (this._tabsWaitingForUpdate.hasItems())
      this.startHeartbeat();
  },

  // ----------
  // Function: pausePainting
  // Tells TabItems to stop updating thumbnails (so you can do
  // animations without thumbnail paints causing stutters).
  // pausePainting can be called multiple times, but every call to
  // pausePainting needs to be mirrored with a call to <resumePainting>.
  pausePainting: function TabItems_pausePainting() {
    this.paintingPaused++;
    if (this._heartbeat) {
      clearTimeout(this._heartbeat);
      this._heartbeat = null;
    }
  },

  // ----------
  // Function: resumePainting
  // Undoes a call to <pausePainting>. For instance, if you called
  // pausePainting three times in a row, you'll need to call resumePainting
  // three times before TabItems will start updating thumbnails again.
  resumePainting: function TabItems_resumePainting() {
    this.paintingPaused--;
    Utils.assert(this.paintingPaused > -1, "paintingPaused should not go below zero");
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
  // Function: _getWidthForHeight
  // Private method that returns the tabitem width given a height.
  // Set options.hideTitle=true to measure without a title.
  // Default is to measure with a title.
  _getWidthForHeight: function TabItems__getWidthForHeight(height, options) {    
    let titleSize = (options !== undefined && options.hideTitle === true) ? 
      0 : TabItems.fontSizeRange.max;
    return Math.max(0, Math.max(TabItems.minTabHeight, height - titleSize)) * 
      TabItems.invTabAspect;
  },

  // ----------
  // Function: _getHeightForWidth
  // Private method that returns the tabitem height given a width.
  // Set options.hideTitle=false to measure without a title.
  // Default is to measure with a title.
  _getHeightForWidth: function TabItems__getHeightForWidth(width, options) {
    let titleSize = (options !== undefined && options.hideTitle === true) ? 
      0 : TabItems.fontSizeRange.max;
    return Math.max(0, Math.max(TabItems.minTabWidth,width)) *
      TabItems.tabAspect + titleSize;
  },
  
  // ----------
  // Function: calcValidSize
  // Pass in a desired size, and receive a size based on proper title
  // size and aspect ratio.
  calcValidSize: function TabItems_calcValidSize(size, options) {
    Utils.assert(Utils.isPoint(size), 'input is a Point');
    let retSize = new Point(0,0);
    if (size.x==-1) {
      retSize.x = this._getWidthForHeight(size.y, options);
      retSize.y = size.y;
    } else if (size.y==-1) {
      retSize.x = size.x;
      retSize.y = this._getHeightForWidth(size.x, options);
    } else {
      let fitHeight = this._getHeightForWidth(size.x, options);
      let fitWidth = this._getWidthForHeight(size.y, options);

      // Go with the smallest final dimension.
      if (fitWidth < size.x) {
        retSize.x = fitWidth;
        retSize.y = size.y;
      } else {
        retSize.x = size.x;
        retSize.y = fitHeight;
      }
    }
    return retSize;
  }
};

// ##########
// Class: TabPriorityQueue
// Container that returns tab items in a priority order
// Current implementation assigns tab to either a high priority
// or low priority queue, and toggles which queue items are popped
// from. This guarantees that high priority items which are constantly
// being added will not eclipse changes for lower priority items.
function TabPriorityQueue() {
};

TabPriorityQueue.prototype = {
  _low: [], // low priority queue
  _high: [], // high priority queue

  // ----------
  // Function: toString
  // Prints [TabPriorityQueue count=count] for debug use
  toString: function TabPriorityQueue_toString() {
    return "[TabPriorityQueue count=" + (this._low.length + this._high.length) + "]";
  },

  // ----------
  // Function: clear
  // Empty the update queue
  clear: function TabPriorityQueue_clear() {
    this._low = [];
    this._high = [];
  },

  // ----------
  // Function: hasItems
  // Return whether pending items exist
  hasItems: function TabPriorityQueue_hasItems() {
    return (this._low.length > 0) || (this._high.length > 0);
  },

  // ----------
  // Function: getItems
  // Returns all queued items, ordered from low to high priority
  getItems: function TabPriorityQueue_getItems() {
    return this._low.concat(this._high);
  },

  // ----------
  // Function: push
  // Add an item to be prioritized
  push: function TabPriorityQueue_push(tab) {
    // Push onto correct priority queue.
    // It's only low priority if it's in a stack, and isn't the top,
    // and the stack isn't expanded.
    // If it already exists in the destination queue,
    // leave it. If it exists in a different queue, remove it first and push
    // onto new queue.
    let item = tab._tabViewTabItem;
    if (item.parent && (item.parent.isStacked() &&
      !item.parent.isTopOfStack(item) &&
      !item.parent.expanded)) {
      let idx = this._high.indexOf(tab);
      if (idx != -1) {
        this._high.splice(idx, 1);
        this._low.unshift(tab);
      } else if (this._low.indexOf(tab) == -1)
        this._low.unshift(tab);
    } else {
      let idx = this._low.indexOf(tab);
      if (idx != -1) {
        this._low.splice(idx, 1);
        this._high.unshift(tab);
      } else if (this._high.indexOf(tab) == -1)
        this._high.unshift(tab);
    }
  },

  // ----------
  // Function: pop
  // Remove and return the next item in priority order
  pop: function TabPriorityQueue_pop() {
    let ret = null;
    if (this._high.length)
      ret = this._high.pop();
    else if (this._low.length)
      ret = this._low.pop();
    return ret;
  },

  // ----------
  // Function: peek
  // Return the next item in priority order, without removing it
  peek: function TabPriorityQueue_peek() {
    let ret = null;
    if (this._high.length)
      ret = this._high[this._high.length-1];
    else if (this._low.length)
      ret = this._low[this._low.length-1];
    return ret;
  },

  // ----------
  // Function: remove
  // Remove the passed item
  remove: function TabPriorityQueue_remove(tab) {
    let index = this._high.indexOf(tab);
    if (index != -1)
      this._high.splice(index, 1);
    else {
      index = this._low.indexOf(tab);
      if (index != -1)
        this._low.splice(index, 1);
    }
  }
};

// ##########
// Class: TabCanvas
// Takes care of the actual canvas for the tab thumbnail
// Does not need to be accessed from outside of tabitems.js
function TabCanvas(tab, canvas) {
  this.tab = tab;
  this.canvas = canvas;
};

TabCanvas.prototype = {
  // ----------
  // Function: toString
  // Prints [TabCanvas (tab)] for debug use
  toString: function TabCanvas_toString() {
    return "[TabCanvas (" + this.tab + ")]";
  },

  // ----------
  // Function: paint
  paint: function TabCanvas_paint(evt) {
    var w = this.canvas.width;
    var h = this.canvas.height;
    if (!w || !h)
      return;

    if (!this.tab.linkedBrowser.contentWindow) {
      Utils.log('no tab.linkedBrowser.contentWindow in TabCanvas.paint()');
      return;
    }

    let ctx = this.canvas.getContext("2d");
    let tempCanvas = TabItems.tempCanvas;
    let bgColor = '#fff';

    if (w < tempCanvas.width) {
      // Small draw case where nearest-neighbor algorithm breaks down in Windows
      // First draw to a larger canvas (150px wide), and then draw that image
      // to the destination canvas.
      let tempCtx = tempCanvas.getContext("2d");
      this._drawWindow(tempCtx, tempCanvas.width, tempCanvas.height, bgColor);

      // Now copy to tabitem canvas.
      try {
        this._fillCanvasBackground(ctx, w, h, bgColor);
        ctx.drawImage(tempCanvas, 0, 0, w, h);
      } catch (e) {
        Utils.error('paint', e);
      }
    } else {
      // General case where nearest neighbor algorithm looks good
      // Draw directly to the destination canvas
      this._drawWindow(ctx, w, h, bgColor);
    }
  },

  // ----------
  // Function: _fillCanvasBackground
  // Draws a rectangle of <width>x<height> with color <bgColor> to the given
  // canvas context.
  _fillCanvasBackground: function TabCanvas__fillCanvasBackground(ctx, width, height, bgColor) {
    ctx.fillStyle = bgColor;
    ctx.fillRect(0, 0, width, height);
  },

  // ----------
  // Function: _drawWindow
  // Draws contents of the tabs' browser window to the given canvas context.
  _drawWindow: function TabCanvas__drawWindow(ctx, width, height, bgColor) {
    this._fillCanvasBackground(ctx, width, height, bgColor);

    let rect = this._calculateClippingRect(width, height);
    let scaler = width / rect.width;

    ctx.save();
    ctx.scale(scaler, scaler);

    try {
      let win = this.tab.linkedBrowser.contentWindow;
      ctx.drawWindow(win, rect.left, rect.top, rect.width, rect.height,
                     bgColor, ctx.DRAWWINDOW_DO_NOT_FLUSH);
    } catch (e) {
      Utils.error('paint', e);
    }

    ctx.restore();
  },

  // ----------
  // Function: _calculateClippingRect
  // Calculate the clipping rect that will be projected to the tab's
  // thumbnail canvas.
  _calculateClippingRect: function TabCanvas__calculateClippingRect(origWidth, origHeight) {
    let win = this.tab.linkedBrowser.contentWindow;

    // TODO BUG 631593: retrieve actual scrollbar width
    // 25px is supposed to be width of the vertical scrollbar
    let maxWidth = Math.max(1, win.innerWidth - 25);
    let maxHeight = win.innerHeight;

    let height = Math.min(maxHeight, Math.floor(origHeight * maxWidth / origWidth));
    let width = Math.floor(origWidth * height / origHeight);

    // very short pages in combination with a very wide browser window force us
    // to extend the clipping rect and add some empty space around the thumb
    let factor = 0.7;
    if (width < maxWidth * factor) {
      width = maxWidth * factor;
      height = Math.floor(origHeight * width / origWidth);
    }

    let left = win.scrollX + Math.max(0, Math.round((maxWidth - width) / 2));
    let top = win.scrollY;

    return new Rect(left, top, width, height);
  },

  // ----------
  // Function: toImageData
  toImageData: function TabCanvas_toImageData() {
    return this.canvas.toDataURL("image/png", "");
  }
};
