/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

const VMGR_VURL_SCHEME = "x-vloc:";
const VMGR_SCHEME_LEN  = VMGR_VURL_SCHEME.length;

const VMGR_HIDDEN      = "hidden";
const VMGR_NEW         = "new";
const VMGR_MAINWINDOW  = "mainwindow";
const VMGR_GUTTER      = "gutter";

const VMGR_VURL_HIDDEN     = VMGR_VURL_SCHEME + "/" + VMGR_HIDDEN;
const VMGR_VURL_NEW        = VMGR_VURL_SCHEME + "/" + VMGR_NEW;
const VMGR_VURL_MAINWINDOW = VMGR_VURL_SCHEME + "/" + VMGR_MAINWINDOW;
const VMGR_VURL_GUTTER     = VMGR_VURL_MAINWINDOW + "/gutter";
const VMGR_DEFAULT_CONTAINER = "initial-container";

var VMGR_GUTTER_CONTAINER = VMGR_VURL_MAINWINDOW + "?target=container&" +
    "id=" + VMGR_GUTTER + "&type=vertical";

function ViewManager(commandManager, mainWindow)
{
    this.commandManager = commandManager;    
    this.floaterSequence = 0;
    this.containerSequence = 0;
    this.windows = new Object();
    this.mainWindow = mainWindow;
    this.windows[VMGR_MAINWINDOW] = mainWindow;
    this.views = new Object();
}

ViewManager.prototype.realizeViews =
function vmgr_realizeviews (views)
{
    for (var v in views)
        this.realizeView(views[v]);
}    

ViewManager.prototype.realizeView =
function vmgr_realizeview (view)
{
    var entry;
    var key = view.viewId;
    this.views[key] = view;
    if ("init" in view && typeof view.init == "function")
        view.init();
    
    var toggleName = "toggle-" + key;
    if (!(key in this.commandManager.commands))
    {
        entry = [toggleName, "toggle-view " + key, CMD_CONSOLE];
        
        if ("cmdary" in view)
            view.cmdary.unshift(entry);
        else
            view.cmdary = [entry];
    }
    
    if ("cmdary" in view)
    {
        var commands = this.commandManager.defineCommands(view.cmdary);
        for (var i in this.windows)
            this.commandManager.installKeys (this.windows[i].document, commands);
        view.commands = commands;
    }
    if ("hooks" in view)
        this.commandManager.addHooks (view.hooks, key);
}

ViewManager.prototype.unrealizeViews =
function vmgr_unrealizeviews (views)
{
    for (var v in views)
        this.unrealizeView(views[v]);
}    

ViewManager.prototype.unrealizeView =
function vmgr_unrealizeview (view)
{
    if ("commands" in view)
        this.commandManager.uninstallKeys(view.commands);
    if ("hooks" in view)
        this.commandManager.removeHooks (view.hooks, view.viewId);

    if ("destroy" in view && typeof view.destroy == "function")
        view.destroy();

    delete this.views[view.viewId];
}

ViewManager.prototype.getWindowById =
function vmgr_getwindow (windowId)
{
    if (windowId in this.windows)
        return this.windows[windowId];

    return null;
}

ViewManager.prototype.createWindow =
function vmgr_createwindow (windowId, cb)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        var cm = viewManager.commandManager;
        cm.installKeys (window.document, cm.commands);
        if (typeof cb == "function")
            cb(window);
    };
    
    if (!ASSERT(!(windowId in this.windows), "window " + windowId +
                " already exists"))
    {
        return null;
    }
    
    var win = openDialog ("chrome://venkman/content/venkman-floater.xul?id=" +
                          escape(windowId), "_blank",
                          "chrome,menubar,toolbar,resizable,dialog=no",
                          onWindowLoaded);
    this.windows[windowId] = win;
    return win;
}

ViewManager.prototype.destroyWindows =
function vmgr_destroywindows ()
{
    for (var w in this.windows)
    {
        if (w == VMGR_MAINWINDOW)
            this.destroyWindow(w);
        else
            this.windows[w].close();
    }
}

