# -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
#   Pierre Chanial <chanial@noos.fr>
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   David Hyatt <hyatt@apple.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.

var BookmarksMenu = {
  _selection:null,
  _target:null,
  _orientation:null,
  
  /////////////////////////////////////////////////////////////////////////////
  // shows the 'Open in Tabs' menu item if validOpenInTabsMenuItem is true -->
  showOpenInTabsMenuItem: function (aTarget)
  {
    if (!this.validOpenInTabsMenuItem(aTarget) ||
        aTarget.lastChild.getAttribute("class") == "openintabs-menuitem")
      return;
    var element = document.createElementNS(XUL_NS, "menuseparator");
    element.setAttribute("class", "openintabs-menuseparator");
    aTarget.appendChild(element);
    element = document.createElementNS(XUL_NS, "menuitem");
    element.setAttribute("class", "openintabs-menuitem");
    element.setAttribute("label", BookmarksUtils.getLocaleString("cmd_bm_openfolder"));
    element.setAttribute("accesskey", BookmarksUtils.getLocaleString("cmd_bm_openfolder_accesskey"));
    aTarget.appendChild(element);
  },

#ifdef XP_MACOSX
  // FIXME: Work around a Mac menu bug with onpopuphidden.  onpopuphiding/hidden on mac fire
  // before oncommand executes on a menuitem.  For now we're applying a XUL workaround
  // since it's too late to fix this in the Mac widget code for 1.5.
  gOpenInTabsParent : null,
  delayedHideOpenInTabsMenuItem: function ()
  {
    if (!gOpenInTabsParent.hasChildNodes())
      return;
    if (gOpenInTabsParent.lastChild.getAttribute("class") == "openintabs-menuitem") {
      gOpenInTabsParent.removeChild(gOpenInTabsParent.lastChild);
      gOpenInTabsParent.removeChild(gOpenInTabsParent.lastChild);
    }
  },

  hideOpenInTabsMenuItem: function (aTarget)
  {
    gOpenInTabsParent = aTarget;
    setTimeout(function() { BookmarksMenu.delayedHideOpenInTabsMenuItem(); }, 0);
  },

#else

  /////////////////////////////////////////////////////////////////////////////
  // hides the 'Open in Tabs' on popuphidden so that we won't duplicate it -->
  hideOpenInTabsMenuItem: function (aTarget)
  {
    if (aTarget.hasChildNodes() &&
        aTarget.lastChild.getAttribute("class") == "openintabs-menuitem") {
      aTarget.removeChild(aTarget.lastChild);
      aTarget.removeChild(aTarget.lastChild);
    }
  },
#endif

  /////////////////////////////////////////////////////////////////////////////
  // returns false if...
  // - the parent is the bookmark menu or the chevron
  // - the menupopup contains ony one bookmark
  validOpenInTabsMenuItem: function (aTarget)
  {
    var rParent = RDF.GetResource(aTarget.parentNode.id)
    var type = BookmarksUtils.resolveType(rParent);
    if (type != "Folder" && type != "PersonalToolbarFolder")
      return false;
    var count = 0;
    if (!aTarget.hasChildNodes())
      return false;
    var curr = aTarget.firstChild;
    do {
      type = BookmarksUtils.resolveType(curr.id);
      if (type == "Bookmark" && ++count == 2)
        return true;
      curr = curr.nextSibling;
    } while (curr);
    return false;
  },

  //////////////////////////////////////////////////////////////////////////
  // Fill a context menu popup with menuitems appropriate for the current
  // selection.
  createContextMenu: function (aEvent)
  {
    var target = document.popupNode;
    if (!this.isBTBookmark(target.id))
      return false;
    var bt = document.getElementById("bookmarks-ptf");
    bt.focus(); // buttons in the bt have -moz-user-focus: ignore

    this._selection   = this.getBTSelection(target);
    this._orientation = this.getBTOrientation(aEvent, target);
    this._target      = this.getBTTarget(target, this._orientation);
    BookmarksCommand.createContextMenu(aEvent, this._selection);
    this.onCommandUpdate();
    aEvent.target.addEventListener("mousemove", BookmarksMenuController.onMouseMove, false);
    return true;
  },

  /////////////////////////////////////////////////////////////////////////
  // Clean up after closing the context menu popup
  destroyContextMenu: function (aEvent)
  {
#   note that this method is called after doCommand.
#   let''s focus the content and dismiss the popup chain (needed when the user
#   type escape or if he/she clicks outside the context menu)
    if (content)
      content.focus();
    // XXXpch: see bug 210910, it should be done properly in the backend
    BookmarksMenuDNDObserver.mCurrentDragOverTarget = null;
    BookmarksMenuDNDObserver.onDragCloseTarget();

#   if the user types escape, we need to remove the feedback
    BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode);

    aEvent.target.removeEventListener("mousemove", BookmarksMenuController.onMouseMove, false)
  },

  /////////////////////////////////////////////////////////////////////////////
  // returns the formatted selection from aNode
  getBTSelection: function (aNode)
  {
    var item;
    switch (aNode.id) {
    case "bookmarks-ptf":
      item = BMSVC.getBookmarksToolbarFolder().Value;
      break;
    case "bookmarks-menu":
      item = "NC:BookmarksRoot";
      break;
    default:
      item = aNode.id;
      if (!this.isBTBookmark(item))
        return {length:0};
    }
    var parent           = this.getBTContainer(aNode);
    var isExpanded       = aNode.hasAttribute("open") && aNode.open;
    var selection        = {};
    selection.item       = [RDF.GetResource(item)];
    selection.parent     = [RDF.GetResource(parent)];
    selection.isExpanded = [isExpanded];
    selection.length     = selection.item.length;
    BookmarksUtils.checkSelection(selection);
    return selection;
  },

  /////////////////////////////////////////////////////////////////////////
  // returns the insertion target from aNode
  getBTTarget: function (aNode, aOrientation)
  {
    var item, parent, index;
    switch (aNode.id) {
    case "bookmarks-ptf":
      parent = BMSVC.getBookmarksToolbarFolder().Value;
      item = BookmarksToolbar.getLastVisibleBookmark();
      break;
    case "bookmarks-menu":
      parent = "NC:BookmarksRoot";
      break;
    case "bookmarks-chevron":
      parent = BMSVC.getBookmarksToolbarFolder().Value;
      break;
    default:
      if (aOrientation == BookmarksUtils.DROP_ON)
        parent = aNode.id
      else {
        parent = this.getBTContainer(aNode);
        item = aNode;
      }
    }

    parent = RDF.GetResource(parent);
    if (aOrientation == BookmarksUtils.DROP_ON)
      return BookmarksUtils.getTargetFromFolder(parent);

    item = RDF.GetResource(item.id);
    RDFC.Init(BMDS, parent);
    index = RDFC.IndexOf(item);
    if (aOrientation == BookmarksUtils.DROP_AFTER)
      ++index;

    return { parent: parent, index: index };
  },

  /////////////////////////////////////////////////////////////////////////
  // returns the parent resource of a node in the personal toolbar.
  // this is determined by inspecting the source element and walking up the 
  // DOM tree to find the appropriate containing node.
  getBTContainer: function (aNode)
  {
    var parent;
    var item = aNode.id;
    if (!this.isBTBookmark(item))
      return "NC:BookmarksRoot"
    parent = aNode.parentNode.parentNode;
    parent = parent.id;
    switch (parent) {
    case "bookmarks-chevron":
    case "bookmarks-stack":
    case "bookmarks-toolbar":
      return BMSVC.getBookmarksToolbarFolder().Value;
    case "bookmarks-menu":
      return "NC:BookmarksRoot";
    default:
      return parent;
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // returns true if the node is a bookmark, a folder or a bookmark separator
  isBTBookmark: function (aURI)
  {
    if (!aURI)
      return false;
    var type = BookmarksUtils.resolveType(aURI);
    return (type == "BookmarkSeparator"     ||
            type == "Bookmark"              ||
            type == "Folder"                ||
            type == "PersonalToolbarFolder" ||
            type == "Livemark"              ||
            aURI == "bookmarks-ptf")
  },

  /////////////////////////////////////////////////////////////////////////
  // returns true if the node is a container. -->
  isBTContainer: function (aTarget)
  {
    return  aTarget.localName == "menu" || (aTarget.localName == "toolbarbutton" &&
           (aTarget.getAttribute("container") == "true"));
  },

  /////////////////////////////////////////////////////////////////////////
  // returns BookmarksUtils.DROP_BEFORE, DROP_ON or DROP_AFTER accordingly
  // to the event coordinates. Skin authors could break us, we'll cross that 
  // bridge when they turn us 90degrees.  -->
  getBTOrientation: function (aEvent, aTarget)
  {
    var target
    if (!aTarget)
      target = aEvent.target;
    else
      target = aTarget;
    if (target.localName == "menu"                 &&
        target.parentNode.localName != "menupopup" ||
        target.id == "bookmarks-chevron")
      return BookmarksUtils.DROP_ON;
    if (target.id == "bookmarks-ptf") {
      return target.hasChildNodes()?
             BookmarksUtils.DROP_AFTER:BookmarksUtils.DROP_ON;
    }

    var overButtonBoxObject = target.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);
    var overParentBoxObject = target.parentNode.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);

    var size, border;
    var coordValue, clientCoordValue;
    switch (target.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        size = overButtonBoxObject.width;
        coordValue = overButtonBoxObject.x;
        clientCoordValue = aEvent.clientX;
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        size = overButtonBoxObject.height;
        coordValue = overButtonBoxObject.y-overParentBoxObject.y;
        clientCoordValue = aEvent.clientY;
        break;
      default: return BookmarksUtils.DROP_ON;
    }
    if (this.isBTContainer(target))
      if (target.localName == "toolbarbutton") {
        // the DROP_BEFORE area excludes the label
        var iconNode = document.getAnonymousElementByAttribute(target, "class", "toolbarbutton-icon");
        border = parseInt(document.defaultView.getComputedStyle(target,"").getPropertyValue("padding-left")) +
                 parseInt(document.defaultView.getComputedStyle(iconNode     ,"").getPropertyValue("width"));
        border = Math.min(size/5,Math.max(border,4));
      } else
        border = size/5;
    else
      border = size/2;

    // in the first region?
    if (clientCoordValue-coordValue < border)
      return BookmarksUtils.DROP_BEFORE;
    // in the last region?
    else if (clientCoordValue-coordValue >= size-border)
      return BookmarksUtils.DROP_AFTER;
    else // must be in the middle somewhere
      return BookmarksUtils.DROP_ON;
  },

  /////////////////////////////////////////////////////////////////////////
  // expand the folder targeted by the context menu.
  expandBTFolder: function ()
  {
    var target = document.popupNode.lastChild;
    if (document.popupNode.open)
      target.hidePopup();
    else
      target.showPopup(document.popupNode);
  },

  onCommandUpdate: function ()
  {
    var selection = this._selection;
    var target    = this._target;
    BookmarksController.onCommandUpdate(selection, target);
    if (document.popupNode.id == "bookmarks-ptf") {
      // disabling 'cut' and 'copy' on the empty area of the personal toolbar
      var commandNode = document.getElementById("cmd_cut");
      commandNode.setAttribute("disabled", "true");
      commandNode = document.getElementById("cmd_copy");
      commandNode.setAttribute("disabled", "true");
    }
  },

  ///////////////////////////////////////////////////////////////
  // Load a bookmark in menus or toolbar buttons
  // aTarget may not the aEvent target (see Open in tabs command)
  loadBookmark: function (aEvent, aTarget, aDS)
  {
    if (aTarget.getAttribute("class") == "openintabs-menuitem")
      aTarget = aTarget.parentNode.parentNode;
      
    // Check for invalid bookmarks (most likely a static menu item like "Manage Bookmarks")
    if (!this.isBTBookmark(aTarget.id))
      return;
    var rSource   = RDF.GetResource(aTarget.id);
    var selection = BookmarksUtils.getSelectionFromResource(rSource);
    var browserTarget = whereToOpenLink(aEvent);
    BookmarksCommand.openBookmark(selection, browserTarget, aDS);
    aEvent.preventBubble();
  },

  ////////////////////////////////////////////////
  // loads a bookmark with the mouse middle button
  loadBookmarkMiddleClick: function (aEvent, aDS)
  {
    if (aEvent.button != 1)
      return;
    // unlike for command events, we have to close the menus manually
    BookmarksMenuDNDObserver.mCurrentDragOverTarget = null;
    BookmarksMenuDNDObserver.onDragCloseTarget();
    this.loadBookmark(aEvent, aEvent.target, aDS);
  }
}

