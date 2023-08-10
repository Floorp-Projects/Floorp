/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BUNDLE_SRC =
  "resource://activity-stream/aboutwelcome/aboutwelcome.bundle.js";
const OPTIN_DEFAULT = {
  id: "FAKESPOT_OPTIN_DEFAULT",
  template: "multistage",
  backdrop: "transparent",
  transitions: true,
  screens: [
    {
      id: "FS_OPT_IN",
      content: {
        position: "split",
        title: { string_id: "shopping-onboarding-headline" },
        subtitle: `Not all reviews are created equal. To help you find real reviews, from real people, Firefox can use AI technology to analyze this product’s reviews.`,
        legal_paragraph: {
          text: {
            // fluent ids required to render copy
            string_id:
              "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use",
          },
          link_keys: ["privacy_policy", "terms_of_use"],
        },
        privacy_policy: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/privacy/firefox/",
              where: "tab",
            },
          },
        },
        terms_of_use: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/about/legal/terms/firefox/",
              where: "tab",
            },
          },
        },
        primary_button: {
          label: "Analyze Reviews",
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "browser.shopping.experience2023.optedIn",
                value: 1,
              },
            },
          },
        },
        secondary_button: {
          label: "Not Now",
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "browser.shopping.experience2023.active",
                value: false,
              },
            },
          },
        },
        info_text: {
          raw: "Review quality check is available when you shop on Amazon, Best Buy, and Walmart.",
        },
      },
    },
  ],
};

class Onboarding {
  constructor({ win } = {}) {
    this.win = win;
    this.doc = win.document;
    this.OnboardingSetup = false;

    win.addEventListener("Update", async event => {
      let { showOnboarding } = event.detail;
      // Prepare showing opt-in message by including respective
      // assets needed to render onboarding message
      if (showOnboarding) {
        this.showOptInMessage();
      }
    });
  }

  async _addScriptsAndRender() {
    const reactSrc = "resource://activity-stream/vendor/react.js";
    const domSrc = "resource://activity-stream/vendor/react-dom.js";
    // Add React script
    const getReactReady = async () => {
      return new Promise(resolve => {
        let reactScript = this.doc.createElement("script");
        reactScript.src = reactSrc;
        this.doc.head.appendChild(reactScript);
        reactScript.addEventListener("load", resolve);
      });
    };
    // Add ReactDom script
    const getDomReady = async () => {
      return new Promise(resolve => {
        let domScript = this.doc.createElement("script");
        domScript.src = domSrc;
        this.doc.head.appendChild(domScript);
        domScript.addEventListener("load", resolve);
      });
    };
    // Load React, then React Dom
    if (!this.doc.querySelector(`[src="${reactSrc}"]`)) {
      await getReactReady();
    }
    if (!this.doc.querySelector(`[src="${domSrc}"]`)) {
      await getDomReady();
    }
    // Load the bundle to render the content as configured.
    this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
    let bundleScript = this.doc.createElement("script");
    bundleScript.src = BUNDLE_SRC;
    this.doc.head.appendChild(bundleScript);

    const addStylesheet = href => {
      if (this.doc.querySelector(`link[href="${href}"]`)) {
        return;
      }
      const link = this.doc.head.appendChild(this.doc.createElement("link"));
      link.rel = "stylesheet";
      link.href = href;
    };

    addStylesheet(
      "chrome://activity-stream/content/aboutwelcome/aboutwelcome.css"
    );
  }

  // TBD: Move windows function setup to child actor. See Bug 1843461
  _setupWindowFunctions() {
    this.win.AWGetFeatureConfig = () => OPTIN_DEFAULT;
    this.win.AWFinish = () => {};
  }

  showOptInMessage() {
    if (this.OnboardingSetup) {
      return;
    }
    this._setupWindowFunctions();
    this._addScriptsAndRender();
    this.OnboardingSetup = true;
  }

  static getOnboarding() {
    if (!this.onboarding) {
      this.onboarding = new Onboarding({ win: window });
    }
    return this.onboarding;
  }
}

const OnboardingContainer = Onboarding.getOnboarding();
export default OnboardingContainer;