ViewManager.prototype.destroyWindow =
function vmgr_destroywindow (windowId)
{
    if (!ASSERT(windowId in this.windows, "unknown window id"))
        return;

    var document = this.windows[windowId].document;
    
    for (var viewId in this.views)
    {
        try
        {
            var view = this.views[viewId];
            
            if ("currentContent" in view &&
                view.currentContent.ownerDocument == document)
            {
                this.moveViewURL(VMGR_VURL_HIDDEN, viewId);
            }
        }
        catch (ex)
        {
            dd ("caught exception while moving view ``" + ex + "''");
        }
    }

    var container = document.getElementById (VMGR_DEFAULT_CONTAINER);
    var child = container.firstChild;
    while (child)
    {
        var next = child.nextSibling;
        container.removeChild (child);
        child = next;
    }

    if (windowId != VMGR_MAINWINDOW)
        delete this.windows[windowId];
}

ViewManager.prototype.createContainer =
function vmgr_createcontainer (parsedLocation, containerId, type)
{
    if (!type)
        type = "horizontal";
    
    var parentContainer = this.getLocation(parsedLocation);
    
    if (!ASSERT(parentContainer, "location is hidden or does not exist"))
        return null;

    var document = parentContainer.ownerDocument;
    var container = document.createElement("viewcontainer");
    container.setAttribute ("id", containerId);
    container.setAttribute ("flex", "1");
    container.setAttribute ("type", type);
    if ("width" in parsedLocation)
        container.setAttribute ("width", parsedLocation.width);
    if ("height" in parsedLocation)
        container.setAttribute ("height", parsedLocation.height);
    container.setAttribute ("collapsed", "true");
    var before;
    if ("before" in parsedLocation)
        before = getChildById(parentContainer, parsedLocation.before);
    parentContainer.insertBefore(container, before);
    this.groutContainer(parentContainer, true);

    return container;
}

/*
 * x-vloc:/<window-id>[/<container-id>][?<option>[&<option>...]]
 *
 * <option> is one of...
 *   before=<view-id>
 *   type=(horizontal|vertical|tab)
 *   id=(<view-id>|<container-id>)
 *   target=(view|container)
 *   width=<number>
 *   height=<number>
 *
 * parseResult
 *  +- url
 *  +- windowId
 *  +- containerId
 *  +- <option>
 *  +- ...
 */

ViewManager.prototype.parseLocation =
function vmgr_parselocation (locationURL)
{
    var parseResult;
    var ary;
    
    if (locationURL.indexOf(VMGR_VURL_SCHEME) != 0)
        return null;
    
    ary = locationURL.substr(VMGR_SCHEME_LEN).match(/([^\/?]+)(?:\/([^\/?]+))?/);
    if (!ary)
        return null;

    parseResult = {
        url: locationURL,
        windowId: ary[1],
        containerId: (2 in ary) ? ary[2] : VMGR_DEFAULT_CONTAINER
    };

    var rest = RegExp.rightContext.substr(1);

    ary = rest.match(/([^&]+)/);

    while (ary)
    {
        rest = RegExp.rightContext.substr(1);
        var assignment = ary[1];
        ary = assignment.match(/(.+)=(.*)/);
        if (ASSERT(ary, "error parsing ``" + assignment + "'' from " +
                   locationURL))
        {
            /* only set the property the first time we see it */
            if (2 in ary && !(ary[1] in parseResult))
                parseResult[ary[1]] = ary[2];
        }
        ary = rest.match(/([^&]+)/);
    }
    
    if (parseResult.windowId == VMGR_NEW)
    {
        var id = "floater-" + this.floaterSequence++;

        while (id in this.windows)
            id = "floater-" + this.floaterSequence++;

        parseResult.windowId = id;
    }

    //dd (dumpObjectTree(parseResult));
    return parseResult;
}

ViewManager.prototype.getWindowId =
function getwindowid (window)
{
    for (var m in this.windows)
    {
        if (this.windows[m] == window)
            return m;
    }

    ASSERT (0, "unknown window");
    return null;
}   

