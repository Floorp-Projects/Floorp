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
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is James Ross.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross <silver@warwickcompsoc.co.uk>
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

const MEDIATOR_CONTRACTID   = "@mozilla.org/appshell/window-mediator;1";
const FILEPICKER_CONTRACTID = "@mozilla.org/filepicker;1";

const nsIWindowMediator     = Components.interfaces.nsIWindowMediator;
const nsIFilePicker         = Components.interfaces.nsIFilePicker;

const CONFIG_WINDOWTYPE     = "irc:chatzilla:config";

/* Now we create and set up some required items from other Chatzilla JS files 
 * that we really have no reason to load, but the ones we do need won't work 
 * without these...
 */
var ASSERT = function(cond, msg) { if (!cond) { alert(msg); } return cond; }
var client;

function CIRCNetwork() {}
function CIRCServer() {}
function CIRCChannel() {}
function CIRCChanUser() {}

function getObjectDetails(obj)
{
    var rv = new Object();
    rv.sourceObject = obj;
    rv.TYPE = obj.TYPE;
    rv.parent = ("parent" in obj) ? obj.parent : null;
    rv.user = null;
    rv.channel = null;
    rv.server = null;
    rv.network = null;
    
    switch (obj.TYPE)
    {
        case "PrefNetwork":
            rv.network = obj;
            if ("primServ" in rv.network)
                rv.server = rv.network.primServ;
            else
                rv.server = null;
            break;
            
        case "PrefChannel":
            rv.channel = obj;
            rv.server = rv.channel.parent;
            rv.network = rv.server.parent;
            break;
            
        case "PrefUser":
            rv.user = obj;
            rv.server = rv.user.parent;
            rv.network = rv.server.parent;
            break;
    }
    
    return rv;
}

/* Global object for the prefs. The 'root' of all the objects to do with the
 * prefs.
 */
function PrefGlobal()
{
    this.networks = new Object();
    this.commandManager = new Object();
    this.commandManager.defineCommand = function() {};
}
PrefGlobal.prototype.TYPE = "PrefGlobal";

/* Represents a single network in the hierarchy.
 * 
 * |force| - If true, sets a pref on this object. This makes sure the object
 *           is "known" next time we load up (since we look for any prefs).
 * 
 * |show|  - If true, the object still exists even if the magic pref is not set.
 *           Thus, allows an object to exist without any prefs set.
 */
function PrefNetwork(parent, name, force, show)
{
    if (name in parent.networks)
        return parent.networks[name];
    
    this.parent = parent;
    this.unicodeName = name;
    this.viewName = name;
    this.canonicalName = name;
    this.encodedName = name;
    this.prettyName = getMsg(MSG_PREFS_FMT_DISPLAY_NETWORK, this.unicodeName);
    this.servers = new Object();
    this.primServ = new PrefServer(this, "dummy server");
    this.channels = this.primServ.channels;
    this.users = this.primServ.users;
    this.prefManager = getNetworkPrefManager(this);
    this.prefs = this.prefManager.prefs;
    this.prefManager.onPrefChanged = function(){};
    
    if (force)
        this.prefs["hasPrefs"] = true;
    
    if (this.prefs["hasPrefs"] || show)
        this.parent.networks[this.canonicalName] = this;
    
    return this;
};
PrefNetwork.prototype.TYPE = "PrefNetwork";

/* Cleans up the mess. */
PrefNetwork.prototype.clear =
function pnet_clear()
{
    this.prefs["hasPrefs"] = false;
    delete this.parent.networks[this.canonicalName];
}

/* A middle-management object.
 * 
 * Exists only to satisfy the IRC library pref functions that expect this
 * particular hierarchy.
 */
function PrefServer(parent, name)
{
    this.parent = parent;
    this.unicodeName = name;
    this.viewName = name;
    this.canonicalName = name;
    this.encodedName = name;
    this.prettyName = this.unicodeName; // Not used, thus not localised.
    this.channels = new Object();
    this.users = new Object();
    this.parent.servers[name] = this;
    return this;
};
PrefServer.prototype.TYPE = "PrefServer";

/* Represents a single channel in the hierarchy.
 * 
 * |force| and |show| the same as PrefNetwork.
 */
function PrefChannel(parent, name, force, show)
{
    if (name in parent.channels)
        return parent.channels[name];
    
    this.parent = parent;
    this.unicodeName = name;
    this.viewName = name;
    this.canonicalName = name;
    this.encodedName = name;
    this.prettyName = getMsg(MSG_PREFS_FMT_DISPLAY_CHANNEL, 
                             [this.parent.parent.unicodeName, this.unicodeName]);
    this.prefManager = getChannelPrefManager(this);
    this.prefs = this.prefManager.prefs;
    this.prefManager.onPrefChanged = function(){};
    
    if (force)
        this.prefs["hasPrefs"] = true;
    
    if (this.prefs["hasPrefs"] || show)
        this.parent.channels[this.canonicalName] = this;
    
    return this;
};
PrefChannel.prototype.TYPE = "PrefChannel";

/* Cleans up the mess. */
PrefChannel.prototype.clear =
function pchan_clear()
{
    this.prefs["hasPrefs"] = false;
    delete this.parent.channels[this.canonicalName];
}

/* Represents a single user in the hierarchy.
 * 
 * |force| and |show| the same as PrefNetwork.
 */
function PrefUser(parent, name, force, show)
{
    if (name in parent.users)
        return parent.users[name];
    
    this.parent = parent;
    this.unicodeName = name;
    this.viewName = name;
    this.canonicalName = name;
    this.encodedName = name;
    this.prettyName = getMsg(MSG_PREFS_FMT_DISPLAY_USER, 
                             [this.parent.parent.unicodeName, this.unicodeName]);
    this.prefManager = getUserPrefManager(this);
    this.prefs = this.prefManager.prefs;
    this.prefManager.onPrefChanged = function(){};
    
    if (force)
        this.prefs["hasPrefs"] = true;
    
    if (this.prefs["hasPrefs"] || show)
        this.parent.users[this.canonicalName] = this;
    
    return this;
};
PrefUser.prototype.TYPE = "PrefUser";

/* Cleans up the mess. */
PrefUser.prototype.clear =
function puser_clear()
{
    this.prefs["hasPrefs"] = false;
    delete this.parent.users[this.canonicalName];
}

// Stores a list of |PrefObject|s.
function PrefObjectList()
{
    this.objects = new Array();
    
    return this;
}

