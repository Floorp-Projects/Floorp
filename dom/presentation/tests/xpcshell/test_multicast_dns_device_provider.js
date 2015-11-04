/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* global Services, do_register_cleanup, do_test_pending */

"use strict";

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const INFO_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-info;1";
const PROVIDER_CONTRACT_ID = "@mozilla.org/presentation-device/multicastdns-provider;1";
const SD_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-sd;1";
const UUID_CONTRACT_ID = "@mozilla.org/uuid-generator;1";
const SERVER_CONTRACT_ID = "@mozilla.org/presentation-device/tcp-presentation-server;1";

const PREF_DISCOVERY = "dom.presentation.discovery.enabled";
const PREF_DISCOVERABLE = "dom.presentation.discoverable";
const PREF_DEVICENAME= "dom.presentation.device.name";

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

function MockFactory(aClass) {
  this._cls = aClass;
}
MockFactory.prototype = {
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    switch(typeof(this._cls)) {
      case "function":
        return new this._cls().QueryInterface(aIID);
      case "object":
        return this._cls.QueryInterface(aIID);
      default:
        return null;
    }
  },
  lockFactory: function(aLock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

function ContractHook(aContractID, aClass) {
  this._contractID = aContractID;
  this.classID = Cc[UUID_CONTRACT_ID].getService(Ci.nsIUUIDGenerator).generateUUID();
  this._newFactory = new MockFactory(aClass);

  if (!this.hookedMap.has(this._contractID)) {
    this.hookedMap.set(this._contractID, []);
  }

  this.init();
}

ContractHook.prototype = {
  hookedMap: new Map(), // remember only the most original factory.

  init: function() {
    this.reset();

    let oldContract = this.unregister();
    this.hookedMap.get(this._contractID).push(oldContract);
    registrar.registerFactory(this.classID,
                              "",
                              this._contractID,
                              this._newFactory);

    do_register_cleanup(() => { this.cleanup.apply(this); });
  },

  reset: function() {},

  cleanup: function() {
    this.reset();

    this.unregister();
    let prevContract = this.hookedMap.get(this._contractID).pop();

    if (prevContract.factory) {
      registrar.registerFactory(prevContract.classID,
                                "",
                                this._contractID,
                                prevContract.factory);
    }
  },

  unregister: function() {
    var classID, factory;

    try {
      classID = registrar.contractIDToCID(this._contractID);
      factory = Cm.getClassObject(Cc[this._contractID], Ci.nsIFactory);
    } catch (ex) {
      classID = "";
      factory = null;
    }

    if (factory) {
      registrar.unregisterFactory(classID, factory);
    }

    return { classID: classID, factory: factory };
  }
};

function MockDNSServiceInfo() {}
MockDNSServiceInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceInfo]),

  set host(aHost) {
    this._host = aHost;
  },

  get host() {
    return this._host;
  },

  set address(aAddress) {
    this._address = aAddress;
  },

  get address() {
    return this._address;
  },

  set port(aPort) {
    this._port = aPort;
  },

  get port() {
    return this._port;
  },

  set serviceName(aServiceName) {
    this._serviceName = aServiceName;
  },

  get serviceName() {
    return this._serviceName;
  },

  set serviceType(aServiceType) {
    this._serviceType = aServiceType;
  },

  get serviceType() {
    return this._serviceType;
  },

  set domainName(aDomainName) {
    this._domainName = aDomainName;
  },

  get domainName() {
    return this._domainName;
  },

  set attributes(aAttributes) {
    this._attributes = aAttributes;
  },

  get attributes() {
    return this._attributes;
  }
};

function TestPresentationDeviceListener() {
  this.devices = {};
}
TestPresentationDeviceListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                         Ci.nsISupportsWeakReference]),

  addDevice: function(device) { this.devices[device.id] = device; },
  removeDevice: function(device) { delete this.devices[device.id]; },
  updateDevice: function(device) { this.devices[device.id] = device; },
  onSessionRequest: function(device, url, presentationId, controlChannel) {},

  count: function() {
    var size = 0, key;
    for (key in this.devices) {
        if (this.devices.hasOwnProperty(key)) {
          ++size;
        }
    }
    return size;
  }
};

