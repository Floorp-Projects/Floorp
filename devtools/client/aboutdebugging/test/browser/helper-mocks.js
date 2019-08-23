/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../shared/test/shared-head.js */

const MOCKS_ROOT = CHROME_URL_ROOT + "mocks/";
/* import-globals-from mocks/helper-adb-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "helper-adb-mock.js", this);
/* import-globals-from mocks/helper-client-wrapper-mock.js */
Services.scriptloader.loadSubScript(
  MOCKS_ROOT + "helper-client-wrapper-mock.js",
  this
);
/* import-globals-from mocks/helper-runtime-client-factory-mock.js */
Services.scriptloader.loadSubScript(
  MOCKS_ROOT + "helper-runtime-client-factory-mock.js",
  this
);

const { RUNTIMES } = require("devtools/client/aboutdebugging/src/constants");

/**
 * This wrapper around the mocks used in about:debugging tests provides helpers to
 * quickly setup mocks for runtime tests involving USB, network or wifi runtimes that can
 * are difficult to setup in a test environment.
 */
class Mocks {
  constructor() {
    // Setup the adb mock to rely on internal arrays.
    this.adbMock = createAdbMock();
    this.adbProcessMock = createAdbProcessMock();
    this._usbRuntimes = [];
    this._usbDevices = [];
    this.adbMock.adb.getRuntimes = () => {
      return this._usbRuntimes;
    };
    this.adbMock.adb.getDevices = () => {
      const runtimeDevices = this._usbRuntimes.map(r => {
        return { id: r.deviceId, name: r.deviceName };
      });
      return runtimeDevices.concat(this._usbDevices);
    };

    // adb.updateRuntimes should ultimately fire the "runtime-list-updated" event.
    this.adbMock.adb.updateRuntimes = () => {
      this.emitUSBUpdate();
    };

    this.adbMock.adb.isProcessStarted = () => true;

    // Prepare a fake observer to be able to emit events from this mock.
    this._observerMock = addObserverMock(this.adbMock.adb);

    // Setup the runtime-client-factory mock to rely on the internal _clients map.
    this.runtimeClientFactoryMock = createRuntimeClientFactoryMock();
    this._clients = {
      [RUNTIMES.NETWORK]: {},
      [RUNTIMES.THIS_FIREFOX]: {},
      [RUNTIMES.USB]: {},
    };
    this.runtimeClientFactoryMock.createClientForRuntime = runtime => {
      return this._clients[runtime.type][runtime.id];
    };

    // Add a client for THIS_FIREFOX, since about:debugging will start on the This Firefox
    // page.
    this._thisFirefoxClient = createThisFirefoxClientMock();
    this._clients[RUNTIMES.THIS_FIREFOX][
      RUNTIMES.THIS_FIREFOX
    ] = this._thisFirefoxClient;

    // Enable mocks and remove them after the test.
    this.enableMocks();
    registerCleanupFunction(() => this.disableMocks());
  }

  get thisFirefoxClient() {
    return this._thisFirefoxClient;
  }

  enableMocks() {
    enableAdbMock(this.adbMock);
    enableAdbProcessMock(this.adbProcessMock);
    enableRuntimeClientFactoryMock(this.runtimeClientFactoryMock);
  }

  disableMocks() {
    disableAdbMock();
    disableAdbProcessMock();
    disableRuntimeClientFactoryMock();

    for (const host of Object.keys(this._clients[RUNTIMES.NETWORK])) {
      this.removeNetworkRuntime(host);
    }
  }

  createNetworkRuntime(host, runtimeInfo) {
    const {
      addNetworkLocation,
    } = require("devtools/client/aboutdebugging/src/modules/network-locations");
    addNetworkLocation(host);

    // Add a valid client that can be returned for this particular runtime id.
    const mockNetworkClient = createClientMock();
    mockNetworkClient.getDeviceDescription = () => {
      return {
        name: runtimeInfo.name || "TestBrand",
        channel: runtimeInfo.channel || "release",
        version: runtimeInfo.version || "1.0",
      };
    };
    this._clients[RUNTIMES.NETWORK][host] = mockNetworkClient;

    return mockNetworkClient;
  }

