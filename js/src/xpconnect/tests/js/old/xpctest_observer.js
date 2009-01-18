
const observerInterface = Components.interfaces.nsIObserverService;
const observerClazz = Components.classes["@mozilla.org/observer-service;1"];
var observerService = observerClazz.getService(observerInterface);

var observer1 =  {
    observe : function(aSubject, aTopic, someData) {
      print("observer1 notified for: "+aTopic+" with: "+someData);
    },
    QueryInterface: function (iid) {
        if (iid.equals(Components.interfaces.nsISupportsWeakReference) ||
            iid.equals(Components.interfaces.nsISupports))
            return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    }
}

var observer2 =  {
    observe : function(aSubject, aTopic, someData) {
      print("observer2 notified for: "+aTopic+" with: "+someData);
    }
}

const topic = "xpctest_observer_topic";
observerService.addObserver(observer1, topic, false);
observerService.addObserver(observer2, topic, false);

observerService.notifyObservers(null, topic, "notification 1");
gc();
observer1 = null;
observer2 = null;
observerService.notifyObservers(null, topic, "notification 2");
gc();
observerService.notifyObservers(null, topic, "notification 3");