ViewManager.prototype.getLayoutState =
function vmgr_getlayout ()
{
    var ary = new Array();
    
    for (var w in this.windows)
    {
        var container = 
            this.windows[w].document.getElementById (VMGR_DEFAULT_CONTAINER);
        this.getContainerContents (container, true, ary);
    }
    
    return ary;
}        
        
ViewManager.prototype.getContainerContents =
function vmgr_getcontents (container, recurse, ary)
{
    if (!ary)
        ary = new Array();
    var child = container.firstChild;
    
    while (child)
    {
        if (child.localName == "viewcontainer" && "viewCount" in child &&
            child.viewCount > 0)
        {
            ary.push (this.computeLocation(child));
            if (recurse)
                this.getContainerContents(child, true, ary);
        }
        else if (child.localName == "floatingview")
        {
            ary.push (this.computeLocation(child));
        }
        child = child.nextSibling;
    }

    return ary;
}

ViewManager.prototype.reconstituteVURLs =
function vmgr_remake (ary, startIndex)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        viewManager.reconstituteVURLs (ary, i);
    };
    
    if (!startIndex)
        startIndex = 0;

    for (var i = startIndex; i < ary.length; ++i)
    {
        var parsedLocation = this.parseLocation (ary[i]);
        if (!ASSERT(parsedLocation, "can't parse " + ary[i]) ||
            !ASSERT("target" in parsedLocation, "no target in " + ary[i]) ||
            !ASSERT("id" in parsedLocation, "no id in " + ary[i]))
        {
            continue;
        }

        if (parsedLocation.target == "container")
        {
            if (!(parsedLocation.windowId in this.windows))
            {
                //dd ("loading window " + parsedLocation.windowId);
                this.createWindow (parsedLocation.windowId, onWindowLoaded);
                return;
            }
                
            if (!ASSERT("type" in parsedLocation, "no type in " + ary[i]))
                parsedLocation.type = "horizontal";
            this.createContainer (parsedLocation, parsedLocation.id,
                                  parsedLocation.type);
        }
        else if (parsedLocation.target == "view")
        {
            var height = ("height" in parsedLocation ?
                         parsedLocation.height : null);
            var width = ("width" in parsedLocation ?
                         parsedLocation.width : null);
            this.moveView (parsedLocation, parsedLocation.id, height, width);
        }
        else
        {
            ASSERT(0, "unknown target in " + ary[i]);
        }
    }
}

ViewManager.prototype.computeLocation =
function vmgr_getlocation (element)
{
    if (!element.parentNode)
        return VMGR_VURL_HIDDEN;
    
    var target;
    var containerId;
    var type;
    
    if (element.localName == "floatingview")
    {
        target = "view";
        if (element.parentNode.localName != "viewcontainer")
            containerId = VMGR_HIDDEN;
        else
            containerId = element.parentNode.getAttribute("id");
    }
    else if (element.localName == "viewcontainer")
    {
        target      = "container";
        containerId = element.parentNode.getAttribute("id");
        type        = element.getAttribute("type");
    }
    else
    {
        ASSERT(0, "can't get location for unknown element " + element.localName);
        return null;
    }

    var before;
    
    if (containerId != VMGR_HIDDEN)
    {
        var beforeNode = element.nextSibling;
        if (beforeNode)
        {
            if (beforeNode.hasAttribute("grout"))
                beforeNode = beforeNode.nextSibling;
            
            if (ASSERT(beforeNode, "nothing before the grout?"))
                before = beforeNode.getAttribute("id");
        }
    }

    var windowId = this.getWindowId(element.ownerWindow);
    var id       = element.getAttribute("id");
    var height   = element.getAttribute("height");
    var width    = element.getAttribute("width");
    var url      = VMGR_VURL_SCHEME + "/" + windowId + "/" + containerId +
        "?target=" + target + "&id=" + id;
    if (height)
        url += "&height=" + height;
    if (width)
        url += "&width=" + width;
    if (before)
        url += "&before=" + before;
    if (type)
        url += "&type=" + type;
        
    var result = new String(url);
    result.url = url;
    result.windowId = windowId;
    result.containerId = containerId;
    result.target = target;
    result.id = id;
    result.height = height;
    result.width = width;
    result.before = before;
    result.type = type;

    return result;
}

