/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  Subprocess: "resource://gre/modules/Subprocess.sys.mjs",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "XreDirProvider",
  "@mozilla.org/xre/directory-provider;1",
  "nsIXREDirProvider"
);

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.sys.mjs for details.
    maxLogLevel: "error",
    maxLogLevelPref: "browser.shell.loglevel",
    prefix: "ShellService",
  };
  return new ConsoleAPI(consoleOptions);
});

/**
 * Internal functionality to save and restore the docShell.allow* properties.
 */
let ShellServiceInternal = {
  /**
   * Used to determine whether or not to offer "Set as desktop background"
   * functionality. Even if shell service is available it is not
   * guaranteed that it is able to set the background for every desktop
   * which is especially true for Linux with its many different desktop
   * environments.
   */
  get canSetDesktopBackground() {
    if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
      return true;
    }

    if (AppConstants.platform == "linux") {
      if (this.shellService) {
        let linuxShellService = this.shellService.QueryInterface(
          Ci.nsIGNOMEShellService
        );
        return linuxShellService.canSetDesktopBackground;
      }
    }

    return false;
  },

  isDefaultBrowserOptOut() {
    if (AppConstants.platform == "win") {
      let optOutValue = lazy.WindowsRegistry.readRegKey(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        "Software\\Mozilla\\Firefox",
        "DefaultBrowserOptOut"
      );
      lazy.WindowsRegistry.removeRegKey(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        "Software\\Mozilla\\Firefox",
        "DefaultBrowserOptOut"
      );
      if (optOutValue == "True") {
        Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", false);
        return true;
      }
    }
    return false;
  },

  /**
   * Used to determine whether or not to show a "Set Default Browser"
   * query dialog. This attribute is true if the application is starting
   * up and "browser.shell.checkDefaultBrowser" is true, otherwise it
   * is false.
   */
  _checkedThisSession: false,
  get shouldCheckDefaultBrowser() {
    // If we've already checked, the browser has been started and this is a
    // new window open, and we don't want to check again.
    if (this._checkedThisSession) {
      return false;
    }

    if (!Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser")) {
      return false;
    }

    if (this.isDefaultBrowserOptOut()) {
      return false;
    }

    return true;
  },

  set shouldCheckDefaultBrowser(shouldCheck) {
    Services.prefs.setBoolPref(
      "browser.shell.checkDefaultBrowser",
      !!shouldCheck
    );
  },

  isDefaultBrowser(startupCheck, forAllTypes) {
    // If this is the first browser window, maintain internal state that we've
    // checked this session (so that subsequent window opens don't show the
    // default browser dialog).
    if (startupCheck) {
      this._checkedThisSession = true;
    }
    if (this.shellService) {
      return this.shellService.isDefaultBrowser(forAllTypes);
    }
    return false;
  },

  /*
   * Invoke the Windows Default Browser agent with the given options.
   *
   * Separated for easy stubbing in tests.
   */
  _callExternalDefaultBrowserAgent(options = {}) {
    const wdba = Services.dirsvc.get("XREExeF", Ci.nsIFile);
    wdba.leafName = "default-browser-agent.exe";
    return lazy.Subprocess.call({
      ...options,
      command: options.command || wdba.path,
    });
  },

  /*
   * Check if UserChoice is impossible.
   *
   * Separated for easy stubbing in tests.
   *
   * @return string telemetry result like "Err*", or null if UserChoice
   * is possible.
   */
  _userChoiceImpossibleTelemetryResult() {
    if (!ShellService.checkAllProgIDsExist()) {
      return "ErrProgID";
    }
    if (!ShellService.checkBrowserUserChoiceHashes()) {
      return "ErrHash";
    }
    return null;
  },

  /*
   * Accommodate `setDefaultPDFHandlerOnlyReplaceBrowsers` feature.
   * @return true if Firefox should set itself as default PDF handler, false
   * otherwise.
   */
  _shouldSetDefaultPDFHandler() {
    if (
      !lazy.NimbusFeatures.shellService.getVariable(
        "setDefaultPDFHandlerOnlyReplaceBrowsers"
      )
    ) {
      return true;
    }

    const handler = this.getDefaultPDFHandler();
    if (handler === null) {
      // We only get an exception when something went really wrong.  Fail
      // safely: don't set Firefox as default PDF handler.
      lazy.log.warn(
        "Could not determine default PDF handler: not setting Firefox as " +
          "default PDF handler!"
      );
      return false;
    }

    if (!handler.registered) {
      lazy.log.debug(
        "Current default PDF handler has no registered association; " +
          "should set as default PDF handler."
      );
      return true;
    }

    if (handler.knownBrowser) {
      lazy.log.debug(
        "Current default PDF handler progID matches known browser; should " +
          "set as default PDF handler."
      );
      return true;
    }

    lazy.log.debug(
      "Current default PDF handler progID does not match known browser " +
        "prefix; should not set as default PDF handler."
    );
    return false;
  },

  getDefaultPDFHandler() {
    const knownBrowserPrefixes = [
      "AppXq0fevzme2pys62n3e0fbqa7peapykr8v", // Edge before Blink, per https://stackoverflow.com/a/32724723.
      "Brave", // For "BraveFile".
      "Chrome", // For "ChromeHTML".
      "Firefox", // For "FirefoxHTML-*" or "FirefoxPDF-*".  Need to take from other installations of Firefox!
      "IE", // Best guess.
      "MSEdge", // For "MSEdgePDF".  Edgium.
      "Opera", // For "OperaStable", presumably varying with channel.
      "Yandex", // For "YandexPDF.IHKFKZEIOKEMR6BGF62QXCRIKM", presumably varying with installation.
    ];

    let currentProgID = "";
    try {
      // Returns the empty string when no association is registered, in
      // which case the prefix matching will fail and we'll set Firefox as
      // the default PDF handler.
      currentProgID = this.queryCurrentDefaultHandlerFor(".pdf");
    } catch (e) {
      // We only get an exception when something went really wrong.  Fail
      // safely: don't set Firefox as default PDF handler.
      lazy.log.warn("Failed to queryCurrentDefaultHandlerFor:");
      return null;
    }

    if (currentProgID == "") {
      return { registered: false, knownBrowser: false };
    }

    const knownBrowserPrefix = knownBrowserPrefixes.find(it =>
      currentProgID.startsWith(it)
    );

    if (knownBrowserPrefix) {
      lazy.log.debug(`Found known browser prefix: ${knownBrowserPrefix}`);
    }

    return {
      registered: true,
      knownBrowser: !!knownBrowserPrefix,
    };
  },

  /*
   * Set the default browser through the UserChoice registry keys on Windows.
   *
   * NOTE: This does NOT open the System Settings app for manual selection
   * in case of failure. If that is desired, catch the exception and call
   * setDefaultBrowser().
   *
   * @return Promise, resolves when successful, rejects with Error on failure.
   */
  async setAsDefaultUserChoice() {
    if (AppConstants.platform != "win") {
      throw new Error("Windows-only");
    }

    lazy.log.info("Setting Firefox as default using UserChoice");

    // We launch the WDBA to handle the registry writes, see
    // SetDefaultBrowserUserChoice() in
    // toolkit/mozapps/defaultagent/SetDefaultBrowser.cpp.
    // This is external in case an overzealous antimalware product decides to
    // quarrantine any program that writes UserChoice, though this has not
    // occurred during extensive testing.

    let telemetryResult = "ErrOther";

    try {
      telemetryResult =
        this._userChoiceImpossibleTelemetryResult() ?? "ErrOther";
      if (telemetryResult == "ErrProgID") {
        throw new Error("checkAllProgIDsExist() failed");
      }
      if (telemetryResult == "ErrHash") {
        throw new Error("checkBrowserUserChoiceHashes() failed");
      }

      const aumi = lazy.XreDirProvider.getInstallHash();

      telemetryResult = "ErrLaunchExe";
      const exeArgs = ["set-default-browser-user-choice", aumi];
      if (
        lazy.NimbusFeatures.shellService.getVariable("setDefaultPDFHandler")
      ) {
        if (this._shouldSetDefaultPDFHandler()) {
          lazy.log.info("Setting Firefox as default PDF handler");
          exeArgs.push(".pdf", "FirefoxPDF");
        } else {
          lazy.log.info("Not setting Firefox as default PDF handler");
        }
      }
      const exeProcess = await this._callExternalDefaultBrowserAgent({
        arguments: exeArgs,
      });
      telemetryResult = "ErrOther";
      await this._handleWDBAResult(exeProcess);
      telemetryResult = "Success";
    } catch (ex) {
      if (ex instanceof WDBAError) {
        telemetryResult = ex.telemetryResult;
      }

      throw ex;
    } finally {
      try {
        const histogram = Services.telemetry.getHistogramById(
          "BROWSER_SET_DEFAULT_USER_CHOICE_RESULT"
        );
        histogram.add(telemetryResult);
      } catch (ex) {}
    }
  },

  async setAsDefaultPDFHandlerUserChoice() {
    if (AppConstants.platform != "win") {
      throw new Error("Windows-only");
    }

    // See comment in setAsDefaultUserChoice for an explanation of why we shell
    // out to WDBA.
    let telemetryResult = "ErrOther";

    try {
      const aumi = lazy.XreDirProvider.getInstallHash();
      const exeProcess = await this._callExternalDefaultBrowserAgent({
        arguments: [
          "set-default-extension-handlers-user-choice",
          aumi,
          ".pdf",
          "FirefoxPDF",
        ],
      });
      telemetryResult = "ErrOther";
      await this._handleWDBAResult(exeProcess);
      telemetryResult = "Success";
    } catch (ex) {
      if (ex instanceof WDBAError) {
        telemetryResult = ex.telemetryResult;
      }

      throw ex;
    } finally {
      try {
        const histogram = Services.telemetry.getHistogramById(
          "BROWSER_SET_DEFAULT_PDF_HANDLER_USER_CHOICE_RESULT"
        );
        histogram.add(telemetryResult);
      } catch (ex) {}
    }
  },

  // override nsIShellService.setDefaultBrowser() on the ShellService proxy.
  setDefaultBrowser(claimAllTypes, forAllUsers) {
    // On Windows 10, our best chance is to set UserChoice, so try that first.
    if (
      AppConstants.isPlatformAndVersionAtLeast("win", "10") &&
      lazy.NimbusFeatures.shellService.getVariable(
        "setDefaultBrowserUserChoice"
      )
    ) {
      // nsWindowsShellService::SetDefaultBrowser() kicks off several
      // operations, but doesn't wait for their result. So we don't need to
      // await the result of setAsDefaultUserChoice() here, either, we just need
      // to fall back in case it fails.
      this.setAsDefaultUserChoice().catch(err => {
        console.error(err);
        this.shellService.setDefaultBrowser(claimAllTypes, forAllUsers);
      });
      return;
    }

    this.shellService.setDefaultBrowser(claimAllTypes, forAllUsers);
  },

  setAsDefault() {
    let claimAllTypes = true;
    let setAsDefaultError = false;
    if (AppConstants.platform == "win") {
      try {
        // In Windows 8+, the UI for selecting default protocol is much
        // nicer than the UI for setting file type associations. So we
        // only show the protocol association screen on Windows 8+.
        // Windows 8 is version 6.2.
        let version = Services.sysinfo.getProperty("version");
        claimAllTypes = parseFloat(version) < 6.2;
      } catch (ex) {}
    }
    try {
      ShellService.setDefaultBrowser(claimAllTypes, false);
    } catch (ex) {
      setAsDefaultError = true;
      console.error(ex);
    }
    // Here BROWSER_IS_USER_DEFAULT and BROWSER_SET_USER_DEFAULT_ERROR appear
    // to be inverse of each other, but that is only because this function is
    // called when the browser is set as the default. During startup we record
    // the BROWSER_IS_USER_DEFAULT value without recording BROWSER_SET_USER_DEFAULT_ERROR.
    Services.telemetry
      .getHistogramById("BROWSER_IS_USER_DEFAULT")
      .add(!setAsDefaultError);
    Services.telemetry
      .getHistogramById("BROWSER_SET_DEFAULT_ERROR")
      .add(setAsDefaultError);
  },

  setAsDefaultPDFHandler(onlyIfKnownBrowser = false) {
    if (onlyIfKnownBrowser && !this.getDefaultPDFHandler().knownBrowser) {
      return;
    }

    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      this.setAsDefaultPDFHandlerUserChoice();
    }
  },

  /**
   * Determine if we're the default handler for the given file extension (like
   * ".pdf") or protocol (like "https").  Windows-only for now.
   *
   * @returns {boolean} true if we are the default handler, false otherwise.
   */
  isDefaultHandlerFor(aFileExtensionOrProtocol) {
    if (AppConstants.platform == "win") {
      return this.shellService
        .QueryInterface(Ci.nsIWindowsShellService)
        .isDefaultHandlerFor(aFileExtensionOrProtocol);
    }
    return false;
  },

  /**
   * Checks if Firefox app can and isn't pinned to OS "taskbar."
   *
   * @throws if not called from main process.
   */
  async doesAppNeedPin(privateBrowsing = false) {
    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      throw new Components.Exception(
        "Can't determine pinned from child process",
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }

    // Pretend pinning is not needed/supported if remotely disabled.
    if (lazy.NimbusFeatures.shellService.getVariable("disablePin")) {
      return false;
    }

    // Currently this only works on certain Windows versions.
    try {
      // First check if we can even pin the app where an exception means no.
      await this.shellService
        .QueryInterface(Ci.nsIWindowsShellService)
        .checkPinCurrentAppToTaskbarAsync(privateBrowsing);
      let winTaskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
        Ci.nsIWinTaskbar
      );

      // Then check if we're already pinned.
      return !(await this.shellService.isCurrentAppPinnedToTaskbarAsync(
        privateBrowsing
          ? winTaskbar.defaultPrivateGroupId
          : winTaskbar.defaultGroupId
      ));
    } catch (ex) {}

    // Next check mac pinning to dock.
    try {
      // Accessing this.macDockSupport will ensure we're actually running
      // on Mac (it's possible to be on Linux in this block).
      const isInDock = this.macDockSupport.isAppInDock;
      // We can't pin Private Browsing mode on Mac, only a shortcut to the vanilla app
      return privateBrowsing ? false : !isInDock;
    } catch (ex) {}
    return false;
  },

  /**
   * Pin Firefox app to the OS "taskbar."
   */
  async pinToTaskbar(privateBrowsing = false) {
    if (await this.doesAppNeedPin(privateBrowsing)) {
      try {
        if (AppConstants.platform == "win") {
          await this.shellService.pinCurrentAppToTaskbarAsync(privateBrowsing);
        } else if (AppConstants.platform == "macosx") {
          this.macDockSupport.ensureAppIsPinnedToDock();
        }
      } catch (ex) {
        console.error(ex);
      }
    }
  },

  async _handleWDBAResult(exeProcess, exeWaitTimeoutMs = 2000) {
    // Exit codes, see toolkit/mozapps/defaultagent/SetDefaultBrowser.h
    const S_OK = 0;
    const STILL_ACTIVE = 0x103;
    const MOZ_E_NO_PROGID = 0xa0000001;
    const MOZ_E_HASH_CHECK = 0xa0000002;
    const MOZ_E_REJECTED = 0xa0000003;
    const MOZ_E_BUILD = 0xa0000004;

    const exeWaitPromise = exeProcess.wait();
    const timeoutPromise = new Promise(function (resolve) {
      lazy.setTimeout(
        () => resolve({ exitCode: STILL_ACTIVE }),
        exeWaitTimeoutMs
      );
    });

    const { exitCode } = await Promise.race([exeWaitPromise, timeoutPromise]);

    if (exitCode != S_OK) {
      const telemetryResult =
        new Map([
          [STILL_ACTIVE, "ErrExeTimeout"],
          [MOZ_E_NO_PROGID, "ErrExeProgID"],
          [MOZ_E_HASH_CHECK, "ErrExeHash"],
          [MOZ_E_REJECTED, "ErrExeRejected"],
          [MOZ_E_BUILD, "ErrBuild"],
        ]).get(exitCode) ?? "ErrExeOther";

      throw new WDBAError(exitCode, telemetryResult);
    }
  },
};

XPCOMUtils.defineLazyServiceGetters(ShellServiceInternal, {
  shellService: ["@mozilla.org/browser/shell-service;1", "nsIShellService"],
  macDockSupport: ["@mozilla.org/widget/macdocksupport;1", "nsIMacDockSupport"],
});

/**
 * The external API exported by this module.
 */
export var ShellService = new Proxy(ShellServiceInternal, {
  get(target, name) {
    if (name in target) {
      return target[name];
    }
    if (target.shellService) {
      return target.shellService[name];
    }
    Services.console.logStringMessage(
      `${name} not found in ShellService: ${target.shellService}`
    );
    return undefined;
  },
});

class WDBAError extends Error {
  constructor(exitCode, telemetryResult) {
    super(`WDBA nonzero exit code ${exitCode}: ${telemetryResult}`);

    this.exitCode = exitCode;
    this.telemetryResult = telemetryResult;
  }
}