  removeNetworkRuntime(host) {
    const {
      removeNetworkLocation,
    } = require("devtools/client/aboutdebugging/src/modules/network-locations");
    removeNetworkLocation(host);

    delete this._clients[RUNTIMES.NETWORK][host];
  }

  emitUSBUpdate() {
    this._observerMock.emit("runtime-list-updated");
  }

  /**
   * Creates a USB runtime for which a client conenction can be established.
   * @param {String} id
   *        The id of the runtime.
   * @param {Object} optional object used to create the fake runtime & device
   *        - channel: {String} Release channel, for instance "release", "nightly"
   *        - clientWrapper: {ClientWrapper} optional ClientWrapper for this runtime
   *        - deviceId: {String} Device id
   *        - deviceName: {String} Device name
   *        - isFenix: {Boolean} set by ADB if the package name matches a Fenix package
   *        - name: {String} Application name, for instance "Firefox"
   *        - shortName: {String} Short name for the device
   *        - socketPath: {String} (should only be used for connecting, so not here)
   *        - version: {String} Version, for instance "63.0a"
   *        - versionName: {String} Version return by ADB "63.0a"
   * @return {Object} Returns the mock client created for this runtime so that methods
   * can be overridden on it.
   */
  createUSBRuntime(id, runtimeInfo = {}) {
    // Add a new runtime to the list of scanned runtimes.
    this._usbRuntimes.push({
      deviceId: runtimeInfo.deviceId || "test device id",
      deviceName: runtimeInfo.deviceName || "test device name",
      id: id,
      isFenix: runtimeInfo.isFenix,
      shortName: runtimeInfo.shortName || "testshort",
      socketPath: runtimeInfo.socketPath || "test/path",
      versionName: runtimeInfo.versionName || "1.0",
    });

    // Add a valid client that can be returned for this particular runtime id.
    let mockUsbClient = runtimeInfo.clientWrapper;
    if (!mockUsbClient) {
      // If no clientWrapper was provided, create a mock client here.
      mockUsbClient = createClientMock();
      mockUsbClient.getDeviceDescription = () => {
        return {
          channel: runtimeInfo.channel || "release",
          name: runtimeInfo.name || "TestBrand",
          version: runtimeInfo.version || "1.0",
        };
      };
    }

    this._clients[RUNTIMES.USB][id] = mockUsbClient;

    return mockUsbClient;
  }

  removeUSBRuntime(id) {
    this._usbRuntimes = this._usbRuntimes.filter(runtime => runtime.id !== id);
    delete this._clients[RUNTIMES.USB][id];
  }

  addDevice(deviceId, deviceName) {
    this._usbDevices.push({
      id: deviceId,
      name: deviceName,
    });
  }

  removeDevice(deviceId) {
    this._usbDevices = this._usbDevices.filter(d => {
      return d.id !== deviceId;
    });
  }

  removeRuntime(id) {
    if (this._clients[RUNTIMES.USB][id]) {
      this.removeUSBRuntime(id);
    } else if (this._clients[RUNTIMES.NETWORK][id]) {
      this.removeNetworkRuntime(id);
    }
  }
}
/* exported Mocks */

const silenceWorkerUpdates = function() {
  const {
    removeMockedModule,
    setMockedModule,
  } = require("devtools/client/shared/browser-loader-mocks");

  const mock = {
    WorkersListener: () => {
      return {
        addListener: () => {},
        removeListener: () => {},
      };
    },
  };
  setMockedModule(mock, "devtools/client/shared/workers-listener");

  registerCleanupFunction(() => {
    removeMockedModule("devtools/client/shared/workers-listener");
  });
};
/* exported silenceWorkerUpdates */