var BookmarksMenuController = {

  supportsCommand: BookmarksController.supportsCommand,

  isCommandEnabled: function (aCommand)
  {
    var selection = BookmarksMenu._selection;
    var target    = BookmarksMenu._target;
    if (selection)
      return BookmarksController.isCommandEnabled(aCommand, selection, target);
    return false;
  },

  doCommand: function (aCommand)
  {
#   we needed to focus the element that has the bm command controller
#   to get here. Now, let''s focus the content before performing the command:
#   if a modal dialog is called from now, the content will be focused again
#   automatically after dismissing the dialog
    if (content)
      content.focus();
    BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode);

#   if a dialog opens, the "open" attribute of a menuitem-container
#   rclicked on won''t be removed. We do it manually.
    var element = document.popupNode.firstChild;
    if (element && element.localName == "menupopup")
      element.hidePopup();

    var selection = BookmarksMenu._selection;
    var target    = BookmarksMenu._target;
    switch (aCommand) {
    case "cmd_bm_expandfolder":
      BookmarksMenu.expandBTFolder();
      break;
    default:
      BookmarksController.doCommand(aCommand, selection, target);
    }
  },

  onMouseMove: function (aEvent)
  {
    var command = aEvent.target.getAttribute("command");
    var isDisabled = aEvent.target.getAttribute("disabled")
    if (isDisabled != "true" && (command == "cmd_bm_newfolder" || command == "cmd_paste")) {
      BookmarksMenuDNDObserver.onDragSetFeedBack(document.popupNode, BookmarksMenu._orientation);
    } else {
      BookmarksMenuDNDObserver.onDragRemoveFeedBack(document.popupNode);
    }
  }
}