// Add an object, and init it's private data.
PrefObjectList.prototype.addObject =
function polist_addObject(pObject)
{
    this.objects.push(pObject);
    return pObject.privateData = new ObjectPrivateData(pObject, this.objects.length - 1);
}

/* Removes an object, without changing the index. */
PrefObjectList.prototype.deleteObject =
function polist_addObject(index)
{
    this.objects[index].privateData.clear();
    this.objects[index].clear();
    this.objects[index] = { privateData: null };
}

// Get a specific object.
PrefObjectList.prototype.getObject =
function polist_getObject(index)
{
    return this.objects[index].privateData;
}

// Gets the private data for an object.
PrefObjectList.getPrivateData =
function polist_getPrivateData(object)
{
    return object.privateData;
}

// Stores the pref object's private data.
function ObjectPrivateData(parent, index)
{
    this.parent = parent;  // Real pref object.
    this.prefs = new Object();
    this.groups = new Object();
    
    this.arrayIndex = index;
    this.deckIndex = -1;
    this.dataLoaded = false;
    
    var treeObj = document.getElementById("pref-tree-object");
    this.tree = document.getElementById("pref-tree");
    this.treeContainer = document.createElement("treeitem");
    this.treeNode = document.createElement("treerow");
    this.treeCell = document.createElement("treecell");
    
    this.treeContainer.setAttribute("prefobjectindex", this.arrayIndex);
    this.treeCell.setAttribute("label", this.parent.unicodeName);
    
    switch (this.parent.TYPE)
    {
        case "PrefChannel":
        case "PrefUser":
            var p = this.parent.parent.parent;  // Network object.
            var pData = PrefObjectList.getPrivateData(p);
            
            if (!("treeChildren" in pData) || !pData.treeChildren)
            {
                pData.treeChildren = document.createElement("treechildren");
                pData.treeContainer.appendChild(pData.treeChildren);
                treeObj.view.toggleOpenState(treeObj.view.rowCount - 1);
            }
            pData.treeContainer.setAttribute("container", "true");
            pData.treeChildren.appendChild(this.treeContainer);
            break;
            
        default:
            this.tree.appendChild(this.treeContainer);
            break;
    }
    
    this.treeContainer.appendChild(this.treeNode);
    this.treeNode.appendChild(this.treeCell);
    
    return this;
}

// Creates all the XUL elements needed to show this pref object.
ObjectPrivateData.prototype.loadXUL =
function opdata_loadXUL(tabOrder)
{
    var t = this;
    
    /* Function that sorts the preferences by their label, else they look 
     * fairly random in order.
     * 
     * Sort keys: not grouped, sub-group name, boolean, pref label.
     */
    function sortByLabel(a, b) {
        if (t.prefs[a].subGroup || t.prefs[b].subGroup)
        {
            // Non-grouped go first.
            if (!t.prefs[a].subGroup)
                return -1;
            if (!t.prefs[b].subGroup)
                return 1;
            
            // Sub-group names.
            if (t.prefs[a].subGroup < t.prefs[b].subGroup)
                return -1;
            if (t.prefs[a].subGroup > t.prefs[b].subGroup)
                return 1;
        }
        
        // Booleans go first.
        if ((t.prefs[a].type == "boolean") && (t.prefs[b].type != "boolean"))
            return -1;
        if ((t.prefs[a].type != "boolean") && (t.prefs[b].type == "boolean"))
            return 1;
        
        // ...then label.
        if (t.prefs[a].label < t.prefs[b].label)
            return -1;
        if (t.prefs[a].label > t.prefs[b].label)
            return 1;
        return 0;
    };
    
    if (this.deckIndex >= 0)
        return;
    
    this.deck = document.getElementById("pref-object-deck");
    this.tabbox = document.createElement("tabbox");
    this.tabs = document.createElement("tabs");
    this.tabPanels = document.createElement("tabpanels");
    
    this.tabbox.setAttribute("flex", 1);
    this.tabPanels.setAttribute("flex", 1);
    
    this.tabbox.appendChild(this.tabs);
    this.tabbox.appendChild(this.tabPanels);
    this.deck.appendChild(this.tabbox);
    
    this.deckIndex = this.deck.childNodes.length - 1;
    
    this.loadData();
    
    var prefList = keys(this.prefs);
    prefList.sort(sortByLabel);
    
    for (var i = 0; i < tabOrder.length; i++)
    {
        var pto = tabOrder[i];
        var needTab = pto.fixed;
        if (!needTab)
        {
            // Not a "always visible" tab, check we need it.
            for (var j = 0; j < prefList.length; j++)
            {
                if (this.prefs[prefList[j]].mainGroup == pto.name)
                {
                    needTab = true;
                    break;
                }
            }
        }
        if (needTab)
            this.addGroup(pto.name);
    }
    
    for (i = 0; i < prefList.length; i++)
        this.prefs[prefList[i]].loadXUL();
    
    if (this.tabs.childNodes.length > 0)
        this.tabbox.selectedIndex = 0;
}

// Loads all the prefs.
ObjectPrivateData.prototype.loadData =
function opdata_loadData()
{
    if (this.dataLoaded)
        return;
    
    this.dataLoaded = true;
    
    // Now get the list of pref names, and add them...
    var prefList = this.parent.prefManager.prefNames;
    
    for (i in prefList)
        this.addPref(prefList[i]);
}

// Clears up all the XUL objects and data.
ObjectPrivateData.prototype.clear =
function opdata_clear()
{
    //dd("Removing prefs for " + this.parent.displayName + " {");
    if (!this.dataLoaded)
        this.loadData();
    for (var i in this.prefs)
        this.prefs[i].clear();
    //dd("}");
    
    if (this.deckIndex >= 0)
    {
        this.deck.removeChild(this.tabbox);
        this.treeContainer.removeAttribute("container");
        this.treeContainer.parentNode.removeChild(this.treeContainer);
    }
}

// Resets all the prefs to their original values.
ObjectPrivateData.prototype.reset =
function opdata_reset()
{
    for (var i in this.prefs)
        if (this.prefs[i].type != "hidden")
            this.prefs[i].reset();
}

// Adds a pref to the internal data structures.
ObjectPrivateData.prototype.addPref =
function opdata_addPref(name)
{
    return this.prefs[name] = new PrefData(this, name);
}

// Adds a group to a pref object's data.
ObjectPrivateData.prototype.addGroup =
function opdata_addPref(name)
{
    // Special group for prefs we don't want shown (nothing sinister here).
    if (name == "hidden")
        return null;
    
    if (!(name in this.groups))
        this.groups[name] = new PrefMainGroup(this, name);
    
    return this.groups[name];
}

