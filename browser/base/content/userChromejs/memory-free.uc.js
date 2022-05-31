// ==UserScript==
// @name           memoryMinimizationButton.uc.js
// @namespace      http://space.geocities.yahoo.co.jp/gl/alice0775
// @description    memory minimization button
// @charset        utf-8
// @include        main
// @compatibility  Firefox 100
// @author         Alice0775
// @version        2022/04/01 23:00 Convert Components.utils.import to ChromeUtils.import
// @version        2018/10/09 00:00 fix CSS
// @version        2018/09/07 23:00 fix initial visual status
// ==/UserScript==
var memoryMinimizationButton = {
  get memoryMinimizationButton(){
    return document.getElementById("memoryMinimizationButton");
  },

  get statusinfo() {
    if ("StatusPanel" in window) {
      // fx61+
      return StatusPanel._labelElement.value;
    } else {
      return XULBrowserWindow.statusTextField.label;
    }
  },

  set statusinfo(val) {
    if ("StatusPanel" in window) {
      // fx61+
      StatusPanel._label = val;
    } else {
      XULBrowserWindow.statusTextField.label = val;
    }
    if(this._statusinfotimer)
      clearTimeout(this._statusinfotimer);
    this._statusinfotimer = setTimeout(() => {this.hideStatusInfo();}, 2000);
    this._laststatusinfo = val;
    return val;
  },

  init: function() {
    let style = `
      #memoryMinimizationButton {
          width: 16px;
          height: 16px;
        list-style-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAYdEVYdFNvZnR3YXJlAHBhaW50Lm5ldCA0LjEuMWMqnEsAAAFdSURBVFhH7VbLbsMwDMsef5dc8z/53F13GXZYm5kaZciqnKro2hxaAkRkmkqI2gI6KNZ1vStPEJluyS6WZVlB3xDpWc3q/Ewf6ASClwicLkhojc5PxaDvgQIU4dWSPjEa/Z1y1ed5fqPUeCmJVhAGUC9YNzz4ghNEelYDrI5aAozjeNyD5QBqgF34DJAN8GN4oGb3cJ527fftumEqgAVurdk7Um680zR92bXWEdMBWB+s14+TegFf93hpgF5dgylK/cFSPSGvOgKA9afWgP4y+lR/xKuOwMN6AdwFXff4H0eAqfhWnU9Mhdx+rrtMBSjER3TUtLYaqOMYjaldN8wGuBmfAWqAcmMPe1ACYF6lIBUZXXFO29I3/5IVoFMpcLoA2iX96gVLiBb0bb6gINSy/WLsgb4HDgBD0BzqXsv0i8nDm+/JCC8lKW5zY4SGvT9LBdbVS0g/64q4fxh+AZvdJHHKcZdFAAAAAElFTkSuQmCC');
      }
      @media (min-resolution: 1.1dppx) {
        #memoryMinimizationButton {
          width: 32px;
          height: 32px;
        }
      }
     `.replace(/\s+/g, " ");

    let sss = Components.classes['@mozilla.org/content/style-sheet-service;1']
                .getService(Components.interfaces.nsIStyleSheetService);
    let newURIParam = {
        aURL: 'data:text/css,' + encodeURIComponent(style),
        aOriginCharset: null,
        aBaseURI: null
    }
    let cssUri = Services.io.newURI(newURIParam.aURL, newURIParam.aOriginCharset, newURIParam.aBaseURI);
    if (!sss.sheetRegistered(cssUri, sss.AUTHOR_SHEET))
      sss.loadAndRegisterSheet(cssUri, sss.AUTHOR_SHEET);

    if (this.memoryMinimizationButton)
      return;

    ChromeUtils.import("resource:///modules/CustomizableUI.jsm");
    try {
      CustomizableUI.createWidget({ //must run createWidget before windowListener.register because the register function needs the button added first
        id: 'memoryMinimizationButton',
        type:  'custom',
        defaultArea: CustomizableUI.AREA_NAVBAR,
        onBuild: function(aDocument) {
          var toolbaritem = aDocument.createElementNS('http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul', 'toolbarbutton');
          var props = {
            id: "memoryMinimizationButton",
            class: "toolbarbutton-1 chromeclass-toolbar-additional",
            tooltiptext: "Memory minimization",
            oncommand: "memoryMinimizationButton.doMinimize(event);",
            type: CustomizableUI.TYPE_TOOLBAR,
            label: "Memory minimization",
            removable: "true"
          };
          for (var p in props) {
            toolbaritem.setAttribute(p, props[p]);
          }
          
          return toolbaritem;
        },
      });
    } catch(ee) {}
  },

  doMinimize: function(event) {
    function doGlobalGC()  {
       Services.obs.notifyObservers(null, "child-gc-request");
       Cu.forceGC();
    }

    function doCC()  {
      Services.obs.notifyObservers(null, "child-cc-request");
      if (typeof window.windowUtils != "undefined")
        window.windowUtils.cycleCollect();
      else
      window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
            .getInterface(Components.interfaces.nsIDOMWindowUtils).cycleCollect();
    }

    function doMemMinimize() {
      Services.obs.notifyObservers(null, "child-mmu-request");
      var gMgr = Cc["@mozilla.org/memory-reporter-manager;1"]
             .getService(Ci.nsIMemoryReporterManager);
      gMgr.minimizeMemoryUsage(() => memoryMinimizationButton.displayStatus("Memory minimization done"));
    }
    function sendHeapMinNotifications()  {
      function runSoon(f) {
        var tm = Cc["@mozilla.org/thread-manager;1"]
                  .getService(Ci.nsIThreadManager);

        tm.mainThread.dispatch({ run: f }, Ci.nsIThread.DISPATCH_NORMAL);
      }

      function sendHeapMinNotificationsInner() {
        var os = Cc["@mozilla.org/observer-service;1"]
                 .getService(Ci.nsIObserverService);
        os.notifyObservers(null, "memory-pressure", "heap-minimize");

        if (++j < 3)
          runSoon(sendHeapMinNotificationsInner);
      }

      var j = 0;
      sendHeapMinNotificationsInner();
    }

    this.displayStatus("Memory minimization start")
    doGlobalGC();
    doCC();
    //sendHeapMinNotifications();
    setTimeout(()=> {doMemMinimize();}, 1000);
  },
  
  _statusinfotimer: null,
  _laststatusinfo: null,
  displayStatus: function(val) {
    this.statusinfo = val;
  },
  hideStatusInfo: function() {
    if(this._statusinfotimer)
      clearTimeout(this._statusinfotimer);
    this._statusinfotimer = null;
    if (this._laststatusinfo == this.statusinfo)
      this.statusinfo = "";
  }

}

  memoryMinimizationButton.init();