ViewManager.prototype.locationExists =
function vmgr_islocation (parsedLocation)
{
    if (parsedLocation.windowId == VMGR_HIDDEN)
        return true;
    
    if (this.getLocation(parsedLocation))
        return true;

    return false;
}

ViewManager.prototype.getLocation =
function vmgr_getlocation (parsedLocation)
{
    if (parsedLocation.windowId == VMGR_HIDDEN)
    {
        //dd ("getLocation: location is hidden");
        return null;
    }
    
    var window = this.getWindowById(parsedLocation.windowId);
    if (!ASSERT(window, "unknown window id " + parsedLocation.windowId))
        return false;

    if (!parsedLocation.containerId)
        parsedLocation.containerId = VMGR_DEFAULT_CONTAINER;
    
    var container = window.document.getElementById(parsedLocation.containerId);
    return container;
}    

ViewManager.prototype.ensureLocation =
function vmgr_ensurelocation (parsedLocation, cb)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        var container = 
            window.document.getElementById (parsedLocation.containerId);
        if (!container && parsedLocation.containerId == VMGR_GUTTER)
        {
            viewManager.reconstituteVURLs ([VMGR_GUTTER_CONTAINER]);
            container = 
                window.document.getElementById (parsedLocation.containerId);
        }
        cb (window, container);
    };
    
    if (parsedLocation.windowId == VMGR_HIDDEN)
    {
        cb(null, null);
        return;
    }
    
    var window = this.getWindowById(parsedLocation.windowId);
    if (window)
    {
        onWindowLoaded(window);
        return;
    }
    
    this.createWindow (parsedLocation.windowId, onWindowLoaded);
}

ViewManager.prototype.moveViewURL =
function vmgr_moveurl (locationURL, viewId)
{
    var parsedLocation = this.parseLocation(locationURL);
    if (!ASSERT(parsedLocation, "can't parse " + locationURL))
        return;
    
    this.moveView(parsedLocation, viewId);
}

