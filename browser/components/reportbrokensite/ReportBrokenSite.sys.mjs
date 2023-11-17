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
  isReasonValid = false;

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
    this.isReasonValid = false;
  }

  enableOrDisableSendButton(
    isReasonEnabled,
    isReasonOptional,
    isDescriptionOptional
  ) {
    const isReasonOkay =
      !isReasonEnabled || isReasonOptional || this.isReasonValid;

    const isDescriptionOkay = isDescriptionOptional || this.isDescriptionValid;

    this.sendButton.disabled =
      !this.isURLValid || !isReasonOkay || !isDescriptionOkay;
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
  static SEND_MORE_INFO_PREF = "ui.new-webcompat-reporter.send-more-info-link";
  static NEW_REPORT_ENDPOINT_PREF =
    "ui.new-webcompat-reporter.new-report-endpoint";
  static REPORT_SITE_ISSUE_PREF = "extensions.webcompat-reporter.enabled";

  static MAIN_PANELVIEW_ID = "report-broken-site-popup-mainView";

  #_enabled = false;
  get enabled() {
    return this.#_enabled;
  }

  #reasonEnabled = false;
  #reasonIsOptional = true;
  #descriptionIsOptional = true;
  #sendMoreInfoEnabled = true;

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

    this.#sendMoreInfoEnabled = this.sendMoreInfoPref;
    this.#newReportEndpoint = this.newReportEndpointPref;
  }

  #initMainView(state) {
    state.sendButton.addEventListener("command", async ({ target }) => {
      const multiview = target.closest("panelmultiview");
      multiview.showSubView("report-broken-site-popup-reportSentView");
      state.reset();
    });

    state.cancelButton.addEventListener("command", ({ target, view }) => {
      const multiview = target.closest("panelmultiview");
      view.ownerGlobal.PanelMultiView.forNode(multiview).hidePopup();
      state.reset();
    });

    state.sendMoreInfoLink.addEventListener("click", async event => {
      event.preventDefault();
      const tabbrowser = event.view.ownerGlobal.gBrowser;
      const multiview = event.target.closest("panelmultiview");
      event.view.ownerGlobal.PanelMultiView.forNode(multiview).hidePopup();
      await this.#openWebCompatTab(tabbrowser);
      state.reset();
    });

    state.urlInput.addEventListener("input", ({ target }) => {
      const newUrlValid = target.value && target.checkValidity();
      if (state.isURLValid != newUrlValid) {
        state.isURLValid = newUrlValid;
        state.enableOrDisableSendButton(
          this.#reasonEnabled,
          this.#reasonIsOptional,
          this.#descriptionIsOptional
        );
      }
    });

    state.descriptionInput.addEventListener("input", ({ target }) => {
      const newDescValid = gDescriptionCheckRE.test(target.value);
      if (state.isDescriptionValid != newDescValid) {
        state.isDescriptionValid = newDescValid;
        state.enableOrDisableSendButton(
          this.#reasonEnabled,
          this.#reasonIsOptional,
          this.#descriptionIsOptional
        );
      }
    });

    const reasonDropdown = state.reasonInput;
    reasonDropdown.addEventListener("command", ({ target }) => {
      const choiceId = target.closest("menulist").selectedItem.id;
      const newValidity = choiceId !== ViewState.CHOOSE_A_REASON_OPT_ID;
      if (state.isReasonValid != newValidity) {
        state.isReasonValid = newValidity;
        state.enableOrDisableSendButton(
          this.#reasonEnabled,
          this.#reasonIsOptional,
          this.#descriptionIsOptional
        );
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
    state.okayButton.addEventListener("command", ({ target, view }) => {
      const multiview = target.closest("panelmultiview");
      view.ownerGlobal.PanelMultiView.forNode(multiview).hidePopup();
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

    state.sendMoreInfoLink.hidden = !this.#sendMoreInfoEnabled;

    state.reasonInput.hidden = !this.#reasonEnabled;

    state.reasonLabelRequired.hidden =
      !this.#reasonEnabled || this.#reasonIsOptional;
    state.reasonLabelOptional.hidden =
      !this.#reasonEnabled || !this.#reasonIsOptional;

    state.descriptionLabelRequired.hidden = this.#descriptionIsOptional;
    state.descriptionLabelOptional.hidden = !this.#descriptionIsOptional;

    state.enableOrDisableSendButton(
      this.#reasonEnabled,
      this.#reasonIsOptional,
      this.#descriptionIsOptional
    );

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
    );
  }

  open(event) {
    const { target } = event.sourceEvent;
    const { selectedBrowser } = event.view.ownerGlobal.gBrowser;
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
