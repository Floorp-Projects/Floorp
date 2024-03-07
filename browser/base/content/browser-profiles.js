/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

var gProfiles = {
  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "PROFILES_ENABLED",
      "browser.profiles.enabled",
      false,
      this.toggleProfileButtonsVisibility.bind(this)
    );

    if (!this.PROFILES_ENABLED) {
      return;
    }

    this.toggleProfileButtonsVisibility();
  },

  toggleProfileButtonsVisibility() {
    let profilesButton = PanelMultiView.getViewNode(
      document,
      "appMenu-profiles-button"
    );

    profilesButton.hidden = !this.PROFILES_ENABLED;

    if (this.PROFILES_ENABLED) {
      document.l10n.setArgs(profilesButton, {
        profilename: ProfileService.currentProfile?.name ?? "",
      });
    }
  },

  updateView(panel) {
    this.populateSubView();
    PanelUI.showSubView("PanelUI-profiles", panel);
  },

  async populateSubView() {
    let closeProfileButton = PanelMultiView.getViewNode(
      document,
      "profiles-close-profile-button"
    );
    document.l10n.setArgs(closeProfileButton, {
      profilename: ProfileService.currentProfile?.name ?? "",
    });

    let profileIconEl = PanelMultiView.getViewNode(
      document,
      "profile-icon-image"
    );
    profileIconEl.style.listStyleImage = `url(${
      ProfileService.currentProfile?.iconURL ??
      "chrome://branding/content/icon64.png"
    })`;

    let profileNameEl = PanelMultiView.getViewNode(document, "profile-name");
    profileNameEl.textContent = ProfileService.currentProfile?.name ?? "";

    let profilesList = PanelMultiView.getViewNode(
      document,
      "PanelUI-profiles"
    ).querySelector("#profiles-list");
    while (profilesList.lastElementChild) {
      profilesList.lastElementChild.remove();
    }

    for (let profile of ProfileService.profiles) {
      if (profile === ProfileService.currentProfile) {
        continue;
      }

      let button = document.createXULElement("toolbarbutton");
      button.setAttribute("label", profile.name);
      button.className = "subviewbutton subviewbutton-iconic";
      button.style.listStyleImage = `url(${
        profile.iconURL ?? "chrome://branding/content/icon16.png"
      })`;
      button.onclick = () => {
        Services.startup.createInstanceWithProfile(profile);
      };

      profilesList.appendChild(button);
    }
  },
};