function createDevice(host, port, serviceName, serviceType, domainName, attributes) {
  let device = new MockDNSServiceInfo();
  device.host = host || "";
  device.port = port || 0;
  device.address = host || "";
  device.serviceName = serviceName || "";
  device.serviceType = serviceType || "";
  device.domainName = domainName || "";
  device.attributes = attributes || null;
  return device;
}

function registerService() {
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);

  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {},
    registerService: function(serviceInfo, listener) {
      this.serviceRegistered++;
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          this.serviceUnregistered++;
        }.bind(this)
      };
    },
    resolveService: function(serviceInfo, listener) {},
    serviceRegistered: 0,
    serviceUnregistered: 0
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);

  Assert.equal(mockObj.serviceRegistered, 0);
  Assert.equal(mockObj.serviceUnregistered, 0);

  // Register
  provider.listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
  };
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 0);

  // Unregister
  provider.listener = null;
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 1);

  run_next_test();
}

function noRegisterService() {
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, false);

  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {},
    registerService: function(serviceInfo, listener) {
      Assert.ok(false, "should not register service if not discoverable");
    },
    resolveService: function(serviceInfo, listener) {},
  };

  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);

  // Try register
  provider.listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
  };
  provider.listener = null;

  run_next_test();
}

function registerServiceDynamically() {
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, false);

  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {},
    registerService: function(serviceInfo, listener) {
      this.serviceRegistered++;
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          this.serviceUnregistered++;
        }.bind(this)
      };
    },
    resolveService: function(serviceInfo, listener) {},
    serviceRegistered: 0,
    serviceUnregistered: 0
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);

  Assert.equal(mockObj.serviceRegistered, 0);
  Assert.equal(mockObj.serviceRegistered, 0);

  // Try Register
  provider.listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
  };
  Assert.equal(mockObj.serviceRegistered, 0);
  Assert.equal(mockObj.serviceUnregistered, 0);

  // Enable registration
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 0);

  // Disable registration
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, false);
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 1);

  // Try unregister
  provider.listener = null;
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 1);

  run_next_test();
}

function addDevice() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");
  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceName, mockDevice.serviceName);
      Assert.equal(serviceInfo.serviceType, mockDevice.serviceType);
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    }
  };

  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = new TestPresentationDeviceListener();
  Assert.equal(listener.count(), 0);

  // Start discovery
  provider.listener = listener;
  Assert.equal(listener.count(), 1);

  // Force discovery again
  provider.forceDiscovery();
  Assert.equal(listener.count(), 1);

  provider.listener = null;
  Assert.equal(listener.count(), 1);

  run_next_test();
}

function handleSessionRequest() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, false);

  const testUrl = "http://example.com";
  const testPresentationId = "test-presentation-id";
  const testDeviceName = "test-device-name";

  Services.prefs.setCharPref(PREF_DEVICENAME, testDeviceName);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");
  let mockSDObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    }
  };

  let mockServerObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPPresentationServer]),
    requestSession: function(deviceInfo, url, presentationId) {
      this.request = {
        deviceInfo: deviceInfo,
        url: url,
        presentationId: presentationId,
      };
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
      };
    },
    id: "",
  };

  let contractHookSD = new ContractHook(SD_CONTRACT_ID, mockSDObj);
  let contractHookServer = new ContractHook(SERVER_CONTRACT_ID, mockServerObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {
      this.device = device;
    },
  };

  provider.listener = listener;

  let controlChannel = listener.device.establishControlChannel(testUrl, testPresentationId);

  Assert.equal(mockServerObj.request.deviceInfo.id, mockDevice.host);
  Assert.equal(mockServerObj.request.deviceInfo.address, mockDevice.host);
  Assert.equal(mockServerObj.request.deviceInfo.port, mockDevice.port);
  Assert.equal(mockServerObj.request.url, testUrl);
  Assert.equal(mockServerObj.request.presentationId, testPresentationId);
  Assert.equal(mockServerObj.id, testDeviceName);

  provider.listener = null;

  run_next_test();
}