// Represents a single pref on a single object within the pref window.
function PrefData(parent, name)
{
    // We want to keep all this "worked out" info, so make a hash of all 
    // the prefs on the pwData [Pref Window Data] property of the object.
    
    // First, lets find out what kind of pref we've got:
    this.parent = parent;  // Private data for pref object.
    this.name = name;
    this.manager = this.parent.parent.prefManager;   // PrefManager.
    this.record = this.manager.prefRecords[name];    // PrefRecord.
    this.def = this.record.defaultValue;             // Default value.
    this.type = typeof this.def;                     // Pref type.
    this.val = this.manager.prefs[name];             // Current value.
    this.startVal = this.val;                        // Start value.
    this.label = this.record.label;                  // Display name.
    this.help = this.record.help;                    // Help text.
    this.group = this.record.group;                  // Group identifier.
    this.labelFor = "none";                          // Auto-grouped label.
    
    // Handle defered prefs (call defer function, and use resulting 
    // value/type instead).
    if (this.type == "function")
        this.def = this.def(this.name);
    this.type = typeof this.def;
    
    // And those arrays... this just makes our life easier later by having 
    // a particular name for array prefs.
    if (this.def instanceof Array)
        this.type = "array";
    
    if (this.group == "hidden")
        this.type = "hidden";
    
    // Convert "a.b" into sub-properties...
    var m = this.group.match(/^([^.]*)(\.(.*))?$/)
    ASSERT(m, "Failed group match!");
    this.mainGroup = m[1];
    this.subGroup = m[3];
    
    return this;
}