ViewManager.prototype.moveView =
function vmgr_move (parsedLocation, viewId, height, width)
{
    var viewManager = this;
    
    function moveContent (content, newParent, before)
    {
        if (!("originalParent" in content))
        {
            if (!ASSERT(content.parentNode, "no original parent to record"))
                return;
            content.originalParent = content.parentNode;
        }
        
        //dd ("OFF moveContent {");
        
        var previousParent = content.parentNode;
        previousParent.removeChild(content);
        if (previousParent.localName == "viewcontainer")
            viewManager.groutContainer (previousParent, Boolean(newParent));

        if (newParent)
        {
            //dd ("placing in new parent");
            newParent.insertBefore (content, before);
            viewManager.groutContainer (newParent);
        }
        else
        {
            //dd ("hiding");
            content.originalParent.appendChild(content);
            content.setAttribute ("containertype", "hidden");
        }

        //dd ("}");
    };

    function onLocationFound (window, container)
    {
        if (!window)
        {
            /* view needs to be hidden. */
            if (currentContent)
            {
                if ("onHide" in view)
                    view.onHide();

                if ("onViewRemoved" in view.currentContent.ownerWindow)
                    view.currentContent.ownerWindow.onViewRemoved(view);

                moveContent (view.currentContent, null);
                delete view.currentContent;
            }
            return;
        }
        
        var content = window.document.getElementById(viewId);
        if (!ASSERT(content, "no content for ``" + viewId + "'' found in " +
                    parsedLocation.windowId))
        {
            return;
        }

        var beforeNode;
        if ("before" in parsedLocation)
        {
            beforeNode = window.document.getElementById(parsedLocation.before);
            if (beforeNode && beforeNode.parentNode.localName != "viewcontainer")
            {
                beforeNode = null;
            }
        }
        
        if (!container)
        {
            /* container does not exist. */
            if (beforeNode)
            {
                container = beforeNode.parentNode;
            }
            else
            {
                container =
                    window.document.getElementById(VMGR_DEFAULT_CONTAINER);
            }
            if (!ASSERT(container, "can't locate default container"))
                return;
        }
        else if (beforeNode && beforeNode.parentNode != container)
        {
            //dd ("before is not in container");
            /* container portion of url wins in a mismatch situation */
            beforeNode = null;
        }   
        
        if (currentContent)
        {
            //dd ("have content");
            /* view is already visible, fire an onHide().  If we're destined for
             * a new document, return the current content to it's hiding place.
             */
            if (beforeNode == currentContent ||
                (currentContent.nextSibling == beforeNode &&
                 container == currentContent.parentNode))
            {
                /* unless of course we're already where we are supposed to be. */
                return;
            }   
            
            if ("onHide" in view)
                view.onHide();

            if ("onViewRemoved" in currentContent.ownerWindow)
                currentContent.ownerWindow.onViewRemoved(view);

            if (currentContent != content)
            {
                //dd ("returning content to home");
                moveContent (currentContent, null);
            }
        }        
        
        moveContent (content, container, beforeNode);
        view.currentContent = content;
        if (height)
            content.setAttribute ("height", height);
        if (width)
            content.setAttribute ("width", width);
        
        if ("onShow" in view)
            view.onShow();

        if ("onViewAdded" in window)
            window.onViewAdded(view);
    };

    var view = this.views[viewId];
    var currentContent = ("currentContent" in view ?
                          view.currentContent : null);
    if (currentContent && currentContent.ownerWindow == this.mainWindow)
        view.previousLocation = this.computeLocation(currentContent);

    this.ensureLocation (parsedLocation, onLocationFound);
}

ViewManager.prototype.groutContainer =
function vmgr_groutcontainer (container, noclose)
{
    var type = container.getAttribute ("type");
    
    if (!ASSERT(type in this.groutFunctions, "unknown container type ``" +
                type + "''"))
    {
        return;
    }
    
    this.groutFunctions[type](this, container, noclose);
}

ViewManager.prototype.groutFunctions = new Object();

ViewManager.prototype.groutFunctions["horizontal"] =
ViewManager.prototype.groutFunctions["vertical"] =
function vmgr_groutbox (viewManager, container, noclose)
{
    function closeWindow (window)
    {
        window.close();
    };
    
    function isContainerEmpty (container)
    {
        var rv = container.localName == "viewcontainer" && 
            "viewCount" in container && container.viewCount == 0;
        //dd ("isContainerEmpty: " + container.getAttribute("id") + ": " + rv);
        return rv;
    };
    
    if (!container)
        throw new InvalidParam ("container", container);

    ASSERT(container.localName == "viewcontainer",
           "Attempt to grout something that is not a view container");

    //dd ("OFF grouting: " + container.getAttribute("id") +" {");

    var lastViewCount = ("viewCount" in container) ? container.viewCount : 0;
    container.viewCount = 0;
    var doc = container.ownerDocument;
    var content = container.firstChild;

    var previousContent;
    var nextContent;
    
    while (content)
    {
        previousContent = content.previousSibling;
        nextContent = content.nextSibling;

        if (content.hasAttribute("grout"))
        {
            if (!previousContent || !nextContent ||
                previousContent.hasAttribute("grout") ||
                nextContent.hasAttribute("grout") ||
                isContainerEmpty(previousContent))
            {
                container.removeChild(content);
            }
        }
        else
        {
            if (content.localName == "floatingview")
                ++container.viewCount;
            else if (content.localName == "viewcontainer")
            {
                if ("viewCount" in content)
                    container.viewCount += content.viewCount;
                //else
                    //dd ("no viewCount in " + content.getAttribute("id"));
            }

            if (nextContent && !nextContent.hasAttribute("grout") &&
                !isContainerEmpty(nextContent) && !isContainerEmpty(content))
            {
                var collapse;
                if (content.hasAttribute("splitter-collapse"))
                    collapse = content.getAttribute("splitter-collapse");
                else
                    collapse = "after";
                var split = doc.createElement("splitter");
                split.setAttribute("grout", "true");
                split.setAttribute("collapse", collapse);
                split.appendChild(doc.createElement("grippy"));
                container.insertBefore(split, nextContent);
            }
        }
        content = nextContent;
    }

    if (previousContent && previousContent.hasAttribute("grout") &&
        isContainerEmpty(previousContent.nextSibling))
    {
        container.removeChild(previousContent);
    }
            

    if (isContainerEmpty(container))
    {
        //dd ("container is empty, hiding");
        container.setAttribute("collapsed", "true");
        if (!noclose && container.parentNode.localName == "window" &&
            container.ownerWindow != viewManager.mainWindow &&
            !("isClosing" in container.ownerWindow) &&
            lastViewCount > 0)
        {
            setTimeout (closeWindow, 1, container.ownerWindow);
            //dd ("} no children, closing window");
            return;
        }
    }
    else
    {
        //dd ("unhiding");
        container.removeAttribute("collapsed");
    }

    if (container.parentNode.localName == "viewcontainer")
        viewManager.groutContainer(container.parentNode, noclose);

    //dd ("} " + container.getAttribute("id") +
    //    " view count: " + container.viewCount);

}

