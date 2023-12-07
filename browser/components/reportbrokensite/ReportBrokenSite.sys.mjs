/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

const DEFAULT_NEW_REPORT_ENDPOINT = "https://webcompat.com/issues/new";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const gDescriptionCheckRE = /\S/;

class ViewState {
  #doc;
  #mainView;
  #reportSentView;

  currentTabURI;
  currentTabWebcompatDetailsPromise;
  isURLValid = false;
  isDescriptionValid = false;
  #isReasonValid = false;

  constructor(doc) {
    this.#doc = doc;
    this.#mainView = doc.ownerGlobal.PanelMultiView.getViewNode(
      this.#doc,
      "report-broken-site-popup-mainView"
    );
    this.#reportSentView = doc.ownerGlobal.PanelMultiView.getViewNode(
      this.#doc,
      "report-broken-site-popup-reportSentView"
    );
    ViewState.#cache.set(doc, this);
  }

  static #cache = new WeakMap();
  static get(doc) {
    return ViewState.#cache.get(doc) ?? new ViewState(doc);
  }

  get urlInput() {
    return this.#mainView.querySelector("#report-broken-site-popup-url");
  }

  get url() {
    return this.urlInput.value;
  }

  set url(spec) {
    this.urlInput.value = spec;
  }

  resetURLToCurrentTab() {
    const { currentURI } = this.#doc.ownerGlobal.gBrowser.selectedBrowser;
    this.currentTabURI = currentURI;
    this.urlInput.value = currentURI.spec;
    this.isURLValid = true;
  }

  get descriptionInput() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-description"
    );
  }

  get description() {
    return this.descriptionInput.value;
  }

  set description(value) {
    this.descriptionInput.value = value;
  }

  static REASON_CHOICES_ID_PREFIX = "report-broken-site-popup-reason-";

  get reasonInput() {
    return this.#mainView.querySelector("#report-broken-site-popup-reason");
  }

  get reasonInputValidationHelper() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-missing-reason-validation-helper"
    );
  }

  get reason() {
    const reason = this.reasonInput.selectedItem.id.replace(
      ViewState.REASON_CHOICES_ID_PREFIX,
      ""
    );
    return reason == "choose" ? undefined : reason;
  }

  set reason(value) {
    this.reasonInput.selectedItem = this.#mainView.querySelector(
      `#${ViewState.REASON_CHOICES_ID_PREFIX}${value}`
    );
  }

  static CHOOSE_A_REASON_OPT_ID = "report-broken-site-popup-reason-choose";

  get chooseAReasonOption() {
    return this.#mainView.querySelector(`#${ViewState.CHOOSE_A_REASON_OPT_ID}`);
  }

  reset() {
    this.currentTabWebcompatDetailsPromise = undefined;

    this.resetURLToCurrentTab();

    this.description = "";
    this.isDescriptionValid = false;

    this.reason = "choose";
    this.#isReasonValid = false;
    this.toggleReasonValidationMessage(false);
  }

  get isReasonValid() {
    return this.#isReasonValid;
  }

  set isReasonValid(isValid) {
    this.#isReasonValid = isValid;
    this.toggleReasonValidationMessage(!isValid);
  }

  toggleReasonValidationMessage(show) {
    const validation = this.reasonInputValidationHelper;
    validation.setCustomValidity(show ? "required" : "");
    validation.reportValidity();
  }

  get isReasonOkay() {
    const { reasonEnabled, reasonIsOptional } =
      this.#doc.ownerGlobal.ReportBrokenSite;
    return !reasonEnabled || reasonIsOptional || this.isReasonValid;
  }

  get isDescriptionOkay() {
    const { descriptionIsOptional } = this.#doc.ownerGlobal.ReportBrokenSite;
    return descriptionIsOptional || this.isDescriptionValid;
  }

  checkAndShowInputValidity() {
    // This function focuses on the first invalid input (if any), updates the validity of
    // the helper input for the reason drop-down (so CSS :invalid state is updated),
    // and returns true if the form has an invalid input (false otherwise).
    const { isURLValid, isReasonOkay, isDescriptionOkay } = this;
    const validation = this.reasonInputValidationHelper;
    validation.setCustomValidity(isReasonOkay ? "" : "missing");
    validation.reportValidity();
    if (!isURLValid) {
      this.urlInput.focus();
    } else if (!isReasonOkay) {
      this.reasonInput.openMenu(true);
    } else if (!isDescriptionOkay) {
      this.descriptionInput.focus();
    }
    return !(isURLValid && isReasonOkay && isDescriptionOkay);
  }

  get sendMoreInfoLink() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-send-more-info-link"
    );
  }

  get reasonLabelRequired() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-reason-label"
    );
  }

  get reasonLabelOptional() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-reason-optional-label"
    );
  }

  get descriptionLabelRequired() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-description-label"
    );
  }

  get descriptionLabelOptional() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-description-optional-label"
    );
  }

  get sendButton() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-send-button"
    );
  }

  get cancelButton() {
    return this.#mainView.querySelector(
      "#report-broken-site-popup-cancel-button"
    );
  }

  get mainView() {
    return this.#mainView;
  }

  get reportSentView() {
    return this.#reportSentView;
  }

  get okayButton() {
    return this.#reportSentView.querySelector(
      "#report-broken-site-popup-okay-button"
    );
  }
}