var BookmarksMenuDNDObserver = {

  ////////////////////
  // Public methods //
  ////////////////////

  onDragStart: function (aEvent, aXferData, aDragAction)
  {
    var target = aEvent.target;

    // Prevent dragging from an invalid region
    if (!this.canDrop(aEvent))
      return;

    // Prevent dragging out of menupopups on non Win32 platforms. 
    // a) on Mac drag from menus is generally regarded as being satanic
    // b) on Linux, this causes an X-server crash, (bug 151336)
    // c) on Windows, there is no hang or crash associated with this, so we'll leave 
    // the functionality there. 
    if (navigator.platform != "Win32" && target.localName != "toolbarbutton")
      return;

    // bail if dragging from the empty area of the bookmarks toolbar
    if (target.id == "bookmarks-ptf")
      return

    // a drag start is fired when leaving an open toolbarbutton(type=menu) 
    // (see bug 143031)
    if (this.isContainer(target)) {
      if (this.isPlatformNotSupported) 
        return;
      if (!aEvent.shiftKey && !aEvent.altKey && !aEvent.ctrlKey)
        return;
      // menus open on mouse down
      target.firstChild.hidePopup();
    }
    var selection  = BookmarksMenu.getBTSelection(target);
    aXferData.data = BookmarksUtils.getXferDataFromSelection(selection);
  },

  onDragOver: function(aEvent, aFlavour, aDragSession) 
  {
    var orientation = BookmarksMenu.getBTOrientation(aEvent)
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent.target, orientation);
    if (orientation != this.mCurrentDropPosition) {
      // emulating onDragExit and onDragEnter events since the drop region
      // has changed on the target.
      this.onDragExit(aEvent, aDragSession);
      this.onDragEnter(aEvent, aDragSession);
    }
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported)
      return;
    this.onDragOverCheckTimers();
  },

  onDragEnter: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    var orientation = BookmarksMenu.getBTOrientation(aEvent);
    if (target.localName == "menupopup" || target.id == "bookmarks-ptf")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(target, orientation);
      this.onDragEnterSetTimer(target, aDragSession);
    }
    this.mCurrentDragOverTarget = target;
    this.mCurrentDropPosition   = orientation;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    if (target.localName == "menupopup" || target.id == "bookmarks-ptf")
      target = target.parentNode;
    this.onDragRemoveFeedBack(target);
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    this.mCurrentDropPosition = null;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var target = aEvent.target;
    this.onDragRemoveFeedBack(target);

    var selection = BookmarksUtils.getSelectionFromXferData(aDragSession);

    var orientation = BookmarksMenu.getBTOrientation(aEvent);

    // For RTL PersonalBar bookmarks buttons, orientation should be inverted (only in drop case)
    var PBStyle = window.getComputedStyle(document.getElementById("PersonalToolbar"),'');
    var isHorizontal = (target.localName == "toolbarbutton");    if ((PBStyle.direction == 'rtl') && isHorizontal)      if (orientation == BookmarksUtils.DROP_AFTER)        orientation = BookmarksUtils.DROP_BEFORE;      else if (orientation == BookmarksUtils.DROP_BEFORE)        orientation = BookmarksUtils.DROP_AFTER;

    var selTarget   = BookmarksMenu.getBTTarget(target, orientation);

    const kDSIID      = Components.interfaces.nsIDragService;
    const kCopyAction = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;

    // hide the 'open in tab' menuseparator because bookmarks
    // can be inserted after it if they are dropped after the last bookmark
    // a more comprehensive fix would be in the menupopup template builder
    var menuSeparator = null;
    var menuTarget = (target.localName == "toolbarbutton" ||
                      target.localName == "menu")         && 
                     orientation == BookmarksUtils.DROP_ON?
                     target.lastChild:target.parentNode;
    if (menuTarget.hasChildNodes() &&
        menuTarget.lastChild.getAttribute("class") == "openintabs-menuitem") {
      menuSeparator = menuTarget.lastChild.previousSibling;
      menuTarget.removeChild(menuSeparator);
    }

    if (aDragSession.dragAction & kCopyAction)
      BookmarksUtils.insertAndCheckSelection("drag", selection, selTarget);
    else
      BookmarksUtils.moveAndCheckSelection("drag", selection, selTarget);

    // show again the menuseparator
    if (menuSeparator)
      menuTarget.insertBefore(menuSeparator, menuTarget.lastChild);

  },

  canDrop: function (aEvent, aDragSession)
  {
    var target = aEvent.target;
    return BookmarksMenu.isBTBookmark(target.id)       && 
           target.id != "NC:SystemBookmarksStaticRoot" &&
           target.id.substring(0,5) != "find:"         ||
           target.id == "bookmarks-menu"               ||
           target.id == "bookmarks-chevron"            ||
           target.id == "bookmarks-ptf";
  },

  canHandleMultipleItems: true,

  getSupportedFlavours: function () 
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("moz/rdfitem");
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
  }, 
  

  ////////////////////////////////////
  // Private methods and properties //
  ////////////////////////////////////

  springLoadedMenuDelay: 350, // milliseconds
  isPlatformNotSupported: navigator.platform.indexOf("Mac") != -1, // see bug 136524
  isTimerSupported: navigator.platform.indexOf("Win") == -1,

  mCurrentDragOverTarget: null,
  mCurrentDropPosition: null,
  loadTimer  : null,
  closeTimer : null,
  loadTarget : null,
  closeTarget: null,

  _observers : null,
  get mObservers ()
  {
    if (!this._observers) {
      this._observers = [
        document.getElementById("bookmarks-ptf"),
#       menubar menus haven''t an "open" attribute: we can take the child
        document.getElementById("bookmarks-menu").firstChild,
        document.getElementById("bookmarks-chevron").parentNode
      ]
    }
    return this._observers;
  },

  getObserverForNode: function (aNode)
  {
    if (!aNode)
      return null;
    var node = aNode;
    var observer;
    while (node) {
      for (var i=0; i < this.mObservers.length; i++) {
        observer = this.mObservers[i];
        if (observer == node)
          return observer;
      }
      node = node.parentNode;
    }
    return null;
  },

  onDragCloseMenu: function (aNode)
  {
    var children = aNode.childNodes;
    for (var i = 0; i < children.length; i++) {
      if (this.isContainer(children[i]) && 
          children[i].getAttribute("open") == "true") {
        this.onDragCloseMenu(children[i].lastChild);
        if (children[i] != this.mCurrentDragOverTarget || this.mCurrentDropPosition != BookmarksUtils.DROP_ON)
          children[i].lastChild.hidePopup();
      }
    } 
  },

  onDragCloseTarget: function ()
  {
    var currentObserver = this.getObserverForNode(this.mCurrentDragOverTarget);
    // close all the menus not hovered by the mouse
    for (var i=0; i < this.mObservers.length; i++) {
      if (currentObserver != this.mObservers[i]) {
        this.onDragCloseMenu(this.mObservers[i]);
        if (this.mObservers[i].parentNode.id == "bookmarks-menu")
          this.mObservers[i].hidePopup();
      } else
        this.onDragCloseMenu(this.mCurrentDragOverTarget.parentNode);
    }
  },

  onDragLoadTarget: function (aTarget) 
  {
    if (!this.mCurrentDragOverTarget)
      return;
    // Load the current menu
    if (this.mCurrentDropPosition == BookmarksUtils.DROP_ON && 
        this.isContainer(aTarget))
      aTarget.lastChild.showPopup(aTarget);
  },

  onDragOverCheckTimers: function ()
  {
    var now = new Date().getTime();
    if (this.closeTimer && now-this.springLoadedMenuDelay>this.closeTimer) {
      this.onDragCloseTarget();
      this.closeTimer = null;
    }
    if (this.loadTimer && (now-this.springLoadedMenuDelay>this.loadTimer)) {
      this.onDragLoadTarget(this.loadTarget);
      this.loadTimer = null;
    }
  },

  onDragEnterSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    if (this.isTimerSupported) {
      var targetToBeLoaded = aTarget;
      clearTimeout(this.loadTimer);
      if (aTarget == aDragSession.sourceNode)
        return;
      var This = this;
      this.loadTimer=setTimeout(function () {This.onDragLoadTarget(targetToBeLoaded)}, This.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.loadTimer  = now;
      this.loadTarget = aTarget;
    }
  },

  onDragExitSetTimer: function (aTarget, aDragSession)
  {
    if (this.isPlatformNotSupported)
      return;
    var This = this;
    if (this.isTimerSupported) {
      clearTimeout(this.closeTimer)
      this.closeTimer=setTimeout(function () {This.onDragCloseTarget()}, This.springLoadedMenuDelay);
    } else {
      var now = new Date().getTime();
      this.closeTimer  = now;
      this.closeTarget = aTarget;
      this.loadTimer = null;

      // If user isn't rearranging within the menu, close it
      // To do so, we exploit a Mac bug: timeout set during
      // drag and drop on Windows and Mac are fired only after that the drop is released.
      // timeouts will pile up, we may have a better approach but for the moment, this one
      // correctly close the menus after a drop/cancel outside the personal toolbar.
      // The if statement in the function has been introduced to deal with rare but reproducible
      // missing Exit events.
      if (aDragSession.sourceNode.localName != "menuitem" && aDragSession.sourceNode.localName != "menu")
        setTimeout(function () { if (This.mCurrentDragOverTarget) {This.onDragRemoveFeedBack(This.mCurrentDragOverTarget); This.mCurrentDragOverTarget=null} This.loadTimer=null; This.onDragCloseTarget() }, 0);
    }
  },

  onDragSetFeedBack: function (aTarget, aOrientation)
  {
   switch (aTarget.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-left", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-right", "true");
            break;
          case BookmarksUtils.DROP_ON:
            aTarget.setAttribute("dragover-top"   , "true");
            aTarget.setAttribute("dragover-bottom", "true");
            aTarget.setAttribute("dragover-left"  , "true");
            aTarget.setAttribute("dragover-right" , "true");
            break;
        }
        break;
      case "menuseparator": 
      case "menu":
      case "menuitem":
        switch (aOrientation) {
          case BookmarksUtils.DROP_BEFORE: 
            aTarget.setAttribute("dragover-top", "true");
            break;
          case BookmarksUtils.DROP_AFTER:
            aTarget.setAttribute("dragover-bottom", "true");
            break;
          case BookmarksUtils.DROP_ON:
            break;
        }
        break;
      case "hbox"     : 
        // hit between the last visible bookmark and the chevron
        var newTarget = BookmarksToolbar.getLastVisibleBookmark();
        if (newTarget)
          newTarget.setAttribute("dragover-right", "true");
        break;
      case "stack"    :
      case "menupopup": break; 
     default: dump("No feedback for: "+aTarget.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function (aTarget)
  { 
    var newTarget;
    var bt;
    if (aTarget.id == "bookmarks-ptf") { 
      // hit when dropping in the bt or between the last visible bookmark 
      // and the chevron
      newTarget = BookmarksToolbar.getLastVisibleBookmark();
      if (newTarget)
        newTarget.removeAttribute("dragover-right");
    } else if (aTarget.id == "bookmarks-stack") {
      newTarget = BookmarksToolbar.getLastVisibleBookmark();
      newTarget.removeAttribute("dragover-right");
    } else {
      aTarget.removeAttribute("dragover-left");
      aTarget.removeAttribute("dragover-right");
      aTarget.removeAttribute("dragover-top");
      aTarget.removeAttribute("dragover-bottom");
    }
  },

  onDropSetFeedBack: function (aTarget)
  {
    //XXX Not yet...
  },

  isContainer: function (aTarget)
  {
    return aTarget.localName == "menu"          || 
           aTarget.localName == "toolbarbutton" &&
           aTarget.getAttribute("type") == "menu";
  }
}

