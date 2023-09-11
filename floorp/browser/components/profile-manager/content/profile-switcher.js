/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

let FxAccounts = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
).getFxAccountsSingleton();

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

async function buildFxAccountsInfo() {
  let info = await FxAccounts.getSignedInUser();

  setEventListeners();

  if (!info) {
    let displayName = document.getElementById("fxa-display-name");
    displayName.setAttribute("data-l10n-id", "fxa-not-signed-in");
    document.getElementById("fxa-pairing-icon").hidden = true;
    return;
  }

  let avatar = document.getElementById("fxa-avatar");
  avatar.src = info.avatar;

  let displayName = document.getElementById("fxa-display-name");
  displayName.textContent = info.displayName;

  let email = document.getElementById("fxa-email");
  email.textContent = info.email;
}

function manageFxAccounts() {
  window.open("https://accounts.firefox.com/settings");
}

function openPasswordManager() {
  window.open("about:logins");
}

function openSyncSettings() {
  window.open("about:preferences#sync");
}

function setEventListeners() {
  let manageFxAccountsButton = document.getElementById("fxa-avatar");
  manageFxAccountsButton.addEventListener("click", manageFxAccounts);

  let openPasswordManagerButton = document.getElementById("fxa-password-icon");
  openPasswordManagerButton.addEventListener("click", openPasswordManager);

  let openSyncSettingsButton = document.getElementById("fxa-setting-icon");
  openSyncSettingsButton.addEventListener("click", openSyncSettings);

  let addAnotherDeviceButton = document.getElementById("fxa-pairing-icon");
  addAnotherDeviceButton.addEventListener("click", addAnotherDeviceToSync);
}

