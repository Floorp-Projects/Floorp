
const observerInterface = Components.interfaces.nsIObserverService;
const observerClazz = Components.classes["@mozilla.org/observer-service;1"];
var observerService = observerClazz.getService(observerInterface);

var observer1 =  {
    Observe : function(aSubject, aTopic, someData) {
      print("observer1 notified for: "+aTopic+" with: "+someData);  
    },
    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsISupportsWeakReference)) {
            return this;
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }
}

var observer2 =  {
    Observe : function(aSubject, aTopic, someData) {
      print("observer2 notified for: "+aTopic+" with: "+someData);  
    }
}

const topic = "xpctest_observer_topic";
observerService.AddObserver(observer1, topic);
observerService.AddObserver(observer2, topic);

observerService.Notify(null, topic, "notification 1");
gc();
observer1 = null;
observer2 = null;
observerService.Notify(null, topic, "notification 2");
gc();
observerService.Notify(null, topic, "notification 3");
