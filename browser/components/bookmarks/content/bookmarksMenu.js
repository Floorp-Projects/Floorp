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
  _db:null,

  /////////////////////////////////////////////////////////////////////////////
  // prepare the bookmarks menu for display
  onShowMenu: function (aTarget)
  {
    this.showOpenInTabsMenuItem(aTarget);
    this.showEmptyItem(aTarget);
  },

  /////////////////////////////////////////////////////////////////////////////
  // remove arbitary elements created in this.onShowMenu()
  onHideMenu: function (aTarget)
  {
    this.hideOpenInTabsMenuItem(aTarget);
    this.hideEmptyItem(aTarget);
  },

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

  /////////////////////////////////////////////////////////////////////////////
  // show an empty item if the menu is empty
  showEmptyItem: function (aTarget)
  {
    if(aTarget.hasChildNodes())
      return;

    var EmptyMsg = BookmarksUtils.getLocaleString("emptyFolder");
    var emptyElement = document.createElementNS(XUL_NS, "menuitem");
    emptyElement.setAttribute("id", "empty-menuitem");
    emptyElement.setAttribute("label", EmptyMsg);
    emptyElement.setAttribute("disabled", "true");

    aTarget.appendChild(emptyElement);
  },

  /////////////////////////////////////////////////////////////////////////////
  // remove the empty element
  hideEmptyItem: function (aTarget)
  {
    if (!aTarget.hasChildNodes())
      return;

    // if the user drags to the menu while it's open (i.e. on the toolbar),
    // the bookmark gets added either before or after the Empty menu item
    // before the menu is hidden.  So we need to test both first and last.
    if (aTarget.firstChild.id == "empty-menuitem")
      aTarget.removeChild(aTarget.firstChild);
    else if (aTarget.lastChild.id == "empty-menuitem")
      aTarget.removeChild(aTarget.lastChild);
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

    this._selection = this.getBTSelection(target);
    this._target    = this.getBTTarget(target, "after");
  
    // walk up the tree until we find a database node
    var p = target;
    while (p && !p.database)
      p = p.parentNode;

    this._db = p? p.database : null;
  
    BookmarksCommand.createContextMenu(aEvent, this._selection, this._db);
    this.onCommandUpdate();
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
    BookmarksMenuDNDObserver.onDragRemoveFeedBack();

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

  getBTOrientation: function (aTarget, aEvent)
  {
    switch (aTarget.id) {
    case "bookmarks-ptf":
    case "bookmarks-menu":
    case "bookmarks-chevron":
      return "on";
    default: 
      return BookmarksInsertion.getOrientation(aTarget, aEvent);
    }
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
      if (aOrientation == "on")
        parent = aNode.id
      else {
        parent = this.getBTContainer(aNode);
        item = aNode;
      }
    }

    parent = RDF.GetResource(parent);
    if (aOrientation == "on")
      return BookmarksUtils.getTargetFromFolder(parent);

    item = RDF.GetResource(item.id);
    RDFC.Init(BMDS, parent);
    index = RDFC.IndexOf(item);
    if (aOrientation == "after")
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
      
    // Check for invalid bookmarks (ex: a static menu item like "Manage Bookmarks")
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

#   if a dialog opens, the "open" attribute of a menuitem-container
#   rclicked on won''t be removed. We do it manually.
    var element = document.popupNode.firstChild;
    if (element && element.localName == "menupopup")
      element.hidePopup();

    // hide the 'open in tab' menuseparator because bookmarks
    // can be inserted after it if they are dropped after the last bookmark
    // a more comprehensive fix would be in the menupopup template builder
    var menuSeparator = null;
    var menuTarget = document.popupNode.parentNode;
    if (menuTarget.hasChildNodes() &&
        menuTarget.lastChild.getAttribute("class") == "openintabs-menuitem") {
      menuSeparator = menuTarget.lastChild.previousSibling;
      menuTarget.removeChild(menuSeparator);
    }

    var selection = BookmarksMenu._selection;
    var target    = BookmarksMenu._target;
    switch (aCommand) {
    case "cmd_bm_expandfolder":
      BookmarksMenu.expandBTFolder();
      break;
    default:
      BookmarksController.doCommand(aCommand, selection, target);
    }

    // show again the menuseparator
    if (menuSeparator)
      menuTarget.insertBefore(menuSeparator, menuTarget.lastChild);
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
      return;

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
    var orientation = BookmarksMenu.getBTOrientation(aEvent.target, aEvent);
    if (aDragSession.canDrop)
      this.onDragSetFeedBack(aEvent.target, aEvent);
    if (orientation != this.mCurrentDropPosition) {
      // emulating onDragExit and onDragEnter events since the drop region
      // has changed on the target (Ex: orientation change in a toolbarbutton
      // container).
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
    var orientation = BookmarksMenu.getBTOrientation(aEvent.target, aEvent);
    if (target.localName == "menupopup" || target.id == "bookmarks-ptf")
      target = target.parentNode;
    if (aDragSession.canDrop) {
      this.onDragSetFeedBack(target, aEvent);
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
    this.onDragRemoveFeedBack();
    this.onDragExitSetTimer(target, aDragSession);
    this.mCurrentDragOverTarget = null;
    this.mCurrentDropPosition = null;
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var target = aEvent.target;
    this.onDragRemoveFeedBack();

    var selection = BookmarksUtils.getSelectionFromXferData(aDragSession);
    var orientation = BookmarksMenu.getBTOrientation(target, aEvent);

    var selTarget   = BookmarksMenu.getBTTarget(target, orientation);

    const kDSIID      = Components.interfaces.nsIDragService;
    const kCopyAction = kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_LINK;

    // hide the 'open in tab' menuseparator because bookmarks
    // can be inserted after it if they are dropped after the last bookmark
    // a more comprehensive fix would be in the menupopup template builder
    var menuSeparator = null;
    var menuTarget = (target.localName == "toolbarbutton" ||
                      target.localName == "menu")         && 
                     orientation == "on"?
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
        if (children[i] != this.mCurrentDragOverTarget || this.mCurrentDropPosition != "on")
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
    if (this.mCurrentDropPosition == "on" && 
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
        setTimeout(function () { if (This.mCurrentDragOverTarget) {This.onDragRemoveFeedBack(); This.mCurrentDragOverTarget=null} This.loadTimer=null; This.onDragCloseTarget() }, 0);
    }
  },

  onDragSetFeedBack: function (aTarget, aEvent)
  {

    switch (aTarget.localName) {
      case "toolbarseparator":
      case "toolbarbutton":
      case "menuseparator": 
      case "menu":
      case "menuitem":
        var orientation = BookmarksMenu.getBTOrientation(aTarget, aEvent);
        BookmarksInsertion.showWithOrientation(aTarget, orientation);
        break;
      case "hbox": 
        // hit between the last visible bookmark and the chevron
        var target = BookmarksToolbar.getLastVisibleBookmark();
        if (target)
          BookmarksInsertion.showAfter(target);
        break;
      case "stack"    :
      case "menupopup":
        BookmarksInsertion.hide()
        break; 
      default: dump("No feedback for: "+aTarget.localName+"\n");
    }
  },

  onDragRemoveFeedBack: function ()
  { 
    BookmarksInsertion.hide();
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
  // make bookmarks toolbar act like menubar
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
  // returns the node of the last visible bookmark on the toolbar
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

    var isLTR=window.getComputedStyle(document.getElementById("PersonalToolbar"),"").direction=="ltr";

    for (i=0; i<buttons.childNodes.length; i++) {
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


/////////////////////////////////////////////////////////////////////////////
// this should be an XBL widget when we have xul element absolute positioning
/////////////////////////////////////////////////////////////////////////////

var BookmarksInsertion = 
{

  mDefaultSize: 9,
  mTarget: null,
  mOrientation: null,

  /////////////////////////////////////////////////////////////////////////
  // returns true if the node is a container.
  isContainer: function (aTarget)
  {
    return aTarget.localName == "menu"          || 
           aTarget.localName == "toolbarbutton" &&
           aTarget.getAttribute("type") == "menu";
  },

  //////////////////////////////////////////////////////////////////////////
  // returns "before", "on" or "after" accordingly to the mouse position
  // relative to the target.
  // the orientation refers to the DOM (and is rtl-direction aware)
  getOrientation: function (aTarget, aEvent)
  {

    var size, delta;
    var boxOrientation = document.defaultView
                                 .getComputedStyle(aTarget.parentNode,"")
                                 .getPropertyValue("-moz-box-orient");
    if (boxOrientation == "horizontal") {
      size  = aTarget.boxObject.width;
      delta = aTarget.boxObject.x+size/2-aEvent.clientX;
      var LTR = document.defaultView
                        .getComputedStyle(aTarget.parentNode,"").direction;
      if (LTR != "ltr")
        delta = -delta;
    } else {
      size  = aTarget.boxObject.height;
      delta = aTarget.boxObject.screenY+size/2-aEvent.screenY;
    }

    if (this.isContainer(aTarget)) {
      if (Math.abs(delta) < size*0.3)
        return "on";
    }
    
    return delta<0? "after" : "before";

  },

  showWithOrientation: function (aTarget, aOrientation)
  {
    
    if (aTarget == this.mTarget && aOrientation == this.mOrientation)
      return;

    this.hide();

    // special case for the menu items...
    if (aTarget.localName == "menu"       ||
        aTarget.localName == "menuitem"   ||
        aTarget.localName == "menuseparator") {
      this.showMenuitem(aTarget, aOrientation);
      return;
    }

    this.mTarget = aTarget;
    this.mOrientation = aOrientation;

    var insertion = document.getElementById("insertion-feedback");

    if (aOrientation == "on") {
      insertion.hidden = true;
      return;      
    }

    var boxOrient = document.defaultView.getComputedStyle(aTarget.parentNode,"")
                            .getPropertyValue("-moz-box-orient")
    var insertionWidth, insertionHeight, insertionLeft, insertionTop;
    var targetWidth  = aTarget.boxObject.width;
    var targetHeight = aTarget.boxObject.height;

    if (boxOrient == "horizontal") {
      insertionWidth  = this.mDefaultSize;
      insertionHeight = targetHeight;
      insertionLeft = aTarget.boxObject.x - aTarget.parentNode.boxObject.x - 
                      this.mDefaultSize/2
      var ltr = document.defaultView
                        .getComputedStyle(aTarget.parentNode,"").direction;
      if ((ltr == "rtl") ^ (aOrientation == "after"))
        insertionLeft += targetWidth;
      insertionTop = 0;
    } else {
      // not tested...
      insertionWidth  = targetWidth;
      insertionHeight = this.mDefaultSize;
      insertionLeft = 0;
      insertionTop  = aTarget.boxObject.y - aTarget.parentNode.boxObject.y - 
                      this.mDefaultSize/2
      if (aOrientation == "after")
        insertionTop += targetHeight;
    }
    insertion.hidden = false;
    insertion.style.left   = insertionLeft+"px";
    insertion.style.top    = insertionTop+"px";
    insertion.style.width  = insertionWidth+"px";
    insertion.style.height = insertionHeight+"px";
  },

  showBefore: function (aTarget) 
  {
    this.showWithOrientation(aTarget, "before");
  },

  showAfter: function (aTarget) 
  {
    this.showWithOrientation(aTarget, "after");
  },

  show: function(aTarget, aEvent)
  {
    var orientation = this.getOrientation(aTarget, aEvent);
    this.showWithOrientation(aTarget, orientation);
  },

  hide: function () 
  {
    if (!this.mTarget)
      return;

    // special case for the menu items...
    if (this.mTarget.localName == "menu"       ||
        this.mTarget.localName == "menuitem"   ||
        this.mTarget.localName == "menuseparator") {
      this.hideMenuitem();
      return;
    }

    var insertion = document.getElementById("insertion-feedback");
    insertion.hidden = true;
    this.mTarget = null;
    this.mOrientation = null;
  },

  //XXX To be removed when we have xul element absolute positioning
  showMenuitem: function(aTarget, aOrientation)
  {

    this.mTarget = aTarget;
    this.mOrientation = aOrientation;
    
    if (aOrientation == "before") {
      var previous = aTarget;
      do {
        previous = previous.previousSibling;
      } while (previous && previous.collapsed)
 
      if (previous) {
        aTarget = previous;
        aOrientation = "after";
      }
    }

    // menulist appearance is only implemented for gtk2
    // so don't bother for the others.
#ifdef XP_UNIX
#ifndef XP_MACOSX

    // for menuitems, we will toggle -moz-appearance between 'menuitem' and 
    // 'none' and will set the border color in the latter case to indicate
    // the DND feedback.
    // however, doing so may change the menuitem border width and we'll
    // have to correct for that. Unfortunately, getComputedStyle doesn't
    // return the actual border width of a "native" widget... (bug 256261)
    // big fat ugly hack will follow.

    // cache the menuitem border width. Beware: this is not the actual width of
    // a menuitem with '-moz-appearance:menuitem', but instead this is the 
    // width it would have if it would be styled with '-moz-appearance:none'...
    var borderLeft = document.defaultView.getComputedStyle(aTarget,"")
                             .getPropertyValue("border-left-width");
    var borderTop  = document.defaultView.getComputedStyle(aTarget,"")
                             .getPropertyValue("border-top-width");

    var oldWidth  = aTarget.boxObject.width;
    var oldHeight = aTarget.boxObject.height;
    var labelNode = document.getAnonymousElementByAttribute(aTarget, "class", "menu-iconic-text");
    var oldWidthL = labelNode.boxObject.width;

#endif
#endif

    switch (aOrientation) {
      case "before":
        aTarget.setAttribute("dragover-top", "true");
        break;
      case "after":
        aTarget.setAttribute("dragover-bottom", "true");
        break;
      case "on":
        break;
    }
    
#ifdef XP_UNIX
#ifndef XP_MACOSX
    var width  = aTarget.boxObject.width;
    var height = aTarget.boxObject.height;
    var widthL = labelNode.boxObject.width;
    borderTop  = parseInt(borderTop)  + (oldHeight-height)/2 + "px";
    borderLeft = parseInt(borderLeft) + (oldWidth -width + widthL-oldWidthL) /2 + "px";

    aTarget.style.setProperty("border-left-width"  , borderLeft, "important");
    aTarget.style.setProperty("border-right-width" , borderLeft, "important");
    aTarget.style.setProperty("border-bottom-width", borderTop,  "important");
    aTarget.style.setProperty("border-top-width"   , borderTop,  "important");
#endif
#endif

  },

  //XXX To be removed when we have xul element absolute positioning
  hideMenuitem: function()
  {
    var target = this.mTarget
    
    target.removeAttribute("dragover-top");
    target.removeAttribute("dragover-bottom");
    target = target.previousSibling
    // brute force. anyways this code should go away.
    if (target) {
      target.removeAttribute("dragover-top");
      target.removeAttribute("dragover-bottom");
    }
    this.mTarget = null;
    this.mOrientation = null;
  }

}
