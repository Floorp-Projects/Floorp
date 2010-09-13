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
 * The Original Code is infoitems.js.
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
// Title: infoitems.js

// ##########
// Class: InfoItem
// An <Item> in TabView used for displaying information, such as the welcome video.
// Note that it implements the <Subscribable> interface.
//
// ----------
// Constructor: InfoItem
//
// Parameters:
//   bounds - a <Rect> for where the item should be located
//   options - various options for this infoItem (see below)
//
// Possible options:
//   locked - see <Item.locked>; default is {}
//   dontPush - true if this infoItem shouldn't push away on creation; default is false
function InfoItem(bounds, options) {
  try {
    Utils.assertThrow(Utils.isRect(bounds), 'bounds');

    if (typeof options == 'undefined')
      options = {};

    this._inited = false;
    this.isAnInfoItem = true;
    this.defaultSize = bounds.size();
    this.locked = (options.locked ? Utils.copy(options.locked) : {});
    this.bounds = new Rect(bounds);
    this.isDragging = false;

    var self = this;

    var $container = iQ('<div>')
      .addClass('info-item')
      .css(this.bounds)
      .appendTo('body');

    this.$contents = iQ('<div>')
      .appendTo($container);

    var $close = iQ('<div>')
      .addClass('close')
      .click(function() {
        self.close();
      })
      .appendTo($container);

    // ___ locking
    if (this.locked.bounds)
      $container.css({cursor: 'default'});

    if (this.locked.close)
      $close.hide();

    // ___ Superclass initialization
    this._init($container[0]);

    if (this.$debug)
      this.$debug.css({zIndex: -1000});

    // ___ Finish Up
    if (!this.locked.bounds)
      this.draggable();

    // ___ Position
    this.snap();

    // ___ Push other objects away
    if (!options.dontPush)
      this.pushAway();

    this._inited = true;
    this.save();
  } catch(e) {
    Utils.log(e);
  }
};

// ----------
InfoItem.prototype = Utils.extend(new Item(), new Subscribable(), {

  // ----------
  // Function: getStorageData
  // Returns all of the info worth storing about this item.
  getStorageData: function InfoItem_getStorageData() {
    var data = null;

    try {
      data = {
        bounds: this.getBounds(),
        locked: Utils.copy(this.locked)
      };
    } catch(e) {
      Utils.log(e);
    }

    return data;
  },

  // ----------
  // Function: save
  // Saves this item to persistent storage.
  save: function InfoItem_save() {
    try {
      if (!this._inited) // too soon to save now
        return;

      var data = this.getStorageData();

    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: setBounds
  // Sets the bounds with the given <Rect>, animating unless "immediately" is false.
  setBounds: function InfoItem_setBounds(rect, immediately) {
    try {
      Utils.assertThrow(Utils.isRect(rect), 'InfoItem.setBounds: rect must be a real rectangle!');

      // ___ Determine what has changed
      var css = {};

      if (rect.left != this.bounds.left)
        css.left = rect.left;

      if (rect.top != this.bounds.top)
        css.top = rect.top;

      if (rect.width != this.bounds.width)
        css.width = rect.width;

      if (rect.height != this.bounds.height)
        css.height = rect.height;

      if (Utils.isEmptyObject(css))
        return;

      this.bounds = new Rect(rect);
      Utils.assertThrow(Utils.isRect(this.bounds), 
          'InfoItem.setBounds: this.bounds must be a real rectangle!');

      // ___ Update our representation
      if (immediately) {
        iQ(this.container).css(css);
      } else {
        TabItems.pausePainting();
        iQ(this.container).animate(css, {
          duration: 350,
          easing: "tabviewBounce",
          complete: function() {
            TabItems.resumePainting();
          }
        });
      }

      this._updateDebugBounds();
      this.setTrenches(rect);
      this.save();
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: setZ
  // Set the Z order for the item's container.
  setZ: function InfoItem_setZ(value) {
    try {
      Utils.assertThrow(typeof value == 'number', 'value must be a number');

      this.zIndex = value;

      iQ(this.container).css({zIndex: value});

      if (this.$debug)
        this.$debug.css({zIndex: value + 1});
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: close
  // Closes the item.
  close: function InfoItem_close() {
    try {
      this._sendToSubscribers("close");
      this.removeTrenches();
      iQ(this.container).fadeOut(function() {
        iQ(this).remove();
        Items.unsquish();
      });

    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: html
  // Sets the item's container's html to the specified value.
  html: function InfoItem_html(value) {
    try {
      Utils.assertThrow(typeof value == 'string', 'value must be a string');
      this.$contents.html(value);
    } catch(e) {
      Utils.log(e);
    }
  }
});
