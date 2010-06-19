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
 * The Original Code is drag.js.
 *
 * The Initial Developer of the Original Code is
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
// Title: drag.js 

// ----------
// Variable: drag
// The Drag that's currently in process. 
var drag = {
  info: null,
  zIndex: 100
};


// ##########
// Class: Drag (formerly DragInfo)
// Helper class for dragging <Item>s
// 
// ----------
// Constructor: Drag
// Called to create a Drag in response to a jQuery-UI draggable "start" event.
var Drag = function(element, event) {
  this.el = element;
  this.$el = iQ(this.el);
  this.item = Items.item(this.el);
  this.parent = this.item.parent;
  this.startPosition = new Point(event.clientX, event.clientY);
  this.startTime = Utils.getMilliseconds();
  
  this.item.isDragging = true;
  this.$el.data('isDragging', true);
  this.item.setZ(999999);
  
  this.safeWindowBounds = this.getSafeWindowBounds();
  Trenches.activateOthersTrenches(this.el);
  
  // When a tab drag starts, make it the focused tab.
  if(this.item.isAGroup) {
    var tab = Page.getActiveTab();
    if(!tab || tab.parent != this.item) {
      if(this.item._children.length)
        Page.setActiveTab(this.item._children[0]);
    }
  } else {
    Page.setActiveTab(this.item);
  }
};

Drag.prototype = {
  // ----------  
  snap: function(event, ui){
    var bounds = this.item.getBounds();
    var update = false; // need to update
    var updateX = false;
    var updateY = false;

    // OH SNAP!
    var newRect = Trenches.snap(bounds,true);
    if (newRect) // might be false if no changes were made
      update = true;

    // make sure the bounds are in the window.
    newRect = newRect || bounds;

    var swb = this.safeWindowBounds;

    if (newRect.left < swb.left) {
      newRect.left = swb.left;
      update = true;
      updateX = true;
    }
    if (newRect.left + newRect.width > swb.left + swb.width) {
      if (updateX)
        newRect.width = swb.left + swb.width - newRect.left;
      else
        newRect.left = swb.left + swb.width - newRect.width;
      update = true;
    }
    if (newRect.top < swb.top) {
      newRect.top = swb.top;
      update = true;
      updateY = true;
    }
    if (newRect.top + newRect.height > swb.top + swb.height) {
      if (updateY)
        newRect.height = swb.top + swb.height - newRect.top;
      else
        newRect.top = swb.top + swb.height - newRect.height;
      update = true;
    }


    if (update)
      this.item.setBounds(newRect,true);

    return ui;
  },
  
  // ----------  
  // Function: drag
  // Called in response to a jQuery-UI draggable "drag" event.
  drag: function(event, ui) {
//    if(this.item.isAGroup) {
      var bb = this.item.getBounds();
      bb.left = ui.position.left;
      bb.top = ui.position.top;
      this.item.setBounds(bb, true);
      ui = this.snap(event,ui);
//    } else
//      this.item.reloadBounds();
      
    if(this.parent && this.parent.expanded) {
      var now = Utils.getMilliseconds();
      var distance = this.startPosition.distance(new Point(event.clientX, event.clientY));
      if(/* now - this.startTime > 500 ||  */distance > 100) {
        this.parent.remove(this.item);
        this.parent.collapse();
      }
    }
  },

  // ----------  
  // Function: stop
  // Called in response to a jQuery-UI draggable "stop" event.
  stop: function() {
    this.item.isDragging = false;
    this.$el.data('isDragging', false);    

    // I'm commenting this out for a while as I believe it feels uncomfortable
    // that groups go away when there is still a tab in them. I do this at
    // the cost of symmetry. -- Aza
    /*
    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 1 && !this.parent.getTitle()) {
      this.parent.remove(this.parent._children[0]);
    }*/

    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 0 && !this.parent.getTitle()) {
      this.parent.close();
    }
     
    if(this.parent && this.parent.expanded)
      this.parent.arrange();
      
    if(this.item && !this.item.parent) {
      this.item.setZ(drag.zIndex);
      drag.zIndex++;
      
      this.item.reloadBounds();
      this.item.pushAway();
    }
    
    Trenches.disactivate();
    
  },
  getSafeWindowBounds: function() {
    // the safe bounds that would keep it "in the window"
    var gutter = Items.defaultGutter;
    var pageBounds = Items.getPageBounds();
    var newTabGroupBounds = Groups.getBoundsForNewTabGroup();
    // Here, I've set the top gutter separately, as the top of the window has its own
    // extra chrome which makes a large top gutter unnecessary.
    // TODO: set top gutter separately, elsewhere.
    var topGutter = 5;
    return new Rect( gutter, topGutter, pageBounds.width - 2 * gutter, newTabGroupBounds.top -  gutter - topGutter );
  }
};