var BookmarksToolbar = 
{
  /////////////////////////////////////////////////////////////////////////////
  // make bookmarks toolbar act like menus
  openedMenuButton:null,
  autoOpenMenu: function (aEvent)
  {
    var target = aEvent.target;
    if (BookmarksToolbar.openedMenuButton != target &&
        target.nodeName == "toolbarbutton" &&
        target.type == "menu") {
      BookmarksToolbar.openedMenuButton.open = false;
      target.open = true;
    }
  },
  setOpenedMenu: function (aEvent)
  {
    if (aEvent.target.parentNode.localName == 'toolbarbutton') {
      if (!this.openedMenuButton)
        aEvent.currentTarget.addEventListener("mouseover", this.autoOpenMenu, true);
      this.openedMenuButton = aEvent.target.parentNode;
    }
  },
  unsetOpenedMenu: function (aEvent)
  {
    if (aEvent.target.parentNode.localName == 'toolbarbutton') {
      aEvent.currentTarget.removeEventListener("mouseover", this.autoOpenMenu, true);
      this.openedMenuButton = null;
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // returns the node of the last visible bookmark on the toolbar -->
  getLastVisibleBookmark: function ()
  {
    var buttons = document.getElementById("bookmarks-ptf");
    var button = buttons.firstChild;
    if (!button)
      return null; // empty bookmarks toolbar
    do {
      if (button.collapsed)
        return button.previousSibling;
      button = button.nextSibling;
    } while (button)
    return buttons.lastChild;
  },

  updateOverflowMenu: function (aMenuPopup)
  {
    var hbox = document.getElementById("bookmarks-ptf");
    for (var i = 0; i < hbox.childNodes.length; i++) {
      var button = hbox.childNodes[i];
      var menu = aMenuPopup.childNodes[i];
      if (menu.collapsed == button.collapsed)
        menu.collapsed = !menu.collapsed;
    }
  },

  resizeFunc: function(event) 
  { 
    var buttons = document.getElementById("bookmarks-ptf");
    if (!buttons)
      return;
    var chevron = document.getElementById("bookmarks-chevron");
    var width = window.innerWidth;
    var myToolbar = buttons.parentNode.parentNode.parentNode;
    for (var i = myToolbar.childNodes.length-1; i >= 0; i--){
      var anItem = myToolbar.childNodes[i];
      if (anItem.id == "personal-bookmarks") {
        break;
      }
      width -= anItem.boxObject.width;
    }
    var chevronWidth = 0;
    chevron.collapsed = false;
    chevronWidth = chevron.boxObject.width;
    chevron.collapsed = true;
    var overflowed = false;

    var isLTR=window.getComputedStyle(document.getElementById("PersonalToolbar"),'').direction=='ltr';

    for (var i=0; i<buttons.childNodes.length; i++) {
      var button = buttons.childNodes[i];
      button.collapsed = overflowed;
      
      if (i == buttons.childNodes.length - 1) // last ptf item...
        chevronWidth = 0;
      var offset = isLTR ? button.boxObject.x 
                         : width - button.boxObject.x;
      if (offset + button.boxObject.width + chevronWidth > width) {
         overflowed = true;
        // This button doesn't fit. Show it in the menu. Hide it in the toolbar.
        if (!button.collapsed)
          button.collapsed = true;
        if (chevron.collapsed) {
          chevron.collapsed = false;
          var overflowPadder = document.getElementById("overflow-padder");
          offset = isLTR ? buttons.boxObject.x 
                         : width - buttons.boxObject.x - buttons.boxObject.width;
          overflowPadder.width = width - chevron.boxObject.width - offset;
        }
      }
    }
    BookmarksToolbarRDFObserver._overflowTimerInEffect = false;
  },

  // Fill in tooltips for personal toolbar
  fillInBTTooltip: function (tipElement)
  {

    var title = tipElement.label;
    var url = tipElement.statusText;

    if (!title && !url) {
      // bail out early if there is nothing to show
      return false;
    }

    var tooltipTitle = document.getElementById("btTitleText");
    var tooltipUrl = document.getElementById("btUrlText"); 
    if (title && title != url) {
      tooltipTitle.removeAttribute("hidden");
      tooltipTitle.setAttribute("value", title);
    } else  {
      tooltipTitle.setAttribute("hidden", "true");
    }
    if (url) {
      tooltipUrl.removeAttribute("hidden");
      tooltipUrl.setAttribute("value", url);
    } else {
      tooltipUrl.setAttribute("hidden", "true");
    }
    return true; // show tooltip
  }
}

// Implement nsIRDFObserver so we can update our overflow state when items get
// added/removed from the toolbar
var BookmarksToolbarRDFObserver =
{
  onAssert: function (aDataSource, aSource, aProperty, aTarget)
  {
    if (aProperty.Value == NC_NS+"BookmarksToolbarFolder") {
      var bt = document.getElementById("bookmarks-ptf");
      if (bt) {
        bt.ref = aSource.Value;
        document.getElementById("bookmarks-chevron").ref = aSource.Value;
      }
    }
    this.setOverflowTimeout(aSource, aProperty);
  },
  onUnassert: function (aDataSource, aSource, aProperty, aTarget)
  {
    this.setOverflowTimeout(aSource, aProperty);
  },
  onChange: function (aDataSource, aSource, aProperty, aOldTarget, aNewTarget) {},
  onMove: function (aDataSource, aOldSource, aNewSource, aProperty, aTarget) {},
  onBeginUpdateBatch: function (aDataSource) {},
  onEndUpdateBatch: function (aDataSource)
  {
    this._overflowTimerInEffect = true;
    setTimeout(BookmarksToolbar.resizeFunc, 0);
  },
  _overflowTimerInEffect: false,
  setOverflowTimeout: function (aSource, aProperty)
  {
    if (this._overflowTimerInEffect)
      return;
    if (aSource != BMSVC.getBookmarksToolbarFolder()
        || aProperty.Value == NC_NS+"LastModifiedDate")
      return;
    this._overflowTimerInEffect = true;
    setTimeout(BookmarksToolbar.resizeFunc, 0);
  }
}