ViewManager.prototype.findFloatingView =
function vmgr_findview (element)
{
    while (element && element.localName != "floatingview")
        element = element.parentNode;
    
    return element;
}

ViewManager.prototype.getDropDirection =
function vmgr_getdropdir (event, floatingView)
{
    var eventX = event.screenX - floatingView.boxObject.screenX;
    var eventY = event.screenY - floatingView.boxObject.screenY;
    var viewW = floatingView.boxObject.width;
    var viewH = floatingView.boxObject.height;

    var rv;
    var dir;

    var m = viewH / viewW;
    if (eventY < m * eventX)
        dir = -1;
    else
        dir = 1;
    
    m = -m;
    
    if (eventY < m * eventX + viewH)
    {
        if (dir < 0)
            rv = "up";
        else
            rv = "left";
    }
    else
    {
        if (dir < 0)
            rv = "right";
        else
            rv = "down";
    }

    return rv;
}

ViewManager.prototype.onTitleDragStart =
function vmgr_dragstart (event, transferData, action)
{
    var floatingView = this.findFloatingView (event.originalTarget);
    if (!floatingView)
        return false;
    
    var viewId = floatingView.getAttribute("id");
    if (!ASSERT(viewId in this.views, "unknown view " + viewId))
        return false;
    
    var view = this.views[viewId];
    
    if (!ASSERT("currentContent" in view, "view is not visible " + viewId))
        return false;
    
    var locationURL = this.computeLocation (view.currentContent);
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-vloc", locationURL);

    return true;
}

ViewManager.prototype.getSupportedFlavours =
function vmgr_nicename ()
{
    var flavours = new FlavourSet();
    flavours.appendFlavour("text/x-vloc");
    return flavours;
}

ViewManager.prototype.onViewDragOver =
function vmgr_dragover (event, flavor, session)
{
    session.canDrop = false;
    
    if (flavor.contentType != "text/x-vloc")
        return false;

    var target = this.findFloatingView(event.originalTarget);
    if (!target)
        return false;

    /* COVER YOUR EYES!! */
    var data = nsTransferable.get (this.getSupportedFlavours(),
                                   nsDragAndDrop.getDragData, true);
    data = data.first.first;
    
    var parsedLocation = this.parseLocation(data.data);
    if (target.getAttribute("id") == parsedLocation.id)
        return false;
    
    var direction = this.getDropDirection(event, target);
    target.setAttribute ("dragover", direction);
    target.proxyIcon.setAttribute ("dragover", direction);

    session.canDrop = true;
    return true;
}