function handleOnSessionRequest() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");
  let mockSDObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    }
  };

  let mockServerObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPPresentationServer]),
    startService: function() {},
    sessionRequest: function() {},
    close: function() {},
    id: '',
    port: 0,
    listener: null,
  };

  let contractHookSD = new ContractHook(SD_CONTRACT_ID, mockSDObj);
  let contractHookServer = new ContractHook(SERVER_CONTRACT_ID, mockServerObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
    onSessionRequest: function(device, url, presentationId, controlChannel) {
      Assert.ok(true, "recieve onSessionRequest event");
      this.request = {
        deviceId: device.id,
        url: url,
        presentationId: presentationId,
      };
    }
  };

  provider.listener = listener;

  const deviceInfo = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPDeviceInfo]),
    id: mockDevice.host,
    address: mockDevice.host,
    port: 54321,
  };

  const testUrl = "http://example.com";
  const testPresentationId = "test-presentation-id";
  const testControlChannel = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
  };
  provider.QueryInterface(Ci.nsITCPPresentationServerListener)
          .onSessionRequest(deviceInfo, testUrl, testPresentationId, testControlChannel);

  Assert.equal(listener.request.deviceId, deviceInfo.id);
  Assert.equal(listener.request.url, testUrl);
  Assert.equal(listener.request.presentationId, testPresentationId);

  provider.listener = null;

  run_next_test();
}

function handleOnSessionRequestFromUnknownDevice() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);

  let mockSDObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {},
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {}
  };

  let mockServerObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPPresentationServer]),
    startService: function() {},
    sessionRequest: function() {},
    close: function() {},
    id: '',
    port: 0,
    listener: null,
  };

  let contractHookSD = new ContractHook(SD_CONTRACT_ID, mockSDObj);
  let contractHookServer = new ContractHook(SERVER_CONTRACT_ID, mockServerObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {
      Assert.ok(false, "shouldn't create any new device");
    },
    removeDevice: function(device) {
      Assert.ok(false, "shouldn't remote any device");
    },
    updateDevice: function(device) {
      Assert.ok(false, "shouldn't update any device");
    },
    onSessionRequest: function(device, url, presentationId, controlChannel) {
      Assert.ok(true, "recieve onSessionRequest event");
      this.request = {
        deviceId: device.id,
        url: url,
        presentationId: presentationId,
      };
    }
  };

  provider.listener = listener;

  const deviceInfo = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPDeviceInfo]),
    id: "unknown-device.local",
    address: "unknown-device.local",
    port: 12345,
  };

  const testUrl = "http://example.com";
  const testPresentationId = "test-presentation-id";
  const testControlChannel = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
  };
  provider.QueryInterface(Ci.nsITCPPresentationServerListener)
          .onSessionRequest(deviceInfo, testUrl, testPresentationId, testControlChannel);

  Assert.equal(listener.request.deviceId, deviceInfo.id);
  Assert.equal(listener.request.url, testUrl);
  Assert.equal(listener.request.presentationId, testPresentationId);

  provider.listener = null;

  run_next_test();
}

function noAddDevice() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);

  let mockDevice = createDevice("device.local", 12345, "service.name", "_mozilla_papi._tcp");
  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      Assert.ok(false, "shouldn't perform any device discovery");
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
    }
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);

  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
  };
  provider.listener = listener;
  provider.forceDiscovery();
  provider.listener = null;

  run_next_test();
}

function ignoreSelfDevice() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");
  let mockSDObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {
      listener.onServiceRegistered(createDevice("",
                                                0,
                                                mockDevice.serviceName,
                                                mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceName, mockDevice.serviceName);
      Assert.equal(serviceInfo.serviceType, mockDevice.serviceType);
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    }
  };

  let mockServerObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsITCPPresentationServer]),
    startService: function() {},
    sessionRequest: function() {},
    close: function() {},
    id: '',
    port: 0,
    listener: null,
  };

  let contractHookSD = new ContractHook(SD_CONTRACT_ID, mockSDObj);
  let contractHookServer = new ContractHook(SERVER_CONTRACT_ID, mockServerObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = new TestPresentationDeviceListener();

  // Register service
  provider.listener = listener;
  Assert.equal(mockServerObj.id, mockDevice.host);

  // Start discovery
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);
  Assert.equal(listener.count(), 0);

  provider.listener = null;

  run_next_test();
}
function addDeviceDynamically() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");
  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceName, mockDevice.serviceName);
      Assert.equal(serviceInfo.serviceType, mockDevice.serviceType);
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    }
  };

  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = new TestPresentationDeviceListener();
  provider.listener = listener;
  Assert.equal(listener.count(), 0);

  // Enable discovery
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);
  Assert.equal(listener.count(), 1);

  // Try discovery again
  provider.forceDiscovery();
  Assert.equal(listener.count(), 1);

  // Try discovery once more
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);
  provider.forceDiscovery();
  Assert.equal(listener.count(), 1);

  provider.listener = null;

  run_next_test();
}

