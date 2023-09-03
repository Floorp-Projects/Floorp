/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import Firefox Modules
const { MigrationUtils } = ChromeUtils.importESModule(
    "resource:///modules/MigrationUtils.sys.mjs"
);
const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
);

const designOptions = document.getElementsByClassName("third-design-card-content");
const setupPanels = document.getElementsByClassName("welcome-setups");
const nextSetup = document.getElementsByClassName("go-next-page-button");

const welcomeFunctions = {
    pageFunctions: {
        init () {
            // init setup panels
            for (let i = 0; i < setupPanels.length; i++) {
                setupPanels[i].setAttribute("hidden", "true");
            }
            document.getElementById("first-welcome-box").removeAttribute("hidden");

            // add event listeners for Next setup button
            for (let i = 0; i < nextSetup.length; i++) {
                nextSetup[i].setAttribute("onclick", "welcomeFunctions.pageFunctions.clickNextButton()");
            }
            
            //import Data from another browsers
            const importButton = document.getElementById("import-data-floorp-welcome-button");
            importButton.setAttribute("onclick", "welcomeFunctions.manageBrowserFunctions.openMigrationWizard()");
            
            // setting
            const defaultSettingButton = document.getElementById("apply-default-setting");
            const minimumSettingButton = document.getElementById("apply-minimum-setting");
            const maximumSettingButton = document.getElementById("apply-maximum-setting");
            defaultSettingButton.setAttribute("onclick", "welcomeFunctions.manageBrowserFunctions.applyDefautlSettings()");
            minimumSettingButton.setAttribute("onclick", "welcomeFunctions.manageBrowserFunctions.applyMinimumSettings()");
            maximumSettingButton.setAttribute("onclick", "welcomeFunctions.manageBrowserFunctions.applyMaximumSettings()");

            // add event listeners for Design selection
            for (let i = 0; i < designOptions.length; i++) {
                designOptions[i].setAttribute("onclick", `welcomeFunctions.elementManageFunction.clickToSelectDesign(this, ${i})`);
            }

            // exsist setup
            const existSetup = document.getElementById("exsit-setup-button");
            existSetup.addEventListener("click", () => {
                window.location.href = "about:home";
            });
        },

        clickNextButton () {
            let currentStep = document.querySelector(".welcome-setups:not([hidden])");
            let nextStep = currentStep.nextElementSibling;
            currentStep.setAttribute("hidden", "true");
            nextStep.removeAttribute("hidden");
        },
    },

    elementManageFunction: {
        clickToSelectDesign (element, number) {
            if (element.getAttribute("selected") == "true") {
                return;
            }

            for (let i = 0; i < designOptions.length; i++) {
                designOptions[i].removeAttribute("selected");
            }
            element.setAttribute("selected", "true");
            // set design to browser 0: Proton, 1: Lepton, 2: Lepton Multitab, 3: Fluent, 4: Gnome, 5: Fluerial, 6: Fluerial Multitab
            // Welcome page design options: 0: Lepton, 1: Photon, 2: Proton Fix, 3: Fluerial, 4: Proton
            switch(number) {
                case 0:
                    // Lepton
                    Services.prefs.setIntPref("floorp.browser.user.interface", 3);
                    Services.obs.notifyObservers({}, "set-lepton-ui");
                    break;
                case 1:
                    // Photon
                    Services.prefs.setIntPref("floorp.browser.user.interface", 3);
                    Services.obs.notifyObservers({}, "set-photon-ui");
                    break;
                case 2:
                    // Proton Fix
                    Services.prefs.setIntPref("floorp.browser.user.interface", 3);
                    Services.obs.notifyObservers({}, "set-protonfix-ui");
                    break;
                case 3:
                    // Fluerial
                    Services.prefs.setIntPref("floorp.browser.user.interface", 8);
                    break;
                case 4:
                    // Proton
                    Services.prefs.setIntPref("floorp.browser.user.interface", 0);
                    break;
            }
        },
    },

    manageBrowserFunctions: {
        openMigrationWizard () {
            Promise.all([
                MigrationUtils.showMigrationWizard(null, {})
            ]).then(() => {
                welcomeFunctions.pageFunctions.clickNextButton();
            });
        },

        applyDefautlSettings () {
            Services.prefs.setBoolPref("browser.display.statusbar", false);
            Services.prefs.setBoolPref("floorp.browser.sidebar.enable" , true);
            Services.prefs.setBoolPref("floorp.tabsleep.enabled", false);
            Services.prefs.setIntPref("floorp.download.notification", 4);
        },

        applyMinimumSettings () {
            Services.prefs.setBoolPref("browser.display.statusbar", false);
            Services.prefs.setBoolPref("floorp.browser.sidebar.enable" , false);
            Services.prefs.setBoolPref("floorp.tabsleep.enabled", false);
            Services.prefs.setIntPref("floorp.download.notification", 4);
        },

        applyMaximumSettings () {
            Services.prefs.setBoolPref("browser.display.statusbar", true);
            Services.prefs.setBoolPref("floorp.browser.sidebar.enable" , true);
            Services.prefs.setBoolPref("floorp.tabsleep.enabled", true);
            Services.prefs.setIntPref("floorp.download.notification", 3);
        }
    }
};

window.addEventListener("load", () => {
    welcomeFunctions.pageFunctions.init();
});
