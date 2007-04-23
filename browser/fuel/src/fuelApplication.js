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
 * The Original Code is FUEL.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozilla.com> (Original Author)
 *  John Resig  <jresig@mozilla.com> (Original Author)
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
 
const nsISupports = Components.interfaces.nsISupports;
const nsIClassInfo = Components.interfaces.nsIClassInfo;
const nsIObserver = Components.interfaces.nsIObserver;
const fuelIApplication = Components.interfaces.fuelIApplication;


//=================================================
// Shutdown - used to store cleanup functions which will
//            be called on Application shutdown
var gShutdown = [];

//=================================================
// Console constructor
function Console() {
  this._console = Components.classes["@mozilla.org/consoleservice;1"]
    .getService(Components.interfaces.nsIConsoleService);
}

//=================================================
// Console implementation
Console.prototype = {
  log : function cs_log(aMsg) {
    this._console.logStringMessage(aMsg);
  },
  
  open : function cs_open() {
    var wMediator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                              .getService(Components.interfaces.nsIWindowMediator);
    var console = wMediator.getMostRecentWindow("global:console");
    if (!console) {
      var wWatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Components.interfaces.nsIWindowWatcher);
      wWatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                        "chrome,dialog=no,all", cmdLine);
    } else {
      // console was already open
      console.focus();
    }
  }
};


//=================================================
// EventItem constructor
function EventItem(aType, aData) {
  this._type = aType;
  this._data = aData;
}

//=================================================
// EventItem implementation
EventItem.prototype = {
  _cancel : false,
  
  get type() {
    return this._type;
  },
  
  get data() {
    return this._data;
  },
  
  preventDefault : function ei_pd() {
    this._cancel = true;
  }
};


//=================================================
// Events constructor
function Events() {
  this._listeners = [];
}

//=================================================
// Events implementation
Events.prototype = {
  addListener : function evts_al(aEvent, aListener) {
    if (this._listeners.some(hasFilter))
      return;

    this._listeners.push({
      event: aEvent,
      listener: aListener
    });
    
    function hasFilter(element) {
      return element.event == aEvent && element.listener == aListener;
    }
  },
  
  removeListener : function evts_rl(aEvent, aListener) {
    this._listeners = this._listeners.filter(function(element){
      return element.event != aEvent && element.listener != aListener;
    });
  },
  
  dispatch : function evts_dispatch(aEvent, aEventItem) {
    eventItem = new EventItem(aEvent, aEventItem);
    
    this._listeners.forEach(function(key){
      if (key.event == aEvent) {
        key.listener.handleEvent ?
          key.listener.handleEvent(eventItem) :
          key.listener(eventItem);
      }
    });
    
    return !eventItem._cancel;
  }
};


//=================================================
// Preferences constants
const nsIPrefService = Components.interfaces.nsIPrefService;
const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
const nsIPrefBranch2 = Components.interfaces.nsIPrefBranch2;
const nsISupportsString = Components.interfaces.nsISupportsString;

//=================================================
// PreferenceBranch constructor
function PreferenceBranch(aBranch) {
  if (!aBranch)
    aBranch = "";
  
  this._root = aBranch;
  this._prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(nsIPrefService);

  if (aBranch)
    this._prefs = this._prefs.getBranch(aBranch);
    
  this._prefs.QueryInterface(nsIPrefBranch);
  this._prefs.QueryInterface(nsIPrefBranch2);
  
  this._prefs.addObserver(this._root, this, false);
  this._events = new Events();
  
  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// PreferenceBranch implementation
PreferenceBranch.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function prefs_shutdown() {
    this._prefs.removeObserver(this._root, this);
  },
  
  // for nsIObserver
  observe: function prefs_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed")
      this._events.dispatch("change", aData);
  },

  get root() {
    return this._root;
  },
  
  get all() {
    return this.find({});
  },
  
  get events() {
    return this._events;
  },
  
  // XXX: Disabled until we can figure out the wrapped object issues
  // name: "name" or /name/
  // path: "foo.bar." or "" or /fo+\.bar/
  // type: Boolean, Number, String (getPrefType)
  // locked: true, false (prefIsLocked)
  // modified: true, false (prefHasUserValue)
  find : function prefs_find(aOptions) {
    var retVal = [];
    var items = this._prefs.getChildList("", []);
    
    for (var i = 0; i < items.length; i++) {
      retVal.push(new Preference(items[i], this));
    }

    return retVal;
  },
  
  has : function prefs_has(aName) {
    return (this._prefs.getPrefType(aName) != nsIPrefBranch.PREF_INVALID);
  },
  
  get : function prefs_get(aName) {
    return this.has(aName) ? new Preference(aName, this) : null;
  },

  getValue : function prefs_gv(aName, aValue) {
    var type = this._prefs.getPrefType(aName);
    
    switch (type) {
      case nsIPrefBranch2.PREF_STRING:
        aValue = this._prefs.getComplexValue(aName, nsISupportsString).data;
        break;
      case nsIPrefBranch2.PREF_BOOL:
        aValue = this._prefs.getBoolPref(aName);
        break;
      case nsIPrefBranch2.PREF_INT:
        aValue = this._prefs.getIntPref(aName);
        break;
    }
    
    return aValue;
  },
  
  setValue : function prefs_sv(aName, aValue) {
    var type = aValue != null ? aValue.constructor.name : "";
    
    switch (type) {
      case "String":
        var str = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(nsISupportsString);
        str.data = aValue;
        this._prefs.setComplexValue(aName, nsISupportsString, str);
        break;
      case "Boolean":
        this._prefs.setBoolPref(aName, aValue);
        break;
      case "Number":
        this._prefs.setIntPref(aName, aValue);
        break;
      default:
        throw("Unknown preference value specified.");
    }
  },
  
  reset : function prefs_reset() {
    this._prefs.resetBranch("");
  }
};


