// Mock Idle Service

//Global Variables
var numIdleObserversRemoved = 0;
var numIdleObserversAdded = 0;
var addedIdleObserver = false;
var removedIdleObserver = false;

var idleServiceCID;
var idleServiceContractID;
var oldIdleService;
var componentMgr;
var oldIdleServiceFactoryObj;
var oldIdleServiceCID;
var window;

/*******************************************************************************
* Class Mock Idle Service
*******************************************************************************/
var idleServiceObj = {
  observers: [],
  windowObservers: [],
  idleTimeInMS: 5000,   // In milli seconds

  // Takes note of the idle observers added as the minimum idle observer
  // with the idle service
  timesAdded: [],

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.nsIIdleService)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function(outer, iid) {
    return this.QueryInterface(iid);
  },

  get idleTime() {
    return this.idleTimeInMS;
  },

  set idleTime(timeInMS) {
    this.idleTimeInMS = timeInMS;
  },

  getWindowFromObserver: function(observer) {
    try {
      var interfaceRequestor = observer.
            QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      window = interfaceRequestor.
            getInterface(Components.interfaces.nsIDOMWindow);
      return window;
    }
    catch (e) {}

    return null;
  },

  testIdleBackService: function(observer, topic) {
    dump("\nMOCK IDLE SERVICE: testIdleBackService()\n");
    window = this.getWindowFromObserver(observer);
    dump("window in mock idle service is: " + window + "\n");
    dump("JS NUM OBSERVERS: " + this.observers.length + "\n");

    for (i=1; i<this.observers.length; i++) {
      this.observers[i].observer.observe(observer, topic, '\0');
      dump("JS CALLED OBSERVE FUNCTION!!!\n\n");
    }
  },

  addIdleObserver: function(observer, time) {
    dump("\nJS FAKE IDLE SERVICE add idle observer before\n");
    dump("JS NUM OBSERVERS: " + this.observers.length + "\n");

    window = this.getWindowFromObserver(observer);

    if (window) {
    dump("window is: " + window + "\n");
      this.observers.push({ observer: observer, time: time, });
      addedIdleObserver = true;
      numIdleObserversAdded++;
      this.timesAdded.push(time);

      dump("\nMOCK IDLE SERVICE ADDING idle observer with time: " + time + "\n");
      dump("MOCK IDLE SERVICE: num idle observers added: " +
           numIdleObserversAdded + "\n\n");
    }
    else {
      dump("SHOULD NEVER GET HERE!");
      oldIdleService.addIdleObserver(observer, time);
      addedIdleObserver = false;
    }

    dump("\nJS FAKE IDLE SERVICE end of add idle observer\n");
    dump("JS NUM OBSERVERS: " + this.observers.length + "\n");
  },

  removeIdleObserver: function(observer, time) {
    dump("\nJS REMOVE IDLE OBSERVER () time to be removed: " + time + "\n\n\n");
    window = this.getWindowFromObserver(observer);
    if (!window) {
      oldIdleService.removeIdleObserver(observer, time);
    }
    else {
      var observerIndex = -1;
      for (var i=0; i<this.observers.length; i++) {
        dump("JS removeIdleObserver() observer time: " +
             this.observers[i].time + "\n");
        if (this.observers[i].time === time) {
          observerIndex = i;
          break;
        }
      }

      if (observerIndex != -1 && this.observers.length > 0) {
        numIdleObserversRemoved++;
        this.observers.splice(observerIndex, 1);
        removedIdleObserver = true;
        dump("MOCK IDLE SERVICE REMOVING idle observer with time " +
             time + "\n");
        dump("MOCK IDLE SERVICE numIdleObserversRemoved: " +
          numIdleObserversRemoved + " numIdleObserversAdded: " +
          numIdleObserversAdded + "\n\n");
      }
      else {
        removedIdleObserver = false;
      }
    }
    dump("\nJS FAKE IDLE SERVICE end of remove idle observer\n");
    dump("JS NUM OBSERVERS: " + this.observers.length + "\n");
  },
};

/*******************************************************************************
* RegisterMockIdleService()
*******************************************************************************/
function RegisterMockIdleService() {
  try {
    idleServiceCID = Components.ID("287075a6-f968-4516-8043-406c46f503b4");
    idleServiceContractID = "@mozilla.org/widget/idleservice;1";
    oldIdleService = Components.classes[idleServiceContractID].
          getService(Components.interfaces.nsIIdleService);
  }
  catch(ex) {
    dump("MockIdleService.js: 1) Failed to get old idle service.\n");
  }

  try {
    // Registering new moch JS idle service
    componentMgr = Components.manager.
          QueryInterface(Components.interfaces.nsIComponentRegistrar);
  }
  catch(err) {
    dump("MockIdleService.js: Failed to query component registrar interface.\n");
  }

  try {
    oldIdleServiceFactoryObj =
      componentMgr.getClassObjectByContractID(idleServiceContractID,
                                              Components.interfaces.nsIFactory);
  }
  catch(err) {
    dump("MockIdleService.js: Failed to get old idle service.\n");
  }

  try {
    oldIdleServiceCID = componentMgr.contractIDToCID(idleServiceContractID);
  }
  catch(err) {
    dump("MockIdleService.js: " +
         "Failed to convert ID to CID for old idle service.\n");
  }

  try {
    componentMgr.unregisterFactory(oldIdleServiceCID, oldIdleServiceFactoryObj);
  }
  catch(err) {
    dump("MockIdleService.js: " +
         "Failed to unregister old idle service factory object!\n");
  }

  try {
    componentMgr.registerFactory(idleServiceCID,
      "Test Simple Idle/Back Notifications",
      idleServiceContractID, idleServiceObj);
  }
  catch(err) {
    dump("MockIdleService.js: Failed to register mock idle service.\n");
  }
}

/*******************************************************************************
* FinishTest()
*******************************************************************************/
function FinishTest() {
  SpecialPowers.setBoolPref("browser.sessionhistory.cache_subframes", false);

  try {
    componentMgr.unregisterFactory(idleServiceCID, idleServiceObj);
  }
  catch(err) {
    dump("MockIdleService.js: ShiftLocalTimerBackCleanUp() " +
         "Failed to unregister factory, mock idle service!\n");
  }

  try {
    componentMgr.registerFactory(oldIdleServiceCID,
      "Re registering old idle service",
      idleServiceContractID, oldIdleServiceFactoryObj);
  }
  catch(err) {
    dump("MockIdleService.js: ShiftLocalTimerBackCleanUp() " +
         "Failed to register factory, original idle service!\n");
  }

  ok(true, "Failed test 769726");
  SimpleTest.finish();
}