ViewManager.prototype.onViewDragExit =
function vmgr_dragexit (event, session)
{
    var target = this.findFloatingView(event.originalTarget);
    if (target)
    {
        target.removeAttribute ("dragover");
        target.proxyIcon.removeAttribute ("dragover");
    }
}

ViewManager.prototype.onViewDrop =
function vmgr_ondrop (event, transferData, session)
{
    var viewManager = this;
    
    function callProcessDrop ()
    {
        viewManager.processDrop (sourceView, targetView, direction);
    };
    
    var floatingView = this.findFloatingView (event.originalTarget);
    var viewId = floatingView.getAttribute ("id");
    var targetView = this.views[viewId];
    if (!ASSERT("currentContent" in targetView, "view is not visible " + viewId))
        return false;

    var parsedSource = this.parseLocation(transferData.data);
    if (!ASSERT(parsedSource, "unable to parse " + transferData.data))
        return false;
    
    if (parsedSource.id == viewId)
    {
        //dd ("wanker");
        return false;
    }

    var sourceView = this.views[parsedSource.id];
    
    var direction = this.getDropDirection (event, floatingView);

    /* performing the actual drop action may involve moving DOM nodes
     * that are involved in the actual drag-drop operation.  Mozilla
     * doesn't like it when you do that before drag-drop is done, so we
     * call ourselves back on a timer, to allow the drag-drop wind down before
     * modifying the DOM.
     */
    setTimeout (callProcessDrop, 1);
    return true;
}

ViewManager.prototype.processDrop =
function vmgr_dewdropinn (sourceView, targetView, direction)
{    
    var viewManager = this;

    function reparent (child, type)
    {
        if (!ASSERT(child.parentNode.localName == "viewcontainer",
                    "can't reparent child with unknown parent"))
        {
            return null;
        }
        
        var childLocation = viewManager.computeLocation(child);
        var id = "vm-container-" + ++viewManager.containerSequence;
        while (child.ownerDocument.getElementById(id))
            id = "vm-container-" + ++viewManager.containerSequence;
        var container = viewManager.createContainer (childLocation, id, type);
        var newLocation = { windowId: parsedTarget.windowId, containerId: id };
        viewManager.moveView (newLocation, childLocation.id);

        return container;
    };
    
    var destContainer;
    var destBefore;
    var currentContent   = targetView.currentContent;
    var parsedTarget     = this.computeLocation (currentContent);
    var currentContainer = currentContent.parentNode;
    var parentType       = currentContainer.getAttribute("type");
    
    if (parentType == "vertical")
    {
        switch (direction)
        {
            case "up":
                destContainer = currentContainer;
                destBefore = currentContent;
                break;
                
            case "down":
                destContainer = currentContainer;
                destBefore = currentContent.nextSibling;
                if (destBefore && destBefore.hasAttribute("grout"))
                    destBefore = destBefore.nextSibling;
                break;
                
            case "left":
                destContainer = reparent (currentContent, "horizontal");
                destBefore = currentContent;
                break;
                
            case "right":
                destContainer = reparent (currentContent, "horizontal");
                break;
        }
    }
    else
    {
        switch (direction)
        {
            case "up":
                destContainer = reparent (currentContent, "vertical");
                destBefore = currentContent;
                break;
                
            case "down":
                destContainer = reparent (currentContent, "vertical");
                break;
                
            case "left":
                destContainer = currentContainer;
                destBefore = currentContent;
                break;
                
            case "right":
                destContainer = currentContainer;
                destBefore = currentContent.nextSibling;
                if (destBefore && destBefore.hasAttribute("grout"))
                    destBefore = destBefore.nextSibling;
                break;
        }
    }

    var dest = new Object();
    dest.windowId = parsedTarget.windowId;
    dest.containerId = destContainer.getAttribute ("id");
    dest.before = destBefore ? destBefore.getAttribute("id") : null;
    this.moveView (dest, sourceView.viewId);
}