/* Creates all the XUL elements to display this one pref. */
PrefData.prototype.loadXUL =
function pdata_loadXUL()
{
    if (this.type == "hidden")
        return;
    
    // Create the base box for the pref.
    this.box = document.createElement("box");
    this.box.orient = "horizontal";
    this.box.setAttribute("align", "center");
    
    switch (this.type)
    {
        case "string":
            label = document.createElement("label");
            label.setAttribute("value", this.label);
            label.width = 100;
            label.flex = 1;
            this.box.appendChild(label);
            
            this.edit = document.createElement("textbox");
            // We choose the size based on the length of the default.
            if (this.def.length < 8)
                this.edit.setAttribute("size", "10");
            else if (this.def.length < 20)
                this.edit.setAttribute("size", "25");
            else
                this.edit.flex = 1;
            
            var editCont = document.createElement("hbox");
            editCont.flex = 1000;
            editCont.appendChild(this.edit);
            this.box.appendChild(editCont);
            
            // But if it's a file/URL...
            if (this.def.match(/^([a-z]+:\/|[a-z]:\\)/i))
            {
                // ...we make it as big as possible.
                this.edit.removeAttribute("size");
                this.edit.flex = 1;
                
                if (!this.name.match(/path$/i) && 
                    (this.def.match(/^(file|chrome):\//i) ||
                     this.name.match(/filename$/i)))
                {
                    // So long as the pref name doesn't end in "path", and 
                    // it's chrome:, file: or a local file, we add the button.
                    var ext = "";
                    var m = this.def.match(/\.([a-z0-9]+)$/);
                    if (m)
                        ext = "*." + m[1];
                    
                    // We're cheating again here, if it ends "filename" it's 
                    // a local file path.
                    var type = (this.name.match(/filename$/i) ? "file" : "fileurl");
                    appendButton(this.box, "onPrefBrowse", 
                                 { label: MSG_PREFS_BROWSE, spec: ext, 
                                   kind: type });
                }
            }
            break;
            
        case "number":
            label = document.createElement("label");
            label.setAttribute("value", this.label);
            label.width = 100;
            label.flex = 1;
            this.box.appendChild(label);
            
            this.edit = document.createElement("textbox");
            this.edit.setAttribute("size", "5");
            
            editCont = document.createElement("hbox");
            editCont.flex = 1000;
            editCont.appendChild(this.edit);
            this.box.appendChild(editCont);
            break;
            
        case "boolean":
            this.edit = document.createElement("checkbox");
            this.edit.setAttribute("label", this.label);
            this.box.appendChild(this.edit);
            break;
            
        case "array":
            this.box.orient = "vertical";
            this.box.removeAttribute("align");
            
            if (this.help)
            {
                label = document.createElement("label");
                label.appendChild(document.createTextNode(this.help));
                this.box.appendChild(label);
            }
            
            var oBox = document.createElement("box");
            oBox.orient = "horizontal";
            this.box.appendChild(oBox);
            
            this.edit = document.createElement("listbox");
            this.edit.flex = 1;
            this.edit.setAttribute("style", "height: 1em;");
            this.edit.setAttribute("kind", "url");
            if (this.def.length > 0 && this.def[0].match(/^file:\//))
                this.edit.setAttribute("kind", "fileurl");
            this.edit.setAttribute("onselect", "gPrefWindow.onPrefListSelect(this);");
            this.edit.setAttribute("ondblclick", "gPrefWindow.onPrefListEdit(this);");
            oBox.appendChild(this.edit);
            
            var box = document.createElement("box");
            box.orient = "vertical";
            oBox.appendChild(box);
            
            // NOTE: This order is important - getRelatedItem needs to be 
            // kept in sync with this order. Perhaps a better way is needed...
            appendButton(box, "onPrefListUp",     { label: MSG_PREFS_MOVE_UP, 
                                                    "class": "up" });
            appendButton(box, "onPrefListDown",   { label: MSG_PREFS_MOVE_DOWN, 
                                                    "class": "down" });
            appendSeparator(box);
            appendButton(box, "onPrefListAdd",    { label: MSG_PREFS_ADD });
            appendButton(box, "onPrefListEdit",   { label: MSG_PREFS_EDIT });
            appendButton(box, "onPrefListDelete", { label: MSG_PREFS_DELETE });
            break;
            
        default:
            // This is really more of an error case, since we really should
            // know about all the valid pref types.
            var label = document.createElement("label");
            label.setAttribute("value", "[not editable] " + this.type);
            this.box.appendChild(label);
    }
    
    this.loadData();
    
    if (this.edit)
    {
        this.edit.setAttribute("prefobjectindex", this.parent.arrayIndex);
        this.edit.setAttribute("prefname", this.name);
    }
    
    if (!ASSERT("groups" in this.parent, "Must have called " +
                "[ObjectPrivateData].loadXUL before trying to display prefs."))
        return;
    
    this.parent.addGroup(this.mainGroup);
    if (this.subGroup)
        this.parent.groups[this.mainGroup].addGroup(this.subGroup);
    
    if (!this.subGroup)
        this.parent.groups[this.mainGroup].box.appendChild(this.box);
    else
        this.parent.groups[this.mainGroup].groups[this.subGroup].box.appendChild(this.box);
    
    // Setup tooltip stuff...
    if (this.help && (this.type != "array"))
    {
        this.box.setAttribute("tooltiptitle", this.label);
        this.box.setAttribute("tooltipcontent", this.help);
        this.box.setAttribute("onmouseover", "gPrefWindow.onPrefMouseOver(this);");
        this.box.setAttribute("onmousemove", "gPrefWindow.onPrefMouseMove(this);");
        this.box.setAttribute("onmouseout", "gPrefWindow.onPrefMouseOut(this);");
    }
}

/* Loads the pref's data into the edit component. */
PrefData.prototype.loadData =
function pdata_loadData()
{
    /* Note about .value and .setAttribute as used here:
     * 
     * XBL doesn't kick in until CSS is calculated on a node, so the code makes 
     * a compromise and uses these two methods as appropriate. Initally this 
     * is called is before the node has been placed in the document DOM tree, 
     * and thus hasn't been "magiced" by XBL and so .value is meaningless to 
     * it. After initally being set as an attribute, it's added to the DOM, 
     * XBL kicks in, and after that .value is the only way to change the value.
     */
    switch (this.type)
    {
        case "string":
            if (this.edit.hasAttribute("value"))
                this.edit.value = this.val;
            else
                this.edit.setAttribute("value", this.val);
            break;
            
        case "number":
            if (this.edit.hasAttribute("value"))
                this.edit.value = this.val;
            else
                this.edit.setAttribute("value", this.val);
            break;
            
        case "boolean":
            if (this.edit.hasAttribute("checked"))
                this.edit.checked = this.val;
            else
                this.edit.setAttribute("checked", this.val);
            break;
            
        case "array":
            // Remove old entires.
            while (this.edit.firstChild)
                this.edit.removeChild(this.edit.firstChild);
            
            // Add new ones.
            for (i = 0; i < this.val.length; i++)
            {
                var item = document.createElement("listitem");
                item.value = this.val[i];
                item.crop = "center";
                item.setAttribute("label", this.val[i]);
                this.edit.appendChild(item);
            }
            
            // Make sure buttons are up-to-date.
            gPrefWindow.onPrefListSelect(this.edit);
            break;
            
        default:
    }
}

/* Cleans up the mess. */
PrefData.prototype.clear =
function pdata_clear()
{
    //dd("Clearing pref " + this.name);
    if (("box" in this) && this.box)
    {
        this.box.parentNode.removeChild(this.box);
        delete this.box;
    }
    try {
        this.manager.clearPref(this.name);
    } catch(ex) {}
}

/* Resets the pref to it's default. */
PrefData.prototype.reset =
function pdata_reset()
{
    //try {
    //    this.manager.clearPref(this.name);
    //} catch(ex) {}
    this.val = this.def;
    this.loadData();
}

/* Saves the pref... or would do. */
PrefData.prototype.save =
function pdata_save()
{
    //FIXME//
}

// Represents a "main group", i.e. a tab for a single pref object.
function PrefMainGroup(parent, name)
{
    // Init this group's object.
    this.parent = parent;  // Private data for pref object.
    this.name = name;
    this.groups = new Object();
    this.label = getMsg("pref.group." + this.name + ".label", null, this.name);
    this.tab = document.createElement("tab");
    this.tabPanel = document.createElement("tabpanel");
    this.box = this.sb = document.createElement("scroller");
    
    this.tab.setAttribute("label", this.label);
    this.tabPanel.setAttribute("orient", "vertical");
    this.sb.setAttribute("orient", "vertical");
    this.sb.setAttribute("flex", 1);
    
    this.parent.tabs.appendChild(this.tab);
    this.parent.tabPanels.appendChild(this.tabPanel);
    this.tabPanel.appendChild(this.sb);
    
    return this;
}
// Adds a sub group to this main group.
PrefMainGroup.prototype.addGroup =
function pmgroup_addGroup(name)
{
    // If the sub group doesn't exist, make it exist.
    if (!(name in this.groups))
        this.groups[name] = new PrefSubGroup(this, name);
    
    return this.groups[name];
}

// Represents a "sub group", i.e. a groupbox on a tab, for a single main group.
function PrefSubGroup(parent, name)
{
    this.parent = parent;  // Main group.
    this.name = name;
    this.fullGroup = this.parent.name + "." + this.name;
    this.label = getMsg("pref.group." + this.fullGroup + ".label", null, this.name);
    this.help  = getMsg("pref.group." + this.fullGroup + ".help", null, "");
    this.gb = document.createElement("groupbox");
    this.cap = document.createElement("caption");
    this.box = document.createElement("box");
    
    this.cap.setAttribute("label", this.label);
    this.gb.appendChild(this.cap);
    this.box.orient = "vertical";
    
    // If there's some help text, we place it as the first thing inside 
    // the <groupbox>, as an explanation for the entire subGroup.
    if (this.help)
    {
        this.helpLabel = document.createElement("label");
        this.helpLabel.appendChild(document.createTextNode(this.help));
        this.gb.appendChild(this.helpLabel);
    }
    this.gb.appendChild(this.box);
    this.parent.box.appendChild(this.gb);
    
    return this;
}

// Actual pref window itself.
function PrefWindow()
{
    // Not loaded until the pref list and objects have been created in |onLoad|.
    this.loaded = false;
    
    /* PREF TAB ORDER: Determins the order, and fixed tabs, found on the UI.
     * 
     * It is an array of mainGroup names, and a flag indicating if the tab
     * should be created even when there's no prefs for it.
     * 
     * This is for consistency, although none of the ChatZilla built-in pref
     * objects leave fixed tabs empty currently.
     */
    this.prefTabOrder = [{ fixed: true,  name: "general"}, 
                         { fixed: true,  name: "appearance"}, 
                         { fixed: false, name: "lists"}, 
                         { fixed: false, name: "dcc"}, 
                         { fixed: false, name: "startup"}, 
                         { fixed: false, name: "global"}, 
                         { fixed: false, name: "munger"}
                        ];
    
    /* PREF OBJECTS: Stores all the objects we've using that have prefs.
     * 
     * Each object gets a "privateData" object, which is then used by the pref
     * window code for storing all of it's data on the object.
     */
    this.prefObjects = null;
    
    /* TOOLTIPS: Special tooltips for preference items.
     * 
     * Timer:     return value from |setTimeout| whenever used. There is only 
     *            ever one timer going for the tooltips.
     * Showing:   stores visibility of the tooltip.
     * ShowDelay: ms delay which them mouse must be still to show tooltips.
     * HideDelay: ms delay before the tooltips hide themselves.
     */
    this.tooltipTimer = 0;
    this.tooltipShowing = false;
    this.tooltipShowDelay = 1000;
    this.tooltipHideDelay = 20000;
}
PrefWindow.prototype.TYPE = "PrefWindow";

/* Updates the tooltip state, either showing or hiding it. */
PrefWindow.prototype.setTooltipState =
function pwin_setTooltipState(visible)
{
    // Shortcut: if we're already in the right state, don't bother.
    if (this.tooltipShowing == visible)
        return;
    
    var tt = document.getElementById("czPrefTip");
    
    // If we're trying to show it, and we have a reference object
    // (this.tooltipObject), we are ok to go.
    if (visible && this.tooltipObject)
    {
        /* Get the boxObject for the reference object, as we're going to
         * place to tooltip explicitly based on this.
         */
        var tipobjBox = this.tooltipObject.boxObject;
        
        // Adjust the width to that of the reference box.
        tt.sizeTo(tipobjBox.width, tt.boxObject.height);
        /* show tooltip using the reference object, and it's screen 
         * position. NOTE: Most of these parameters are supposed to affect
         * things, and they do seem to matter, but don't work anything like
         * the documentation. Be careful changing them.
         */
        tt.showPopup(this.tooltipObject, -1, -1, "tooltip", "bottomleft", "topleft");
        
        // Set the timer to hide the tooltip some time later...
        // (note the fun inline function)
        this.tooltipTimer = setTimeout(setTooltipState, this.tooltipHideDelay, 
                                       this, false);
        this.tooltipShowing = true;
    }
    else
    {
        /* We're here because either we are meant to be hiding the tooltip,
         * or we lacked the information needed to show one. So hide it.
         */
        tt.hidePopup();
        this.tooltipShowing = false;
    }
}

/** Window event handlers **/

/* Initalises, and loads all the data/utilities and prefs. */
PrefWindow.prototype.onLoad =
function pwin_onLoad()
{
    // Get ourselves a base object for the object hierarchy.
    client = new PrefGlobal();
    
    // Kick off the localisation load.
    initMessages();
    
    // Use localised name.
    client.viewName = MSG_PREFS_GLOBAL;
    client.unicodeName = client.viewName;
    client.prettyName = client.viewName;
    
    // Use the window mediator service to prevent mutliple instances.
    var windowMediator = Components.classes[MEDIATOR_CONTRACTID];
    var windowManager = windowMediator.getService(nsIWindowMediator);
    var enumerator = windowManager.getEnumerator(CONFIG_WINDOWTYPE);
    
    // We only want one open at a time because don't (currently) cope with
    // pref-change notifications. In fact, it's not easy to cope with.
    // Save it for some time later. :)
    
    enumerator.getNext();
    if (enumerator.hasMoreElements())
    {
        alert(MSG_PREFS_ALREADYOPEN);
        window.close();
        return;
    }
    
    // Kick off the core pref initalisation code.
    initPrefs();
    
    // Turn off all notifications, or it'll get confused when any pref 
    // does change.
    client.prefManager.onPrefChanged = function(){};
    
    // The list of objects we're tacking the prefs of.
    this.prefObjects = new PrefObjectList();
    
    /* Oh, this is such an odd way to do this. But hey, it works. :)
     * What we do is ask the pref branch for the client object to give us
     * a list of all the preferences under it. This just gets us all the
     * Chatzilla prefs. Then, we filter them so that we only deal with
     * ones for networks, channels and users. This means, even if only
     * one pref is set for a channel, we get it's network and channel
     * object created here.
     */
    var prefRE = new RegExp("^networks.([^.]+)" +
                            "(?:\\.(users|channels)?\\.([^.]+))?\\.");
    var rv = new Object();
    var netList = client.prefManager.prefBranch.getChildList("networks.", rv);
    for (i in netList)
    {
        var m = netList[i].match(prefRE);
        if (!m)
            continue;
        
        var netName = unMungeNetworkName(m[1]);
        /* We're forcing the network into existance (3rd param) if the
         * pref is actually set, as opposed to just being known about (the
         * pref branch will list all *known* prefs, not just those set).
         *
         * NOTE: |force| will, if |true|, set a property on the object so it
         *       will still exist next time we're here. If |false|, the
         *       the object will only come into existance if this magical
         *       [hidden] pref is set.
         */
        var force = client.prefManager.prefBranch.prefHasUserValue(netList[i]);
        
        // Don't bother with the new if it's already there (time saving).
        if (!(netName in client.networks))
            new PrefNetwork(client, netName, force);
        
        if ((2 in m) && (3 in m) && (netName in client.networks))
        {
            var net = client.networks[netName];
            
            // Create a channel object if appropriate.
            if (m[2] == "channels")
                new PrefChannel(net.primServ, unMungeNetworkName(m[3]), force);
            
            // Create a user object if appropriate.
            if (m[2] == "users")
                new PrefUser(net.primServ, unMungeNetworkName(m[3]), force);
        }
    }
    
    // Store out object that represents the current view.
    var currentView = null;
    
    // Enumerate source window's tab list...
    if (("arguments" in window) && (0 in window.arguments) && ("client" in window.arguments[0]))
    {
        /* Make sure we survive this, external data could be bad. :) */
        try {
            var czWin = window.arguments[0];
            var s;
            var n, c, u;
            
            /* Go nick the source window's view list. We can then show items in
             * the tree for the currently connected/shown networks, channels
             * and users even if they don't have any known prefs yet.
             * 
             * NOTE: the |false, true| params tell the objects to not force 
             *       any prefs into existance, but still show in the tree.
             */
            for (i = 0; i < czWin.client.viewsArray.length; i++)
            {
                var view = czWin.client.viewsArray[i].source;
                
                // Network view...
                if (view.TYPE == "IRCNetwork")
                {
                    n = new PrefNetwork(client, view.unicodeName, false, true);
                    if (view == czWin.client.currentObject)
                        currentView = n;
                }
                
                if (view.TYPE.match(/^IRC(Channel|User)$/))
                    s = client.networks[view.parent.parent.canonicalName].primServ;
                
                // Channel view...
                if (view.TYPE == "IRCChannel")
                {
                    c = new PrefChannel(s, view.unicodeName, false, true);
                    if (view == czWin.client.currentObject)
                        currentView = c;
                }
                
                // User view...
                if (view.TYPE == "IRCUser")
                {
                    u = new PrefUser(s, view.nick, false, true);
                    if (view == czWin.client.currentObject)
                        currentView = u;
                }
            }
        } catch(ex) {}
    }
    
    // Add the client object...
    this.prefObjects.addObject(client);
    // ...and everyone else.
    var i, j;
    /* We sort the keys (property names, i.e. network names). This means the UI
     * will show them in lexographical order, which is good.
     */
    var sortedNets = keys(client.networks).sort();
    for (i = 0; i < sortedNets.length; i++) {
        net = client.networks[sortedNets[i]];
        this.prefObjects.addObject(net);
        
        var sortedChans = keys(net.channels).sort();
        for (j = 0; j < sortedChans.length; j++)
            this.prefObjects.addObject(net.channels[sortedChans[j]]);
        
        var sortedUsers = keys(net.users).sort();
        for (j = 0; j < sortedUsers.length; j++)
            this.prefObjects.addObject(net.users[sortedUsers[j]]);
    }
    
    // Select the first item in the list.
    var prefTree = document.getElementById("pref-tree-object");
    if ("selection" in prefTree.treeBoxObject)
        prefTree.treeBoxObject.selection.select(0);
    else
        prefTree.view.selection.select(0);
    
    // Find and select the current view.
    for (i = 0; i < this.prefObjects.objects.length; i++)
    {
        if (this.prefObjects.objects[i] == currentView)
        {
            if ("selection" in prefTree.treeBoxObject)
                prefTree.treeBoxObject.selection.select(i);
            else
                prefTree.view.selection.select(i);
        }
    }
    
    this.onSelectObject();
    
    // We're done, without error, so it's safe to show the stuff.
    document.getElementById("loadDeck").selectedIndex = 1;
    // This allows [OK] to actually save, without this it'll just close.
    this.loaded = true;
    
    // Force the window to be the right size now, not later.
    window.sizeToContent();
    
    // Center window.
    if (("arguments" in window) && (0 in window.arguments))
    {
        var ow = window.arguments[0];
        
        window.moveTo(ow.screenX + Math.max((ow.outerWidth  - window.outerWidth ) / 2, 0), 
                      ow.screenY + Math.max((ow.outerHeight - window.outerHeight) / 2, 0));
    }
}

/* Closing the window. Clean up. */
PrefWindow.prototype.onClose =
function pwin_onClose()
{
    if (this.loaded)
        destroyPrefs();
}

/* OK button. */
PrefWindow.prototype.onOK =
function pwin_onOK()
{
    if (this.onApply())
        window.close();
    return true;
}

/* Apply button. */
PrefWindow.prototype.onApply =
function pwin_onApply()
{
    // If the load failed, better not to try to save.
    if (!this.loaded)
        return false;
    
    try {
        // Get an array of all the (XUL) items we have to save.
        var list = getPrefTags();
        
        //if (!confirm("There are " + list.length + " pref tags to save. OK?")) return false;
        
        for (var i = 0; i < list.length; i++)
        {
            // Save this one pref...
            var index  = list[i].getAttribute("prefobjectindex");
            var name   = list[i].getAttribute("prefname");
            // Get the private data for the object out, since everything is
            // based on this.
            var po     = this.prefObjects.getObject(index);
            var pref   = po.prefs[name];
            
            var value;
            // We just need to force the value from the DOMString form to
            // the right form, so we don't get anything silly happening.
            switch (pref.type)
            {
                case "string":
                    value = list[i].value;
                    break;
                case "number":
                    value = 1 * list[i].value;
                    break;
                case "boolean":
                    value = list[i].checked;
                    break;
                case "array":
                    var l = new Array();
                    for (var j = 0; j < list[i].childNodes.length; j++)
                        l.push(list[i].childNodes[j].value);
                    value = l;
                    break;
                default:
                    throw "Unknown pref type: " + pref.type + "!";
            }
            /* Fun stuff. We don't want to save if the user hasn't changed
             * it. This is so that if the default is defered, and changed,
             * but this hasn't, we keep the defered default. Which is a
             * Good Thing. :)
             */
            if (((pref.type != "array") && (value != pref.startVal)) || 
                ((pref.type == "array") && 
                 (value.join(";") != pref.startVal.join(";"))))
            {
                po.parent.prefs[pref.name] = value;
            }
            // Update our saved value, so the above check works the 2nd 
            // time the user clicks Apply.
            pref.val = value;
            pref.startVal = pref.val;
        }
        
        return true;
    }
    catch (e)
    {
        alert(getMsg(MSG_PREFS_ERR_SAVE, e));
        return false;
    }
}

/* Cancel button. */
PrefWindow.prototype.onCancel =
function pwin_onCancel()
{
    window.close();
    return true;
}

/** Tooltips' event handlers **/

PrefWindow.prototype.onPrefMouseOver =
function pwin_onPrefMouseOver(object)
{
    this.tooltipObject = object;
    this.tooltipTitle = object.getAttribute("tooltiptitle");
    this.tooltipText = object.getAttribute("tooltipcontent");
    // Reset the timer now we're over a new pref.
    clearTimeout(this.tooltipTimer);
    this.tooltipTimer = setTimeout(setTooltipState, this.tooltipShowDelay, 
                                   this, true);
}

PrefWindow.prototype.onPrefMouseMove =
function pwin_onPrefMouseMove(object)
{
    // If the tooltip isn't showing, we need to reset the timer.
    if (!this.tooltipShowing)
    {
        clearTimeout(this.tooltipTimer);
        this.tooltipTimer = setTimeout(setTooltipState, this.tooltipShowDelay, 
                                       this, true);
    }
}

PrefWindow.prototype.onPrefMouseOut =
function pwin_onPrefMouseOut(object)
{
    // Left the pref! Hide tooltip, and clear timer.
    this.setTooltipState(false);
    clearTimeout(this.tooltipTimer);
}

PrefWindow.prototype.onTooltipPopupShowing =
function pwin_onTooltipPopupShowing(popup)
{
    if (!this.tooltipText)
        return false;
    
    var fChild = popup.firstChild;
    var diff = popup.boxObject.height - fChild.boxObject.height;
    
    // Setup the labels...
    var ttt = document.getElementById("czPrefTipTitle");
    ttt.firstChild.nodeValue = this.tooltipTitle;
    var ttl = document.getElementById("czPrefTipLabel");
    ttl.firstChild.nodeValue = this.tooltipText;
    
    popup.sizeTo(popup.boxObject.width, fChild.boxObject.height + diff);
    
    return true;
}

/** Components' event handlers **/

// Selected an item in the tree.
PrefWindow.prototype.onSelectObject =
function pwin_onSelectObject()
{
    var prefTree = document.getElementById("pref-tree-object");
    var rv = new Object();
    if ("selection" in prefTree.treeBoxObject)
        prefTree.treeBoxObject.selection.getRangeAt(0, rv, {});
    else
        prefTree.view.selection.getRangeAt(0, rv, {});
    var prefTreeIndex = rv.value;
    
    var delButton = document.getElementById("object-delete");
    if (prefTreeIndex > 0)
        delButton.removeAttribute("disabled");
    else
        delButton.setAttribute("disabled", "true");
    
    var prefItem = prefTree.contentView.getItemAtIndex(prefTreeIndex);
    
    var pObjectIndex = prefItem.getAttribute("prefobjectindex");
    var pObject = this.prefObjects.getObject(pObjectIndex);
    
    if (!ASSERT(pObject, "Object not found for index! " + 
                prefItem.getAttribute("prefobjectindex")))
        return;
    
    pObject.loadXUL(this.prefTabOrder);
    
    var header = document.getElementById("pref-header");
    header.setAttribute("title", getMsg(MSG_PREFS_FMT_HEADER, 
                                        pObject.parent.prettyName));
    
    var deck = document.getElementById("pref-object-deck");
    deck.selectedIndex = pObject.deckIndex;
    
    this.currentObject = pObject;
}

// Browse button for file prefs.
PrefWindow.prototype.onPrefBrowse =
function pwin_onPrefBrowse(button)
{
    var spec = "$all";
    if (button.hasAttribute("spec"))
        spec = button.getAttribute("spec") + " " + spec;
    
    var rv = pickOpen(MSG_PREFS_BROWSE_TITLE, spec);
    if (rv.reason != PICK_OK)
        return;
    
    var data = { file: rv.file.path, fileurl: rv.picker.fileURL.spec };
    var edit = button.previousSibling.lastChild;
    var type = button.getAttribute("kind");
    edit.value = data[type];
},

// Selection changed on listbox.
PrefWindow.prototype.onPrefListSelect =
function pwin_onPrefListSelect(object)
{
    var list = getRelatedItem(object, "list");
    var buttons = new Object();
    buttons.up   = getRelatedItem(object, "button-up");
    buttons.down = getRelatedItem(object, "button-down");
    buttons.add  = getRelatedItem(object, "button-add");
    buttons.edit = getRelatedItem(object, "button-edit");
    buttons.del  = getRelatedItem(object, "button-delete");
    
    if (("selectedItems" in list) && list.selectedItems && 
        list.selectedItems.length)
    {
        buttons.edit.removeAttribute("disabled");
        buttons.del.removeAttribute("disabled");
    }
    else
    {
        buttons.edit.setAttribute("disabled", "true");
        buttons.del.setAttribute("disabled", "true");
    }
    
    if (!("selectedItems" in list) || !list.selectedItems || 
        list.selectedItems.length == 0 || list.selectedIndex == 0)
    {
        buttons.up.setAttribute("disabled", "true");
    }
    else
    {
        buttons.up.removeAttribute("disabled");
    }
    
    if (!("selectedItems" in list) || !list.selectedItems || 
        list.selectedItems.length == 0 || 
        list.selectedIndex == list.childNodes.length - 1)
    {
        buttons.down.setAttribute("disabled", "true");
    }
    else
    {
        buttons.down.removeAttribute("disabled");
    }
}

// Up button for lists.
PrefWindow.prototype.onPrefListUp =
function pwin_onPrefListUp(object)
{
    var list = getRelatedItem(object, "list");
    
    var selected = list.selectedItems[0];
    var before = selected.previousSibling;
    if (before)
    {
        before.parentNode.insertBefore(selected, before);
        list.selectItem(selected);
        list.ensureElementIsVisible(selected);
    }
}

// Down button for lists.
PrefWindow.prototype.onPrefListDown =
function pwin_onPrefListDown(object)
{
    var list = getRelatedItem(object, "list");
    
    var selected = list.selectedItems[0];
    if (selected.nextSibling)
    {
        if (selected.nextSibling.nextSibling)
            list.insertBefore(selected, selected.nextSibling.nextSibling);
        else
            list.appendChild(selected);
        
        list.selectItem(selected);
    }
}

// Add button for lists.
PrefWindow.prototype.onPrefListAdd =
function pwin_onPrefListAdd(object)
{
    var list = getRelatedItem(object, "list");
    var newItem;
    
    switch (list.getAttribute("kind"))
    {
        case "url":
            var item = prompt(MSG_PREFS_LIST_ADD);
            if (item)
            {
                newItem = document.createElement("listitem");
                newItem.setAttribute("label", item);
                newItem.value = item;
                list.appendChild(newItem);
                this.onPrefListSelect(object);
            }
            break;
        case "file":
        case "fileurl":
            var spec = "$all";
            
            var rv = pickOpen(MSG_PREFS_BROWSE_TITLE, spec);
            if (rv.reason == PICK_OK)
            {
                var data = { file: rv.file.path, fileurl: rv.picker.fileURL.spec };
                var kind = list.getAttribute("kind");
                
                newItem = document.createElement("listitem");
                newItem.setAttribute("label", data[kind]);
                newItem.value = data[kind];
                list.appendChild(newItem);
                this.onPrefListSelect(object);
            }
            
            break;
    }
}

// Edit button for lists.
PrefWindow.prototype.onPrefListEdit =
function pwin_onPrefListEdit(object)
{
    var list = getRelatedItem(object, "list");
    
    switch (list.getAttribute("kind"))
    {
        case "url":
        case "file":
        case "fileurl":
            // We're letting the user edit file types here, since it saves us 
            // a lot of work, and we can't let them pick a file OR a directory,
            // so they pick a file and can edit it off to use a directory.
            var listItem = list.selectedItems[0];
            var newValue = prompt(MSG_PREFS_LIST_EDIT, listItem.value);
            if (newValue)
            {
                listItem.setAttribute("label", newValue);
                listItem.value = newValue;
            }
            break;
    }
}

// Delete button for lists.
PrefWindow.prototype.onPrefListDelete =
function pwin_onPrefListDelete(object)
{
    var list = getRelatedItem(object, "list");
    
    var listItem = list.selectedItems[0];
    if (confirm(getMsg(MSG_PREFS_LIST_DELETE, listItem.value)))
        list.removeChild(listItem);
}

/* Add... button. */
PrefWindow.prototype.onAddObject =
function pwin_onAddObject()
{
    var rv = new Object();
    
    /* Try to nobble the current selection and pre-fill as needed. */
    switch (this.currentObject.parent.TYPE)
    {
        case "PrefNetwork":
            rv.type = "net";
            rv.net = this.currentObject.parent.unicodeName;
            break;
        case "PrefChannel":
            rv.type = "chan";
            rv.net = this.currentObject.parent.parent.parent.unicodeName;
            rv.chan = this.currentObject.parent.unicodeName;
            break;
        case "PrefUser":
            rv.type = "user";
            rv.net = this.currentObject.parent.parent.parent.unicodeName;
            rv.chan = this.currentObject.parent.unicodeName;
            break;
    }
    
    // Show add dialog, passing the data object along.
    window.openDialog("config-add.xul", "cz-config-add", "chrome,dialog,modal", rv);
    
    if (!rv.ok)
        return;
    
    /* Ok, so what type did they want again?
     * 
     * NOTE: The param |true| in the object creation calls is for |force|. It
     *       causes the hidden pref to be set for the objects so they are shown
     *       every time this window opens, until the user deletes them.
     */
    switch (rv.type)
    {
        case "net":
            this.prefObjects.addObject(new PrefNetwork(client, rv.net, true));
            break;
        case "chan":
            if (!(rv.net in client.networks))
                this.prefObjects.addObject(new PrefNetwork(client, rv.net, true));
            this.prefObjects.addObject(new PrefChannel(client.networks[rv.net].primServ, rv.chan, true));
            break;
        case "user":
            if (!(rv.net in client.networks))
                this.prefObjects.addObject(new PrefNetwork(client, rv.net, true));
            this.prefObjects.addObject(new PrefUser(client.networks[rv.net].primServ, rv.chan, true));
            break;
        default:
            // Oops. Not good, if we got here.
            alert("Unknown pref type: " + rv.type);
    }
}

/* Delete button. */
PrefWindow.prototype.onDeleteObject =
function pwin_onDeleteObject()
{
    // Save current node before we re-select.
    var sel = this.currentObject;
    
    // Check they want to go ahead.
    if (!confirm(getMsg(MSG_PREFS_OBJECT_DELETE, sel.parent.unicodeName)))
        return;
    
    // Select a new item BEFORE removing the current item, so the <tree> 
    // doesn't freak out on us.
    var prefTree = document.getElementById("pref-tree-object");
    if ("selection" in prefTree.treeBoxObject)
        prefTree.treeBoxObject.selection.select(0);
    else
        prefTree.view.selection.select(0);
    
    // If it's a network, nuke all the channels and users too.
    if (sel.parent.TYPE == "PrefNetwork")
    {
        var chans = sel.parent.channels;
        for (k in chans)
            PrefObjectList.getPrivateData(chans[k]).clear();
        
        var users = sel.parent.users;
        for (k in users)
            PrefObjectList.getPrivateData(users[k]).clear();
    }
    sel.clear();
    
    this.onSelectObject();
}

/* Reset button. */
PrefWindow.prototype.onResetObject =
function pwin_onResetObject()
{
    // Save current node before we re-select.
    var sel = this.currentObject;
    
    // Check they want to go ahead.
    if (!confirm(getMsg(MSG_PREFS_OBJECT_RESET, sel.parent.unicodeName)))
        return;
    
    // Reset the prefs.
    sel.reset();
}

// End of PrefWindow. //

/*** Base functions... ***/

/* Gets a "related" items, such as the buttons associated with a list. */
function getRelatedItem(object, thing)
{
    switch (object.nodeName)
    {
        case "listbox":
            switch (thing) {
                case "list":
                    return object;
                case "button-up":
                    return object.nextSibling.childNodes[0];
                case "button-down":
                    return object.nextSibling.childNodes[1];
                case "button-add":
                    return object.nextSibling.childNodes[3];
                case "button-edit":
                    return object.nextSibling.childNodes[4];
                case "button-delete":
                    return object.nextSibling.childNodes[5];
            }
            break;
        case "button":
            var n = object.parentNode.previousSibling;
            if (n)
                return getRelatedItem(n, thing);
            break;
    }
    return null;
}

// Wrap this call so we have the right |this|.
function setTooltipState(w, s)
{
	w.setTooltipState(s);
}

// Reverses the Pref Manager's munging of network names.
function unMungeNetworkName(name)
{
    name = ecmaUnescape(name);
    return name.replace(/_/g, ":").replace(/-/g, ".");
}

// Adds a button to a container, setting up the command in a simple way.
function appendButton(cont, oncommand, attr)
{
    var btn = document.createElement("button");
    if (attr)
        for (var a in attr)
            btn.setAttribute(a, attr[a]);
    if (oncommand)
        btn.setAttribute("oncommand", "gPrefWindow." + oncommand + "(this);");
    else
        btn.setAttribute("disabled", "true");
    cont.appendChild(btn);
}

// Like appendButton, but just drops in a separator.
function appendSeparator(cont, attr)
{
    var spacer = document.createElement("separator");
    if (attr)
        for (var a in attr)
            spacer.setAttribute(a, attr[a]);
    cont.appendChild(spacer);
}

/* This simply collects together all the <textbox>, <checkbox> and <listbox> 
 * elements that have the attribute "prefname". Thus, we generate a list of 
 * all elements that are for prefs.
 */
function getPrefTags()
{
    var rv = new Array();
    var i, list;
    
    list = document.getElementsByTagName("textbox");
    for (i = 0; i < list.length; i++)
    {
        if (list[i].hasAttribute("prefname"))
            rv.push(list[i]);
    }
    list = document.getElementsByTagName("checkbox");
    for (i = 0; i < list.length; i++)
    {
        if (list[i].hasAttribute("prefname"))
            rv.push(list[i]);
    }
    list = document.getElementsByTagName("listbox");
    for (i = 0; i < list.length; i++)
    {
        if (list[i].hasAttribute("prefname"))
            rv.push(list[i]);
    }
    
    return rv;
}

// Sets up the "extra1" button (Apply).
function setupButtons()
{
    // Hacky-hacky-hack. Looks like the new toolkit does provide a solution,
    // but we need to support SeaMonkey too. :)
    
    var dialog = document.documentElement;
    dialog.getButton("extra1").label = dialog.getAttribute("extra1Label");
}

// And finally, we want one of these.
var gPrefWindow = new PrefWindow();