//=================================================
// Preference constructor
function Preference(aName, aBranch) {
  this._name = aName;
  this._branch = aBranch;
  this._events = new Events();
  
  var self = this;
  
  this.branch.events.addListener("change", function(aEvent){
    if (aEvent.data == self.name)
      self.events.dispatch(aEvent.type, aEvent.data);
  });
}

//=================================================
// Preference implementation
Preference.prototype = {
  get name() {
    return this._name;
  },
  
  get type() {
    var value = "";
    var type = this._prefs.getPrefType(name);
    
    switch (type) {
      case nsIPrefBranch2.PREF_STRING:
        value = "String";
        break;
      case nsIPrefBranch2.PREF_BOOL:
        value = "Boolean";
        break;
      case nsIPrefBranch2.PREF_INT:
        value = "Number";
        break;
    }
    
    return value;
  },
  
  get value() {
    return this.branch.getValue(this._name, null);
  },
  
  set value(aValue) {
    return this.branch.setValue(this._name, aValue);
  },
  
  get locked() {
    return this.branch._prefs.prefIsLocked(this.name);
  },
  
  set locked(aValue) {
    this.branch._prefs[ aValue ? "lockPref" : "unlockPref" ](this.name);
  },
  
  get modified() {
    return this.branch._prefs.prefHasUserValue(this.name);
  },
  
  get branch() {
    return this._branch;
  },
  
  get events() {
    return this._events;
  },
  
  reset : function pref_reset() {
    this.branch._prefs.clearUserPref(this.name);
  }
};


//=================================================
// SessionStorage constructor
function SessionStorage() {
  this._storage = {};
  this._events = new Events();
}

//=================================================
// SessionStorage implementation
SessionStorage.prototype = {
  get events() {
    return this._events;
  },
  
  has : function ss_has(aName) {
    return this._storage.hasOwnProperty(aName);
  },
  
  set : function ss_set(aName, aValue) {
    this._storage[aName] = aValue;
    this._events.dispatch("change", aName);
  },
  
  get : function ss_get(aName, aDefaultValue) {
    return this.has(aName) ? this._storage[aName] : aDefaultValue;
  }
};


//=================================================
// Extension constants
const nsIUpdateItem = Components.interfaces.nsIUpdateItem;

//=================================================
// Extension constructor
function Extension(aItem) {
  this._item = aItem;
  this._firstRun = false;
  this._prefs = new PreferenceBranch("extensions." + this._item.id + ".");
  this._storage = new SessionStorage();
  this._events = new Events();
  
  var installPref = "install-event-fired";
  if (!this._prefs.has(installPref)) {
    this._prefs.setValue(installPref, true);
    this._firstRun = true;
  }

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.addObserver(this, "em-action-requested", false);
  
  var self = this;
  gShutdown.push(function(){ self._shutdown(); });
}

//=================================================
// Extensions implementation
Extension.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function ext_shutdown() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "em-action-requested");
  },
  
  // for nsIObserver  
  observe: function ext_observe(aSubject, aTopic, aData)
  {
    if ((aData == "item-uninstalled") &&
        (aSubject instanceof nsIUpdateItem) &&
        (aSubject.id == this._item.id))
    {
      this._events.dispatch("uninstall", this._item.id);
    }
  },

  get id() {
    return this._item.id;
  },
  
  get name() {
    return this._item.name;
  },
  
  get version() {
    return this._item.version;
  },
  
  get firstRun() {
    return this._firstRun;
  },
  
  get storage() {
    return this._storage;
  },
  
  get prefs() {
    return this._prefs;
  },
  
  get events() {
    return this._events;
  }
};


//=================================================
// Extensions constructor
function Extensions() {
  this._extmgr = Components.classes["@mozilla.org/extensions/manager;1"]
                           .getService(Components.interfaces.nsIExtensionManager);
}