export var ReportBrokenSite = new (class ReportBrokenSite {
  #newReportEndpoint = undefined;

  get sendMoreInfoEndpoint() {
    return this.#newReportEndpoint || DEFAULT_NEW_REPORT_ENDPOINT;
  }

  static WEBCOMPAT_REPORTER_CONFIG = {
    src: "desktop-reporter",
    utm_campaign: "report-broken-site",
    utm_source: "desktop-reporter",
  };

  static DATAREPORTING_PREF = "datareporting.healthreport.uploadEnabled";
  static REPORTER_ENABLED_PREF = "ui.new-webcompat-reporter.enabled";

  static REASON_PREF = "ui.new-webcompat-reporter.reason-dropdown";
  static REASON_PREF_VALUES = {
    0: "disabled",
    1: "optional",
    2: "required",
  };
  static SEND_MORE_INFO_PREF = "ui.new-webcompat-reporter.send-more-info-link";
  static NEW_REPORT_ENDPOINT_PREF =
    "ui.new-webcompat-reporter.new-report-endpoint";
  static REPORT_SITE_ISSUE_PREF = "extensions.webcompat-reporter.enabled";

  static MAIN_PANELVIEW_ID = "report-broken-site-popup-mainView";
  static SENT_PANELVIEW_ID = "report-broken-site-popup-reportSentView";

  #_enabled = false;
  get enabled() {
    return this.#_enabled;
  }

  #reasonEnabled = false;
  #reasonIsOptional = true;
  #descriptionIsOptional = true;
  #sendMoreInfoEnabled = true;

  get reasonEnabled() {
    return this.#reasonEnabled;
  }

  get reasonIsOptional() {
    return this.#reasonIsOptional;
  }

  get descriptionIsOptional() {
    return this.#descriptionIsOptional;
  }

  constructor() {
    for (const [name, [pref, dflt]] of Object.entries({
      dataReportingPref: [ReportBrokenSite.DATAREPORTING_PREF, false],
      reasonPref: [ReportBrokenSite.REASON_PREF, 0],
      sendMoreInfoPref: [ReportBrokenSite.SEND_MORE_INFO_PREF, false],
      newReportEndpointPref: [
        ReportBrokenSite.NEW_REPORT_ENDPOINT_PREF,
        DEFAULT_NEW_REPORT_ENDPOINT,
      ],
      enabledPref: [ReportBrokenSite.REPORTER_ENABLED_PREF, true],
      reportSiteIssueEnabledPref: [
        ReportBrokenSite.REPORT_SITE_ISSUE_PREF,
        false,
      ],
    })) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        name,
        pref,
        dflt,
        this.#checkPrefs.bind(this)
      );
    }
    this.#checkPrefs();
  }

  canReportURI(uri) {
    return uri && (uri.schemeIs("http") || uri.schemeIs("https"));
  }

  #recordGleanEvent(name, extra) {
    Glean.webcompatreporting[name].record(extra);
  }

  updateParentMenu(event) {
    // We need to make sure that the Report Broken Site menu item
    // is disabled and/or hidden depending on the prefs/active tab URL
    // when our parent popups are shown, and if their tab's location
    // changes while they are open.
    const tabbrowser = event.target.ownerGlobal.gBrowser;
    this.enableOrDisableMenuitems(tabbrowser.selectedBrowser);

    tabbrowser.addTabsProgressListener(this);
    event.target.addEventListener(
      "popuphidden",
      () => {
        tabbrowser.removeTabsProgressListener(this);
      },
      { once: true }
    );
  }

  init(tabbrowser) {
    // Called in browser.js.
    const { ownerGlobal } = tabbrowser.selectedBrowser;
    const { document } = ownerGlobal;

    const state = ViewState.get(document);

    this.#initMainView(state);
    this.#initReportSentView(state);

    for (const id of ["menu_HelpPopup", "appMenu-popup"]) {
      document
        .getElementById(id)
        .addEventListener("popupshown", this.updateParentMenu.bind(this));
    }

    ownerGlobal.PanelMultiView.getViewNode(
      document,
      ReportBrokenSite.MAIN_PANELVIEW_ID
    ).addEventListener("ViewShowing", ({ target }) => {
      const { selectedBrowser } = target.ownerGlobal.gBrowser;
      let source = "helpMenu";
      switch (target.closest("panelmultiview")?.id) {
        case "appMenu-multiView":
          source = "hamburgerMenu";
          break;
        case "protections-popup-multiView":
          source = "ETPShieldIconMenu";
          break;
      }
      this.#onMainViewShown(source, selectedBrowser);
    });
  }

  enableOrDisableMenuitems(selectedbrowser) {
    // Ensures that the various Report Broken Site menu items and
    // toolbar buttons are enabled/hidden when appropriate (and
    // also the Help menu's Report Site Issue item)/

    const canReportUrl = this.canReportURI(selectedbrowser.currentURI);

    const { document } = selectedbrowser.ownerGlobal;

    const cmd = document.getElementById("cmd_reportBrokenSite");
    if (this.enabled) {
      cmd.setAttribute("hidden", "false"); // see bug 805653
    } else {
      cmd.setAttribute("hidden", "true");
    }
    if (canReportUrl) {
      cmd.removeAttribute("disabled");
    } else {
      cmd.setAttribute("disabled", "true");
    }

    // Changes to the "hidden" and "disabled" state of the command aren't reliably
    // reflected on the main menu unless we open it twice, or do it manually.
    // (See bug 1864953).
    const mainmenuItem = document.getElementById("help_reportBrokenSite");
    if (mainmenuItem) {
      mainmenuItem.hidden = !this.enabled;
      mainmenuItem.disabled = !canReportUrl;
    }

    // Report Site Issue is our older issue reporter, shown in the Help
    // menu on pre-release channels. We should hide it unless we're
    // disabled, at which point we should show it when available.
    const reportSiteIssue = document.getElementById("help_reportSiteIssue");
    if (reportSiteIssue) {
      reportSiteIssue.hidden = this.enabled || !this.reportSiteIssueEnabledPref;
      reportSiteIssue.disabled = !canReportUrl;
    }
  }

  #checkPrefs(whichChanged) {
    // No breakage reports can be sent by Glean if it's disabled, so we also
    // disable the broken site reporter. We also have our own pref.
    this.#_enabled =
      Services.policies.isAllowed("feedbackCommands") &&
      this.dataReportingPref &&
      this.enabledPref;

    this.#reasonEnabled = this.reasonPref == 1 || this.reasonPref == 2;
    this.#reasonIsOptional = this.reasonPref == 1;
    if (!whichChanged || whichChanged == ReportBrokenSite.REASON_PREF) {
      const setting = ReportBrokenSite.REASON_PREF_VALUES[this.reasonPref];
      this.#recordGleanEvent("reasonDropdown", { setting });
    }

    this.#sendMoreInfoEnabled = this.sendMoreInfoPref;
    this.#newReportEndpoint = this.newReportEndpointPref;
  }

  #initMainView(state) {
    state.sendButton.addEventListener("command", async ({ target }) => {
      if (state.checkAndShowInputValidity()) {
        return;
      }
      const multiview = target.closest("panelmultiview");
      this.#recordGleanEvent("send");
      await this.#sendReportAsGleanPing(state);
      state.reportSentView.hidden = false;
      multiview.showSubView("report-broken-site-popup-reportSentView");
      state.reset();
    });

    state.cancelButton.addEventListener("command", ({ target }) => {
      target.ownerGlobal.CustomizableUI.hidePanelForNode(target);
      state.reset();
    });

    state.sendMoreInfoLink.addEventListener("click", async event => {
      event.preventDefault();
      const tabbrowser = event.target.ownerGlobal.gBrowser;
      this.#recordGleanEvent("sendMoreInfo");
      event.target.ownerGlobal.CustomizableUI.hidePanelForNode(event.target);
      await this.#openWebCompatTab(tabbrowser);
      state.reset();
    });

    state.urlInput.addEventListener("input", ({ target }) => {
      const newUrlValid = target.value && target.checkValidity();
      if (state.isURLValid != newUrlValid) {
        state.isURLValid = newUrlValid;
      }
    });

    state.descriptionInput.addEventListener("input", ({ target }) => {
      const newDescValid = gDescriptionCheckRE.test(target.value);
      if (state.isDescriptionValid != newDescValid) {
        state.isDescriptionValid = newDescValid;
      }
    });

    const reasonDropdown = state.reasonInput;
    reasonDropdown.addEventListener("command", ({ target }) => {
      const choiceId = target.closest("menulist").selectedItem.id;
      const newValidity = choiceId !== ViewState.CHOOSE_A_REASON_OPT_ID;
      if (state.isReasonValid != newValidity) {
        state.isReasonValid = newValidity;
      }
    });

    const menupopup = reasonDropdown.querySelector("menupopup");
    const onDropDownShowOrHide = ({ type }) => {
      // Hide "choose a reason" while the user has the reason dropdown open
      const shouldHide = type == "popupshowing";
      state.chooseAReasonOption.hidden = shouldHide;
    };
    menupopup.addEventListener("popupshowing", onDropDownShowOrHide);
    menupopup.addEventListener("popuphiding", onDropDownShowOrHide);
  }

  #initReportSentView(state) {
    state.okayButton.addEventListener("command", ({ target }) => {
      target.ownerGlobal.CustomizableUI.hidePanelForNode(target);
    });
  }

  async #onMainViewShown(source, selectedBrowser) {
    const { document } = selectedBrowser.ownerGlobal;

    let didReset = false;
    const state = ViewState.get(document);
    const uri = selectedBrowser.currentURI;
    if (!state.isURLValid && !state.isDescriptionValid) {
      state.reset();
      didReset = true;
    } else if (!state.currentTabURI || !uri.equals(state.currentTabURI)) {
      state.reset();
      didReset = true;
    } else if (!state.url) {
      state.resetURLToCurrentTab();
    }

    state.mainView.hidden = false;

    const { sendMoreInfoLink } = state;
    const { sendMoreInfoEndpoint } = this;
    if (sendMoreInfoLink.href !== sendMoreInfoEndpoint) {
      sendMoreInfoLink.href = sendMoreInfoEndpoint;
    }
    sendMoreInfoLink.hidden = !this.#sendMoreInfoEnabled;

    state.reasonInput.hidden = !this.#reasonEnabled;

    state.reasonLabelRequired.hidden =
      !this.#reasonEnabled || this.#reasonIsOptional;
    state.reasonLabelOptional.hidden =
      !this.#reasonEnabled || !this.#reasonIsOptional;

    state.descriptionLabelRequired.hidden = this.#descriptionIsOptional;
    state.descriptionLabelOptional.hidden = !this.#descriptionIsOptional;

    this.#recordGleanEvent("opened", { source });

    if (didReset || !state.currentTabWebcompatDetailsPromise) {
      state.currentTabWebcompatDetailsPromise = this.#queryActor(
        "GetWebCompatInfo",
        undefined,
        selectedBrowser
      ).catch(err => {
        console.error("Report Broken Site: unexpected error", err);
      });
    }
  }

  async #queryActor(msg, params, browser) {
    const actor =
      browser.browsingContext.currentWindowGlobal.getActor("ReportBrokenSite");
    return actor.sendQuery(msg, params);
  }

  async #loadTab(tabbrowser, url, triggeringPrincipal) {
    const tab = tabbrowser.addTab(url, {
      inBackground: false,
      triggeringPrincipal,
    });
    const expectedBrowser = tabbrowser.getBrowserForTab(tab);
    return new Promise(resolve => {
      const listener = {
        onLocationChange(browser, webProgress, request, uri, flags) {
          if (
            browser == expectedBrowser &&
            uri.spec == url &&
            webProgress.isTopLevel
          ) {
            resolve(tab);
            tabbrowser.removeTabsProgressListener(listener);
          }
        },
      };
      tabbrowser.addTabsProgressListener(listener);
    });
  }

  async #openWebCompatTab(tabbrowser) {
    const endpointUrl = this.sendMoreInfoEndpoint;
    const principal = Services.scriptSecurityManager.createNullPrincipal({});
    const tab = await this.#loadTab(tabbrowser, endpointUrl, principal);
    const { document } = tabbrowser.selectedBrowser.ownerGlobal;
    const { description, reason, url, currentTabWebcompatDetailsPromise } =
      ViewState.get(document);

    return this.#queryActor(
      "SendDataToWebcompatCom",
      {
        reason,
        description,
        endpointUrl,
        reportUrl: url,
        reporterConfig: ReportBrokenSite.WEBCOMPAT_REPORTER_CONFIG,
        webcompatInfo: await currentTabWebcompatDetailsPromise,
      },
      tab.linkedBrowser
    ).catch(err => {
      console.error("Report Broken Site: unexpected error", err);
    });
  }

  async #sendReportAsGleanPing({
    currentTabWebcompatDetailsPromise,
    description,
    reason,
    url,
  }) {
    const gBase = Glean.brokenSiteReport;
    const gTabInfo = Glean.brokenSiteReportTabInfo;
    const gAntitracking = Glean.brokenSiteReportTabInfoAntitracking;
    const gFrameworks = Glean.brokenSiteReportTabInfoFrameworks;
    const gApp = Glean.brokenSiteReportBrowserInfoApp;
    const gGraphics = Glean.brokenSiteReportBrowserInfoGraphics;
    const gPrefs = Glean.brokenSiteReportBrowserInfoPrefs;
    const gSystem = Glean.brokenSiteReportBrowserInfoSystem;

    if (reason) {
      gBase.breakageCategory.set(reason);
    }

    gBase.description.set(description);
    gBase.url.set(url);

    const details = await currentTabWebcompatDetailsPromise;

    if (!details) {
      GleanPings.brokenSiteReport.submit();
      return;
    }

    const {
      antitracking,
      browser,
      devicePixelRatio,
      frameworks,
      languages,
      userAgent,
    } = details;

    gTabInfo.languages.set(languages);
    gTabInfo.useragentString.set(userAgent);
    gGraphics.devicePixelRatio.set(devicePixelRatio);

    for (const [name, value] of Object.entries(antitracking)) {
      gAntitracking[name].set(value);
    }

    for (const [name, value] of Object.entries(frameworks)) {
      gFrameworks[name].set(value);
    }

    const { app, graphics, locales, platform, prefs, security } = browser;

    gApp.defaultLocales.set(locales);
    gApp.defaultUseragentString.set(app.defaultUserAgent);

    const { fissionEnabled, isTablet, memoryMB } = platform;
    gApp.fissionEnabled.set(fissionEnabled);
    gSystem.isTablet.set(isTablet ?? false);
    gSystem.memory.set(memoryMB);

    gPrefs.cookieBehavior.set(prefs["network.cookie.cookieBehavior"]);
    gPrefs.forcedAcceleratedLayers.set(
      prefs["layers.acceleration.force-enabled"]
    );
    gPrefs.globalPrivacyControlEnabled.set(
      prefs["privacy.globalprivacycontrol.enabled"]
    );
    gPrefs.installtriggerEnabled.set(
      prefs["extensions.InstallTrigger.enabled"]
    );
    gPrefs.opaqueResponseBlocking.set(prefs["browser.opaqueResponseBlocking"]);
    gPrefs.resistFingerprintingEnabled.set(
      prefs["privacy.resistFingerprinting"]
    );
    gPrefs.softwareWebrender.set(prefs["gfx.webrender.software"]);

    if (security) {
      for (const [name, value] of Object.entries(security)) {
        if (value?.length) {
          Glean.brokenSiteReportBrowserInfoSecurity[name].set(value);
        }
      }
    }

    const { devices, drivers, features, hasTouchScreen, monitors } = graphics;

    gGraphics.devicesJson.set(JSON.stringify(devices));
    gGraphics.driversJson.set(JSON.stringify(drivers));
    gGraphics.featuresJson.set(JSON.stringify(features));
    gGraphics.hasTouchScreen.set(hasTouchScreen);
    gGraphics.monitorsJson.set(JSON.stringify(monitors));

    GleanPings.brokenSiteReport.submit();
  }

  open(event) {
    const { target } = event.sourceEvent;
    const { selectedBrowser } = target.ownerGlobal.gBrowser;
    const { ownerGlobal } = selectedBrowser;
    const { document } = ownerGlobal;

    switch (target.id) {
      case "appMenu-report-broken-site-button":
        ownerGlobal.PanelUI.showSubView(
          ReportBrokenSite.MAIN_PANELVIEW_ID,
          target
        );
        break;
      case "protections-popup-report-broken-site-button":
        document
          .getElementById("protections-popup-multiView")
          .showSubView(ReportBrokenSite.MAIN_PANELVIEW_ID);
        break;
      case "help_reportBrokenSite":
        // hide the hamburger menu first, as we overlap with it.
        const appMenuPopup = document.getElementById("appMenu-popup");
        appMenuPopup?.hidePopup();

        // See bug 1864957; we should be able to use showSubView here
        ownerGlobal.PanelMultiView.openPopup(
          document.getElementById("report-broken-main-menu-panel"),
          document.getElementById("PanelUI-menu-button"),
          { position: "bottomright topright" }
        );
        break;
    }
  }
})();