function updateDevice() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  let mockDevice1 = createDevice("A.local", 12345, "N1", "_mozilla_papi._tcp");
  let mockDevice2 = createDevice("A.local", 23456, "N2", "_mozilla_papi._tcp");

  let mockObj = {
    discovered: false,

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);

      if (!this.discovered) {
        listener.onServiceFound(mockDevice1);
      } else {
        listener.onServiceFound(mockDevice2);
      }
      this.discovered = true;

      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          listener.onDiscoveryStopped(serviceType);
        }
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceType, "_mozilla_papi._tcp");
      if (serviceInfo.serviceName == "N1") {
        listener.onServiceResolved(mockDevice1);
      } else if (serviceInfo.serviceName == "N2") {
        listener.onServiceResolved(mockDevice2);
      } else {
        Assert.ok(false);
      }
    }
  };

  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),

    addDevice: function(device) {
      Assert.ok(!this.isDeviceAdded);
      Assert.equal(device.id, mockDevice1.host);
      Assert.equal(device.name, mockDevice1.serviceName);
      this.isDeviceAdded = true;
    },
    removeDevice: function(device) { Assert.ok(false); },
    updateDevice: function(device) {
      Assert.ok(!this.isDeviceUpdated);
      Assert.equal(device.id, mockDevice2.host);
      Assert.equal(device.name, mockDevice2.serviceName);
      this.isDeviceUpdated = true;
    },

    isDeviceAdded: false,
    isDeviceUpdated: false
  };
  Assert.equal(listener.isDeviceAdded, false);
  Assert.equal(listener.isDeviceUpdated, false);

  // Start discovery
  provider.listener = listener; // discover: N1

  Assert.equal(listener.isDeviceAdded, true);
  Assert.equal(listener.isDeviceUpdated, false);

  // temporarily disable to stop discovery and re-enable
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  provider.forceDiscovery(); // discover: N2

  Assert.equal(listener.isDeviceAdded, true);
  Assert.equal(listener.isDeviceUpdated, true);

  provider.listener = null;

  run_next_test();
}

function diffDiscovery() {
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  let mockDevice1 = createDevice("A.local", 12345, "N1", "_mozilla_papi._tcp");
  let mockDevice2 = createDevice("B.local", 23456, "N2", "_mozilla_papi._tcp");
  let mockDevice3 = createDevice("C.local", 45678, "N3", "_mozilla_papi._tcp");

  let mockObj = {
    discovered: false,

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);

      if (!this.discovered) {
        listener.onServiceFound(mockDevice1);
        listener.onServiceFound(mockDevice2);
      } else {
        listener.onServiceFound(mockDevice1);
        listener.onServiceFound(mockDevice3);
      }
      this.discovered = true;

      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          listener.onDiscoveryStopped(serviceType);
        }
      };
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceType, "_mozilla_papi._tcp");
      if (serviceInfo.serviceName == "N1") {
        listener.onServiceResolved(mockDevice1);
      } else if (serviceInfo.serviceName == "N2") {
        listener.onServiceResolved(mockDevice2);
      } else if (serviceInfo.serviceName == "N3") {
        listener.onServiceResolved(mockDevice3);
      } else {
        Assert.ok(false);
      }
    }
  };

  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = new TestPresentationDeviceListener();
  Assert.equal(listener.count(), 0);

  // Start discovery
  provider.listener = listener; // discover: N1, N2
  Assert.equal(listener.count(), 2);
  Assert.equal(listener.devices['A.local'].name, mockDevice1.serviceName);
  Assert.equal(listener.devices['B.local'].name, mockDevice2.serviceName);
  Assert.ok(!listener.devices['C.local']);

  // temporarily disable to stop discovery and re-enable
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  provider.forceDiscovery(); // discover: N1, N3, going to remove: N2
  Assert.equal(listener.count(), 3);
  Assert.equal(listener.devices['A.local'].name, mockDevice1.serviceName);
  Assert.equal(listener.devices['B.local'].name, mockDevice2.serviceName);
  Assert.equal(listener.devices['C.local'].name, mockDevice3.serviceName);

  // temporarily disable to stop discovery and re-enable
  Services.prefs.setBoolPref(PREF_DISCOVERY, false);
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  provider.forceDiscovery(); // discover: N1, N3, remove: N2
  Assert.equal(listener.count(), 2);
  Assert.equal(listener.devices['A.local'].name, mockDevice1.serviceName);
  Assert.ok(!listener.devices['B.local']);
  Assert.equal(listener.devices['C.local'].name, mockDevice3.serviceName);

  provider.listener = null;

  run_next_test();
}