//=================================================
// Extensions implementation
Extensions.prototype = {
  get all() {
    return this.find({});
  },
  
  // XXX: Disabled until we can figure out the wrapped object issues
  // id: "some@id" or /id/
  // name: "name" or /name/
  // version: "1.0.1"
  // minVersion: "1.0"
  // maxVersion: "2.0"
  find : function exts_find(aOptions) {
    var retVal = [];
    var items = this._extmgr.getItemList(nsIUpdateItem.TYPE_EXTENSION, {});
    
    for (var i = 0; i < items.length; i++) {
      retVal.push(new Extension(items[i]));
    }

    return retVal;
  },
  
  has : function exts_has(aId) {
    // getItemForID never returns null for a non-existent id, so we
    // check the type of the returned update item, which should be
    // greater than 1 for a valid extension.
    return !!(this._extmgr.getItemForID(aId).type);
  },
  
  get : function exts_get(aId) {
    return this.has(aId) ? new Extension(this._extmgr.getItemForID(aId)) : null;
  }
};


const CLASS_ID = Components.ID("fe74cf80-aa2d-11db-abbd-0800200c9a66");
const CLASS_NAME = "Application wrapper";
const CONTRACT_ID = "@mozilla.org/application;1";

//=================================================
// Application constructor
function Application() {
  this._console = new Console();
  this._prefs = new PreferenceBranch("");
  this._storage = new SessionStorage();
  this._events = new Events();
  
  this._info = Components.classes["@mozilla.org/xre/app-info;1"]
                     .getService(Components.interfaces.nsIXULAppInfo);
    
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);

  os.addObserver(this, "final-ui-startup", false);
  os.addObserver(this, "quit-application-requested", false);
  os.addObserver(this, "quit-application-granted", false);
  os.addObserver(this, "quit-application", false);
  os.addObserver(this, "xpcom-shutdown", false);
}

//=================================================
// Application implementation
Application.prototype = {
  get id() {
    return this._info.ID;
  },
  
  get name() {
    return this._info.name;
  },
  
  get version() {
    return this._info.version;
  },
  
  // for nsIObserver
  observe: function app_observe(aSubject, aTopic, aData) {
    if (aTopic == "app-startup") {
      this._extensions = new Extensions();
      this._events.dispatch("load", "application");
    }
    else if (aTopic == "final-ui-startup") {
      this._events.dispatch("ready", "application");
    }
    else if (aTopic == "quit-application-requested") {
      // we can stop the quit by checking the return value
      if (this._events.dispatch("quit", "application") == false)
        aSubject.data = true;
    }
    else if (aTopic == "xpcom-shutdown") {
      this._events.dispatch("unload", "application");

      // call the cleanup functions and empty the array
      while (gShutdown.length) {
        gShutdown.shift()();
      }

      // release our observers      
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Components.interfaces.nsIObserverService);

      os.removeObserver(this, "final-ui-startup");

      os.removeObserver(this, "quit-application-requested");
      os.removeObserver(this, "quit-application-granted");
      os.removeObserver(this, "quit-application");
      
      os.removeObserver(this, "xpcom-shutdown");
    }
  },

  // for nsIClassInfo
  classDescription : "Application",
  classID : CLASS_ID,
  contractID : CONTRACT_ID,
  flags : nsIClassInfo.SINGLETON,
  implementationLanguage : Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,

  getInterfaces : function app_gi(aCount) {
    var interfaces = [fuelIApplication, nsIObserver, nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage : function app_ghfl(aCount) {
    return null;
  },
  
  // for nsISupports
  QueryInterface: function app_qi(aIID) {
    // add any other interfaces you support here
    if (aIID.equals(fuelIApplication) ||
        aIID.equals(nsIObserver) ||
        aIID.equals(nsIClassInfo) ||
        aIID.equals(nsISupports))
    {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  
  get console() {
    return this._console;
  },
  
  get storage() {
    return this._storage;
  },
  
  get prefs() {
    return this._prefs;
  },
  
  get extensions() {
    return this._extensions;
  },

  get events() {
    return this._events;
  }
}

//=================================================
// Factory - Treat Application as a singleton
var ApplicationFactory = {
  singleton: null,
  
  createInstance: function af_ci(aOuter, aIID)
  {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
      
    if (this.singleton == null)
      this.singleton = new Application();

    return this.singleton.QueryInterface(aIID);
  }
};

//=================================================
// Module
var ApplicationModule = {
  registerSelf: function am_rs(aCompMgr, aFileSpec, aLocation, aType)
  {
    aCompMgr = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(CLASS_ID, CLASS_NAME, CONTRACT_ID, aFileSpec, aLocation, aType);
    
    // make the Update Service a startup observer
    var categoryManager = Components.classes["@mozilla.org/categorymanager;1"]
                                    .getService(Components.interfaces.nsICategoryManager);
    categoryManager.addCategoryEntry("app-startup", CLASS_NAME, "service," + CONTRACT_ID, true, true);

    // add Application as a global property for easy access                                     
    categoryManager.addCategoryEntry("JavaScript global property", "Application", CONTRACT_ID, true, true);
  },

  unregisterSelf: function am_us(aCompMgr, aLocation, aType)
  {
    aCompMgr = aCompMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(CLASS_ID, aLocation);        
  },
  
  getClassObject: function am_gco(aCompMgr, aCID, aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    if (aCID.equals(CLASS_ID))
      return ApplicationFactory;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload: function am_cu(aCompMgr) { return true; }
};

//module initialization
function NSGetModule(aCompMgr, aFileSpec) { return ApplicationModule; }