async function addAnotherDeviceToSync() {
  let info = await FxAccounts.getSignedInUser()
  let uid = info.uid;
  let email = info.email;
  window.open(`https://accounts.firefox.com/connect_another_device?context=fx_desktop_v3&entrypoint=preferences&service=sync&uid=${uid}&email=${email}`);
}
 
 async function flush() {
   try {
     ProfileService.flush();
     rebuildProfileList();
   } catch (e) {
     let [title, msg, button] = await document.l10n.formatValues([
       { id: "profiles-flush-fail-title" },
       {
         id:
           e.result == Cr.NS_ERROR_DATABASE_CHANGED
             ? "profiles-flush-conflict"
             : "profiles-flush-failed",
       },
       { id: "profiles-flush-restart-button" },
     ]);
 
     const PS = Ci.nsIPromptService;
     let result = Services.prompt.confirmEx(
       window,
       title,
       msg,
       PS.BUTTON_POS_0 * PS.BUTTON_TITLE_CANCEL +
         PS.BUTTON_POS_1 * PS.BUTTON_TITLE_IS_STRING,
       null,
       button,
       null,
       null,
       {}
     );
     if (result == 1) {
       restart(false);
     }
   }
 }
 
 function rebuildProfileList() {
   let parent = document.getElementById("profiles");
   while (parent.firstChild) {
     parent.firstChild.remove();
   }
 
   let defaultProfile;
   try {
     defaultProfile = ProfileService.defaultProfile;
   } catch (e) {}
 
   let currentProfile = ProfileService.currentProfile;
 
   for (let profile of ProfileService.profiles) {
     let isCurrentProfile = profile == currentProfile;
     let isInUse = isCurrentProfile;
     if (!isInUse) {
       try {
         let lock = profile.lock({});
         lock.unlock();
       } catch (e) {
         if (
           e.result != Cr.NS_ERROR_FILE_NOT_DIRECTORY &&
           e.result != Cr.NS_ERROR_FILE_NOT_FOUND
         ) {
           isInUse = true;
         }
       }
     }
     display({
       profile,
       isDefault: profile == defaultProfile,
       isCurrentProfile,
       isInUse,
     });
   }
 }
 
 function display(profileData) {
   let parent = document.getElementById("profiles");
 
   let div = document.createElement("div");
   div.className = "profile";
   div.id = profileData.profile.name;
   parent.appendChild(div);
 
   let name = document.createElement("label");
   name.className = "name";
 
   div.appendChild(name);
   name.textContent = profileData.profile.name;
 
   if (profileData.isCurrentProfile) {
     let currentProfile = document.createElement("description");
     currentProfile.className = "current";
      currentProfile.classList.add("tip-caption");
     document.l10n.setAttributes(currentProfile, "profiles-current-profile");
     div.appendChild(currentProfile);
   } else if (profileData.isInUse) {
     let currentProfile = document.createElement("description");
     currentProfile.className = "inuse";
     currentProfile.classList.add("tip-caption");
      document.l10n.setAttributes(currentProfile, "floorp-profiles-in-use");
     div.appendChild(currentProfile);
   }
 
   function createItem(title, value, dir = false) {
     let tr = document.createElement("tr");
 
     let th = document.createElement("th");
     th.setAttribute("class", "column");
     document.l10n.setAttributes(th, title);
     tr.appendChild(th);
 
     let td = document.createElement("td");
     tr.appendChild(td);
 
     if (dir) {
       td.appendChild(document.createTextNode(value.path));
 
       if (value.exists()) {
         let button = document.createElement("button");
         button.setAttribute("class", "opendir");
         document.l10n.setAttributes(button, "profiles-opendir");
 
         td.appendChild(button);
 
         button.addEventListener("click", function (e) {
           value.reveal();
         });
       }
     } else {
       document.l10n.setAttributes(td, value);
     }
   }
 
   createItem(
     "profiles-is-default",
     profileData.isDefault ? "profiles-yes" : "profiles-no"
   );
 
   createItem("profiles-rootdir", profileData.profile.rootDir, true);
 
   if (profileData.profile.localDir.path != profileData.profile.rootDir.path) {
     createItem("profiles-localdir", profileData.profile.localDir, true);
   }
 
   if (!profileData.isInUse) {
     let runButton = document.createElement("button");
     runButton.className = "run";
     document.l10n.setAttributes(runButton, "floorp-open-profile-with-new-instance");
     runButton.onclick = function () {
       openProfile(profileData.profile);
     };
     div.appendChild(runButton);
   }
 }
 
 // This is called from the createProfileWizard.xhtml dialog.
 function CreateProfile(profile) {
   // The wizard created a profile, just make it the default.
   defaultProfile(profile);
 }
 
 function createProfileWizard() {
   // This should be rewritten in HTML eventually.
   window.browsingContext.topChromeWindow.openDialog(
     "chrome://mozapps/content/profile/createProfileWizard.xhtml",
     "",
     "centerscreen,chrome,modal,titlebar",
     ProfileService,
     { CreateProfile }
   );
 }
 
 async function renameProfile(profile) {
   let newName = { value: profile.name };
   let [title, msg] = await document.l10n.formatValues([
     { id: "profiles-rename-profile-title" },
     { id: "profiles-rename-profile", args: { name: profile.name } },
   ]);
 
   if (Services.prompt.prompt(window, title, msg, newName, null, { value: 0 })) {
     newName = newName.value;
 
     if (newName == profile.name) {
       return;
     }
 
     try {
       profile.name = newName;
     } catch (e) {
       let [title, msg] = await document.l10n.formatValues([
         { id: "profiles-invalid-profile-name-title" },
         { id: "profiles-invalid-profile-name", args: { name: newName } },
       ]);
 
       Services.prompt.alert(window, title, msg);
       return;
     }
 
     flush();
   }
 }
 
 async function removeProfile(profile) {
   let deleteFiles = false;
 
   if (profile.rootDir.exists()) {
     let [title, msg, dontDeleteStr, deleteStr] =
       await document.l10n.formatValues([
         { id: "profiles-delete-profile-title" },
         {
           id: "profiles-delete-profile-confirm",
           args: { dir: profile.rootDir.path },
         },
         { id: "profiles-dont-delete-files" },
         { id: "profiles-delete-files" },
       ]);
     let buttonPressed = Services.prompt.confirmEx(
       window,
       title,
       msg,
       Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
         Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
         Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_2,
       dontDeleteStr,
       null,
       deleteStr,
       null,
       { value: 0 }
     );
     if (buttonPressed == 1) {
       return;
     }
 
     if (buttonPressed == 2) {
       deleteFiles = true;
     }
   }
 
   // If we are deleting the default profile we must choose a different one.
   let isDefault = false;
   try {
     isDefault = ProfileService.defaultProfile == profile;
   } catch (e) {}
 
   if (isDefault) {
     for (let p of ProfileService.profiles) {
       if (profile == p) {
         continue;
       }
 
       if (isDefault) {
         try {
           ProfileService.defaultProfile = p;
         } catch (e) {
           // This can happen on dev-edition if a non-default profile is in use.
           // In such a case the next time that dev-edition is started it will
           // find no default profile and just create a new one.
         }
       }
 
       break;
     }
   }
 
   try {
     profile.removeInBackground(deleteFiles);
   } catch (e) {
     let [title, msg] = await document.l10n.formatValues([
       { id: "profiles-delete-profile-failed-title" },
       { id: "profiles-delete-profile-failed-message" },
     ]);
 
     Services.prompt.alert(window, title, msg);
     return;
   }
 
   flush();
 }
 
 async function defaultProfile(profile) {
   try {
     ProfileService.defaultProfile = profile;
     flush();
   } catch (e) {
     // This can happen on dev-edition.
     let [title, msg] = await document.l10n.formatValues([
       { id: "profiles-cannot-set-as-default-title" },
       { id: "profiles-cannot-set-as-default-message" },
     ]);
 
     Services.prompt.alert(window, title, msg);
   }
 }
 
 function openProfile(profile) {
   Services.startup.createInstanceWithProfile(profile);
 }
 
 function restart(safeMode) {
   let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
     Ci.nsISupportsPRBool
   );
   Services.obs.notifyObservers(
     cancelQuit,
     "quit-application-requested",
     "restart"
   );
 
   if (cancelQuit.data) {
     return;
   }
 
   let flags = Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart;
 
   if (safeMode) {
     Services.startup.restartInSafeMode(flags);
   } else {
     Services.startup.quit(flags);
   }
 }
 
 window.addEventListener(
   "DOMContentLoaded",
   async function () {
     let createButton = document.getElementById("create-button");
     createButton.addEventListener("click", createProfileWizard);

     await buildFxAccountsInfo();
 
     if (ProfileService.isListOutdated) {
       document.getElementById("owned").hidden = true;
     } else {
       document.getElementById("conflict").hidden = true;
       rebuildProfileList();
     }
   },
   { once: true }
 );
 