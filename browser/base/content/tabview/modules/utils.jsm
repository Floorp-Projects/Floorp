(function(){

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// Get this in a way where we can load the page automatically
// where it doesn't need to be focused...
var homeWindow = Cc["@mozilla.org/embedcomp/window-watcher;1"]
    .getService(Ci.nsIWindowWatcher)
    .activeWindow;

var consoleService = Cc["@mozilla.org/consoleservice;1"]
    .getService(Components.interfaces.nsIConsoleService);

var Utils = {
  get activeWindow(){
    var win = Cc["@mozilla.org/embedcomp/window-watcher;1"]
               .getService(Ci.nsIWindowWatcher)
               .activeWindow;
               
    if( win != null ) return win;  
    else return homeWindow;
  },
  
  get activeTab(){
    var tabBrowser = this.activeWindow.gBrowser;
    return tabBrowser.selectedTab;
  },
  
  get homeTab(){
    for( var i=0; i<Tabs.length; i++){
      if(Tabs[i].contentWindow.location.host == "tabcandy"){
        return Tabs[i];
      }
    }
    
    return null;
  },
    
  log: function(value) {
    if(typeof(value) == 'object')
      value = this.expandObject(value);
    
    consoleService.logStringMessage(value);
  }, 
  
  error: function(text) {
    Components.utils.reportError(text);
  }, 
  
  trace: function(text) {
    if(typeof(printStackTrace) != 'function')
      this.log(text + ' trace: you need to include stacktrace.js');
    else {
      var calls = printStackTrace();
      calls.splice(0, 3); // Remove this call and the printStackTrace calls
      this.log(text + ' trace:\n' + calls.join('\n'));
    }
  }, 

  expandObject: function(obj) {
      var s = obj + ' = {';
      for(prop in obj) {
        var value = obj[prop]; 
        s += prop + ": " 
        + (typeof(value) == 'string' ? '\'' : '')
        + value 
        + (typeof(value) == 'string' ? '\'' : '')
        + ", ";
      }
      return s + '}';
    } 
}

window.Utils = Utils;

})();
