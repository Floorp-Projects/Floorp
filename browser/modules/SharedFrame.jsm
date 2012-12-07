/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "SharedFrame" ];

const Ci = Components.interfaces;
const Cu = Components.utils;

/**
 * The purpose of this module is to create and group various iframe
 * elements that are meant to all display the same content and only
 * one at a time. This makes it possible to have the content loaded
 * only once, while the other iframes can be kept as placeholders to
 * quickly move the content to them through the swapFrameLoaders function
 * when another one of the placeholder is meant to be displayed.
 * */

let Frames = new Map();

/**
 * The Frames map is the main data structure that holds information
 * about the groups being tracked. Each entry's key is the group name,
 * and the object holds information about what is the URL being displayed
 * on that group, and what is the active element on the group (the frame that
 * holds the loaded content).
 * The reference to the activeFrame is a weak reference, which allows the
 * frame to go away at any time, and when that happens the module considers that
 * there are no active elements in that group. The group can be reactivated
 * by changing the URL, calling preload again or adding a new element.
 *
 *
 *  Frames = {
 *    "messages-panel": {
 *      url: string,
 *      activeFrame: weakref
 *    }
 *  }
 *
 * Each object on the map is called a _SharedFrameGroup, which is an internal
 * class of this module which does not automatically keep track of its state. This
 * object should not be used externally, and all control should be handled by the
 * module's functions.
 */

function UNLOADED_URL(aStr) "data:text/html;charset=utf-8,<!-- Unloaded frame " + aStr + " -->";


this.SharedFrame = {
  /**
   * Creates an iframe element and track it as part of the specified group
   * The module must create the iframe itself because it needs to do some special
   * handling for the element's src attribute.
   *
   * @param aGroupName        the name of the group to which this frame belongs
   * @param aParent           the parent element to which the frame will be appended to
   * @param aFrameAttributes  an object with a list of attributes to set in the iframe
   *                          before appending it to the DOM. The "src" attribute has
   *                          special meaning here and if it's not blank it specifies
   *                          the URL that will be initially assigned to this group
   * @param aPreload          optional, tells if the URL specified in the src attribute
   *                          should be preloaded in the frame being created, in case
   *                          it's not yet preloaded in any other frame of the group.
   *                          This parameter has no meaning if src is blank.
   */
  createFrame: function (aGroupName, aParent, aFrameAttributes, aPreload = true) {
    let frame = aParent.ownerDocument.createElement("iframe");

    for (let [key, val] of Iterator(aFrameAttributes)) {
      frame.setAttribute(key, val);
    }

    let src = aFrameAttributes.src;
    if (!src) {
      aPreload = false;
    }

    let group = Frames.get(aGroupName);

    if (group) {
      // If this group has already been created

      if (aPreload && !group.isAlive) {
        // If aPreload is set and the group is not already loaded, load it.
        // This can happen if:
        // - aPreload was not used while creating the previous frames of this group, or
        // - the previously active frame went dead in the meantime
        group.url = src;
        this.preload(aGroupName, frame);
      } else {
        // If aPreload is not set, or the group is already loaded in a different frame,
        // there's not much that we need to do here: just create this frame as an
        // inactivate placeholder
        frame.setAttribute("src", UNLOADED_URL(aGroupName));
      }

    } else {
      // This is the first time we hear about this group, so let's start tracking it,
      // and also preload it if the src attribute was set and aPreload = true
      group = new _SharedFrameGroup(src);
      Frames.set(aGroupName, group);

      if (aPreload) {
        this.preload(aGroupName, frame);
      } else {
        frame.setAttribute("src", UNLOADED_URL(aGroupName));
      }
    }

    aParent.appendChild(frame);
    return frame;

  },

  /**
   * Function that moves the loaded content from one active frame to
   * another one that is currently a placeholder. If there's no active
   * frame in the group, the content is loaded/reloaded.
   *
   * @param aGroupName   the name of the group
   * @param aTargetFrame the frame element to which the content should
   *                     be moved to.
   */
  setOwner: function (aGroupName, aTargetFrame) {
    let group = Frames.get(aGroupName);
    let frame = group.activeFrame;

    if (frame == aTargetFrame) {
      // nothing to do here
      return;
    }

    if (group.isAlive) {
      // Move document ownership to the desired frame, and make it the active one
      frame.QueryInterface(Ci.nsIFrameLoaderOwner).swapFrameLoaders(aTargetFrame);
      group.activeFrame = aTargetFrame;
    } else {
      // Previous owner was dead, reload the document at the new owner and make it the active one
      aTargetFrame.setAttribute("src", group.url);
      group.activeFrame = aTargetFrame;
    }
  },

  /**
   * Updates the current URL in use by this group, and loads it into the active frame.
   *
   * @param aGroupName  the name of the group
   * @param aURL        the new url
   */
  updateURL: function (aGroupName, aURL) {
    let group = Frames.get(aGroupName);
    group.url = aURL;

    if (group.isAlive) {
      group.activeFrame.setAttribute("src", aURL);
    }
  },

  /**
   * Loads the group's url into a target frame, if the group doesn't have a currently
   * active frame.
   *
   * @param aGroupName    the name of the group
   * @param aTargetFrame  the frame element which should be made active and
   *                      have the group's content loaded to
   */
  preload: function (aGroupName, aTargetFrame) {
    let group = Frames.get(aGroupName);
    if (!group.isAlive) {
      aTargetFrame.setAttribute("src", group.url);
      group.activeFrame = aTargetFrame;
    }
  },

  /**
   * Tells if a group currently have an active element.
   *
   * @param aGroupName  the name of the group
   */
  isGroupAlive: function (aGroupName) {
    return Frames.get(aGroupName).isAlive;
  },

  /**
   * Forgets about this group. This function doesn't need to be used
   * unless the group's name needs to be reused.
   *
   * @param aGroupName  the name of the group
   */
  forgetGroup: function (aGroupName) {
    Frames.delete(aGroupName);
  }
}


function _SharedFrameGroup(aURL) {
  this.url = aURL;
  this._activeFrame = null;
}

_SharedFrameGroup.prototype = {
  get isAlive() {
    let frame = this.activeFrame;
    return !!(frame &&
              frame.contentDocument &&
              frame.contentDocument.location);
  },

  get activeFrame() {
    return this._activeFrame &&
           this._activeFrame.get();
  },

  set activeFrame(aActiveFrame) {
    this._activeFrame = aActiveFrame
                        ? Cu.getWeakReference(aActiveFrame)
                        : null;
  }
}
