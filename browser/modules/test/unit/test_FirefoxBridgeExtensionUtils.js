/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FirefoxBridgeExtensionUtils } = ChromeUtils.importESModule(
  "resource:///modules/FirefoxBridgeExtensionUtils.sys.mjs"
);

const OLD_FIREFOX_SHELL_OPEN_COMMAND_PATH = `${FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL}\\shell\\open\\command`;
const OLD_FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH = `${FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL}\\shell\\open\\command`;
const FIREFOX_SHELL_OPEN_COMMAND_PATH = `${FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL}\\shell\\open\\command`;
const FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH = `${FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL}\\shell\\open\\command`;

class StubbedRegistryKey {
  #children;
  #originalChildren;
  #closeCalled;
  #deletedChildren;
  #openedForRead;
  #values;

  constructor(children, values) {
    this.#children = children;
    this.#values = values;
    this.#originalChildren = new Map(children);

    this.#closeCalled = false;
    this.#openedForRead = false;
    this.#deletedChildren = new Set([]);
  }

  get ACCESS_READ() {
    return 0;
  }

  reset() {
    this.#closeCalled = false;
    this.#deletedChildren = new Set([]);
    this.#children = new Map(this.#originalChildren);
  }

  open(_accessLevel) {
    this.#openedForRead = true;
  }

  get wasOpenedForRead() {
    return this.#openedForRead;
  }

  openChild(path, accessLevel) {
    const result = this.#children.get(path);
    result?.open(accessLevel);
    return result;
  }

  hasChild(path) {
    return this.#children.has(path);
  }

  close() {
    this.#closeCalled = true;
  }

  removeChild(path) {
    this.#deletedChildren.add(path);

    // delete the actual child if it's in there
    this.#children.delete(path);
  }

  isChildDeleted(path) {
    return this.#deletedChildren.has(path);
  }

  getChildName(index) {
    let i = 0;
    for (const [key] of this.#children) {
      if (i == index) {
        return key;
      }
      i++;
    }

    return undefined;
  }

  readStringValue(name) {
    return this.#values.get(name);
  }

  get childCount() {
    return this.#children.size;
  }

  getValueType(entryName) {
    if (typeof this.readStringValue(entryName) == "string") {
      return Ci.nsIWindowsRegKey.TYPE_STRING;
    }

    throw new Error(`${entryName} not found in registry`);
  }

  get wasCloseCalled() {
    return this.#closeCalled;
  }

  getValueName(index) {
    let i = 0;
    for (const [key] of this.#values) {
      if (i == index) {
        return key;
      }
      i++;
    }

    return undefined;
  }

  get valueCount() {
    return this.#values.size;
  }
}

class StubbedDeleteBridgeProtocolRegistryEntryHelper {
  #applicationPath;
  #registryRootKey;

  constructor({ applicationPath, registryRootKey }) {
    this.#applicationPath = applicationPath;
    this.#registryRootKey = registryRootKey;
  }

  getApplicationPath() {
    return this.#applicationPath;
  }

  openRegistryRoot() {
    return this.#registryRootKey;
  }

  deleteRegistryTree(root, toDeletePath) {
    // simplify this for tests
    root.removeChild(toDeletePath);
  }
}

add_task(async function test_DeleteWhenSameFirefoxInstall() {
  for (let protocols of [
    [
      FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL,
      OLD_FIREFOX_SHELL_OPEN_COMMAND_PATH,
      OLD_FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
    [
      FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL,
      FIREFOX_SHELL_OPEN_COMMAND_PATH,
      FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
  ]) {
    let [publicProtocol, privateProtocol, publicPath, privatePath] = protocols;
    const applicationPath = "testPath";

    const firefoxEntries = new Map();
    firefoxEntries.set("", `\"${applicationPath}\" -osint -url \"%1\"`);

    const firefoxProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxEntries
    );

    const firefoxPrivateEntries = new Map();
    firefoxPrivateEntries.set(
      "",
      `\"${applicationPath}\" -osint -private-window \"%1\"`
    );
    const firefoxPrivateProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxPrivateEntries
    );

    const children = new Map();
    children.set(publicPath, firefoxProtocolRegKey);
    children.set(privatePath, firefoxPrivateProtocolRegKey);

    const registryRootKey = new StubbedRegistryKey(children, new Map());

    const stubbedDeleteBridgeProtocolRegistryHelper =
      new StubbedDeleteBridgeProtocolRegistryEntryHelper({
        applicationPath,
        registryRootKey,
      });

    FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
      publicProtocol,
      privateProtocol,
      stubbedDeleteBridgeProtocolRegistryHelper
    );

    ok(registryRootKey.wasCloseCalled, "Root key closed");

    ok(firefoxProtocolRegKey.wasOpenedForRead, "Firefox key opened");
    ok(firefoxProtocolRegKey.wasCloseCalled, "Firefox key closed");
    ok(
      registryRootKey.isChildDeleted(publicProtocol),
      "Firefox protocol registry entry deleted"
    );

    ok(
      firefoxPrivateProtocolRegKey.wasOpenedForRead,
      "Firefox private key opened"
    );
    ok(
      firefoxPrivateProtocolRegKey.wasCloseCalled,
      "Firefox private key closed"
    );
    ok(
      registryRootKey.isChildDeleted(privateProtocol),
      "Firefox private protocol registry entry deleted"
    );
  }
});