function serverClosed() {
  Services.prefs.setBoolPref(PREF_DISCOVERABLE, true);
  Services.prefs.setBoolPref(PREF_DISCOVERY, true);

  let mockDevice = createDevice("device.local",
                                12345,
                                "service.name",
                                "_mozilla_papi._tcp");

  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("",
                                           0,
                                           mockDevice.serviceName,
                                           mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      };
    },
    registerService: function(serviceInfo, listener) {
      this.serviceRegistered++;
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          this.serviceUnregistered++;
        }.bind(this)
      };
    },
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceName, mockDevice.serviceName);
      Assert.equal(serviceInfo.serviceType, mockDevice.serviceType);
      listener.onServiceResolved(createDevice(mockDevice.host,
                                              mockDevice.port,
                                              mockDevice.serviceName,
                                              mockDevice.serviceType));
    },
    serviceRegistered: 0,
    serviceUnregistered: 0
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);

  Assert.equal(mockObj.serviceRegistered, 0);
  Assert.equal(mockObj.serviceUnregistered, 0);

  // Register
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    addDevice: function(device) { this.devices.push(device); },
    removeDevice: function(device) {},
    updateDevice: function(device) {},
    devices: []
  };
  Assert.equal(listener.devices.length, 0);

  provider.listener = listener;
  Assert.equal(mockObj.serviceRegistered, 1);
  Assert.equal(mockObj.serviceUnregistered, 0);
  Assert.equal(listener.devices.length, 1);

  let serverListener = provider.QueryInterface(Ci.nsITCPPresentationServerListener);
  let randomPort = 9527;
  serverListener.onPortChange(randomPort);

  Assert.equal(mockObj.serviceRegistered, 2);
  Assert.equal(mockObj.serviceUnregistered, 1);
  Assert.equal(listener.devices.length, 1);

  // Unregister
  provider.listener = null;
  Assert.equal(mockObj.serviceRegistered, 2);
  Assert.equal(mockObj.serviceUnregistered, 2);
  Assert.equal(listener.devices.length, 1);

  run_next_test();
}

function run_test() {
  let infoHook = new ContractHook(INFO_CONTRACT_ID, MockDNSServiceInfo);

  do_register_cleanup(() => {
    Services.prefs.clearUserPref(PREF_DISCOVERY);
    Services.prefs.clearUserPref(PREF_DISCOVERABLE);
  });

  add_test(registerService);
  add_test(noRegisterService);
  add_test(registerServiceDynamically);
  add_test(addDevice);
  add_test(handleSessionRequest);
  add_test(handleOnSessionRequest);
  add_test(handleOnSessionRequestFromUnknownDevice);
  add_test(noAddDevice);
  add_test(ignoreSelfDevice);
  add_test(addDeviceDynamically);
  add_test(updateDevice);
  add_test(diffDiscovery);
  add_test(serverClosed);

  run_next_test();
}