add_task(async function test_DeleteWhenDifferentFirefoxInstall() {
  for (let protocols of [
    [
      FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL,
      OLD_FIREFOX_SHELL_OPEN_COMMAND_PATH,
      OLD_FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
    [
      FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL,
      FIREFOX_SHELL_OPEN_COMMAND_PATH,
      FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
  ]) {
    let [publicProtocol, privateProtocol, publicPath, privatePath] = protocols;
    const applicationPath = "testPath";
    const badApplicationPath = "testPath2";

    const firefoxEntries = new Map();
    firefoxEntries.set("", `\"${badApplicationPath}\" -osint -url \"%1\"`);

    const firefoxProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxEntries
    );

    const firefoxPrivateEntries = new Map();
    firefoxPrivateEntries.set(
      "",
      `\"${badApplicationPath}\" -osint -private-window \"%1\"`
    );
    const firefoxPrivateProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxPrivateEntries
    );

    const children = new Map();
    children.set(publicPath, firefoxProtocolRegKey);
    children.set(privatePath, firefoxPrivateProtocolRegKey);

    const registryRootKey = new StubbedRegistryKey(children, new Map());

    const stubbedDeleteBridgeProtocolRegistryHelper =
      new StubbedDeleteBridgeProtocolRegistryEntryHelper({
        applicationPath,
        registryRootKey,
      });

    FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
      publicProtocol,
      privateProtocol,
      stubbedDeleteBridgeProtocolRegistryHelper
    );

    ok(registryRootKey.wasCloseCalled, "Root key closed");

    ok(firefoxProtocolRegKey.wasOpenedForRead, "Firefox key opened");
    ok(firefoxProtocolRegKey.wasCloseCalled, "Firefox key closed");
    ok(
      !registryRootKey.isChildDeleted(publicProtocol),
      "Firefox protocol registry entry not deleted"
    );

    ok(
      firefoxPrivateProtocolRegKey.wasOpenedForRead,
      "Firefox private key opened"
    );
    ok(
      firefoxPrivateProtocolRegKey.wasCloseCalled,
      "Firefox private key closed"
    );
    ok(
      !registryRootKey.isChildDeleted(privateProtocol),
      "Firefox private protocol registry entry not deleted"
    );
  }
});

add_task(async function test_DeleteWhenNoRegistryEntries() {
  for (let protocols of [
    [
      FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL,
      OLD_FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
    [
      FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL,
      FIREFOX_PRIVATE_SHELL_OPEN_COMMAND_PATH,
    ],
  ]) {
    let [publicProtocol, privateProtocol, privatePath] = protocols;
    const applicationPath = "testPath";

    const firefoxPrivateEntries = new Map();
    const firefoxPrivateProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxPrivateEntries
    );

    const children = new Map();
    children.set(privatePath, firefoxPrivateProtocolRegKey);

    const registryRootKey = new StubbedRegistryKey(children, new Map());

    const stubbedDeleteBridgeProtocolRegistryHelper =
      new StubbedDeleteBridgeProtocolRegistryEntryHelper({
        applicationPath,
        registryRootKey,
      });

    FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
      publicProtocol,
      privateProtocol,
      stubbedDeleteBridgeProtocolRegistryHelper
    );

    ok(registryRootKey.wasCloseCalled, "Root key closed");

    ok(
      firefoxPrivateProtocolRegKey.wasOpenedForRead,
      "Firefox private key opened"
    );
    ok(
      firefoxPrivateProtocolRegKey.wasCloseCalled,
      "Firefox private key closed"
    );
    ok(
      !registryRootKey.isChildDeleted(publicProtocol),
      "Firefox protocol registry entry deleted when it shouldn't be"
    );
    ok(
      !registryRootKey.isChildDeleted(privateProtocol),
      "Firefox private protocol registry deleted when it shouldn't be"
    );
  }
});

add_task(async function test_DeleteWhenUnexpectedRegistryEntries() {
  for (let protocols of [
    [
      FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL,
      OLD_FIREFOX_SHELL_OPEN_COMMAND_PATH,
    ],
    [
      FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL,
      FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL,
      FIREFOX_SHELL_OPEN_COMMAND_PATH,
    ],
  ]) {
    let [publicProtocol, privateProtocol, publicPath] = protocols;
    const applicationPath = "testPath";

    const firefoxEntries = new Map();
    firefoxEntries.set("", `\"${applicationPath}\" -osint -url \"%1\"`);
    firefoxEntries.set("extraEntry", "extraValue");
    const firefoxProtocolRegKey = new StubbedRegistryKey(
      new Map(),
      firefoxEntries
    );

    const children = new Map();
    children.set(publicPath, firefoxProtocolRegKey);

    const registryRootKey = new StubbedRegistryKey(children, new Map());

    const stubbedDeleteBridgeProtocolRegistryHelper =
      new StubbedDeleteBridgeProtocolRegistryEntryHelper({
        applicationPath,
        registryRootKey,
      });

    FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
      publicProtocol,
      privateProtocol,
      stubbedDeleteBridgeProtocolRegistryHelper
    );

    ok(registryRootKey.wasCloseCalled, "Root key closed");

    ok(firefoxProtocolRegKey.wasOpenedForRead, "Firefox key opened");
    ok(firefoxProtocolRegKey.wasCloseCalled, "Firefox key closed");
    ok(
      !registryRootKey.isChildDeleted(publicProtocol),
      "Firefox protocol registry entry deleted when it shouldn't be"
    );
    ok(
      !registryRootKey.isChildDeleted(privateProtocol),
      "Firefox private protocol registry deleted when it shouldn't be"
    );
  }
});
