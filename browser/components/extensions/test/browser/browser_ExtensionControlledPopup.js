/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global sinon */

"use strict";
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

registerCleanupFunction(() => {
  delete window.sinon;
});

ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionControlledPopup",
                               "resource:///modules/ExtensionControlledPopup.jsm");

function createMarkup(doc) {
  let panel = doc.getElementById("extension-notification-panel");
  let popupnotification = doc.createElement("popupnotification");
  let attributes = {
    id: "extension-controlled-notification",
    class: "extension-controlled-notification",
    popupid: "extension-controlled",
    hidden: "true",
    label: "ExtControlled",
    buttonlabel: "Keep Changes",
    buttonaccesskey: "K",
    secondarybuttonlabel: "Restore Settings",
    secondarybuttonaccesskey: "R",
    closebuttonhidden: "true",
    dropmarkerhidden: "true",
    checkboxhidden: "true",
  };
  Object.entries(attributes).forEach(([key, value]) => {
    popupnotification.setAttribute(key, value);
  });
  let content = doc.createElement("popupnotificationcontent");
  content.setAttribute("orient", "vertical");
  let description = doc.createElement("description");
  description.setAttribute("id", "extension-controlled-description");
  content.appendChild(description);
  popupnotification.appendChild(content);
  panel.appendChild(popupnotification);

  registerCleanupFunction(function removePopup() {
    popupnotification.remove();
  });

  return {panel, popupnotification};
}

/*
 * This function is a unit test for ExtensionControlledPopup. It is also tested
 * where it is being used (currently New Tab and homepage). An empty extension
 * is used along with the expected markup as an example.
 */
add_task(async function testExtensionControlledPopup() {
  let id = "ext-controlled@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id}},
      name: "Ext Controlled",
    },
    // We need to be able to find the extension using AddonManager.
    useAddonManager: "temporary",
  });

  await extension.startup();
  let addon = await AddonManager.getAddonByID(id);
  await ExtensionSettingsStore.initialize();

  let confirmedType = "extension-controlled-confirmed";
  let onObserverAdded = sinon.spy();
  let onObserverRemoved = sinon.spy();
  let observerTopic = "extension-controlled-event";
  let beforeDisableAddon = sinon.spy();
  let settingType = "extension-controlled";
  let settingKey = "some-key";
  let popup = new ExtensionControlledPopup({
    confirmedType,
    observerTopic,
    popupnotificationId: "extension-controlled-notification",
    settingType,
    settingKey,
    descriptionId: "extension-controlled-description",
    descriptionMessageId: "newTabControlled.message2",
    learnMoreMessageId: "newTabControlled.learnMore",
    learnMoreLink: "extension-controlled",
    onObserverAdded,
    onObserverRemoved,
    beforeDisableAddon,
  });

  let doc = Services.wm.getMostRecentWindow("navigator:browser").document;
  let {panel, popupnotification} = createMarkup(doc);

  function openPopupWithEvent() {
    let popupShown = promisePopupShown(panel);
    Services.obs.notifyObservers(null, observerTopic);
    return popupShown;
  }

  function closePopupWithAction(action, extensionId) {
    let done;
    if (action == "ignore") {
      panel.hidePopup();
    } else {
      if (action == "button") {
        done = TestUtils.waitForCondition(() => {
          return ExtensionSettingsStore.getSetting(confirmedType, id, id).value;
        });
      } else if (action == "secondarybutton") {
        done = awaitEvent("shutdown", id);
      }
      doc.getAnonymousElementByAttribute(
        popupnotification, "anonid", action).click();
    }
    return done;
  }

  // No callbacks are initially called.
  ok(!onObserverAdded.called, "No observer has been added");
  ok(!onObserverRemoved.called, "No observer has been removed");
  ok(!beforeDisableAddon.called, "Settings have not been restored");

  // Add the setting and observer.
  await ExtensionSettingsStore.addSetting(id, settingType, settingKey, "controlled", () => "init");
  await popup.addObserver(id);

  // Ensure the panel isn't open.
  ok(onObserverAdded.called, "Observing the event");
  onObserverAdded.reset();
  ok(!onObserverRemoved.called, "Observing the event");
  ok(!beforeDisableAddon.called, "Settings have not been restored");
  ok(panel.getAttribute("panelopen") != "true", "The panel is closed");
  is(popupnotification.hidden, true, "The popup is hidden");
  is(addon.userDisabled, false, "The extension is enabled");
  is(await popup.userHasConfirmed(id), false, "The user is not initially confirmed");

  // The popup should opened based on the observer event.
  await openPopupWithEvent();

  ok(!onObserverAdded.called, "Only one observer has been registered");
  ok(onObserverRemoved.called, "The observer was removed");
  onObserverRemoved.reset();
  ok(!beforeDisableAddon.called, "Settings have not been restored");
  is(panel.getAttribute("panelopen"), "true", "The panel is open");
  is(popupnotification.hidden, false, "The popup content is visible");
  is(await popup.userHasConfirmed(id), false, "The user has not confirmed yet");

  // Verify the description is populated.
  let description = doc.getElementById("extension-controlled-description");
  is(description.textContent,
     "An extension,  Ext Controlled, changed the page you see when you open a new tab.Learn more",
     "The extension name is in the description");
  let link = description.querySelector("label");
  is(link.href, "http://127.0.0.1:8888/support-dummy/extension-controlled",
     "The link has the href set from learnMoreLink");

  // Force close the popup, as if a user clicked away from it.
  await closePopupWithAction("ignore");

  // Nothing was recorded, but we won't show it again.
  ok(!onObserverAdded.called, "The observer hasn't changed");
  ok(!onObserverRemoved.called, "The observer hasn't changed");
  is(await popup.userHasConfirmed(id), false, "The user has not confirmed");
  is(addon.userDisabled, false, "The extension is still enabled");

  // Force add the observer again to keep changes.
  await popup.addObserver(id);
  ok(onObserverAdded.called, "The observer was added again");
  onObserverAdded.reset();
  ok(!onObserverRemoved.called, "The observer is still registered");
  is(await popup.userHasConfirmed(id), false, "The user has not confirmed");

  // Wait for popup.
  await openPopupWithEvent();

  // Keep the changes.
  await closePopupWithAction("button");

  // The observer is removed, but the notification is saved.
  ok(!onObserverAdded.called, "The observer wasn't added");
  ok(onObserverRemoved.called, "The observer was removed");
  onObserverRemoved.reset();
  is(await popup.userHasConfirmed(id), true, "The user has confirmed");
  is(addon.userDisabled, false, "The extension is still enabled");

  // Adding the observer again for this add-on won't work, since it is
  // confirmed.
  await popup.addObserver(id);
  ok(!onObserverAdded.called, "The observer isn't added");
  ok(!onObserverRemoved.called, "The observer isn't removed");
  is(await popup.userHasConfirmed(id), true, "The user has confirmed");

  // Clear that the user was notified.
  await popup.clearConfirmation(id);
  is(await popup.userHasConfirmed(id), false, "The user confirmation has been cleared");

  // Force add the observer again to restore changes.
  await popup.addObserver(id);
  ok(onObserverAdded.called, "The observer was added a third time");
  onObserverAdded.reset();
  ok(!onObserverRemoved.called, "The observer is still active");
  ok(!beforeDisableAddon.called, "We haven't disabled the add-on yet");
  is(await popup.userHasConfirmed(id), false, "The user has not confirmed");

  // Wait for popup.
  await openPopupWithEvent();

  // Restore the settings.
  await closePopupWithAction("secondarybutton");

  // The observer is removed and the add-on is now disabled.
  ok(!onObserverAdded.called, "There is no observer");
  ok(onObserverRemoved.called, "The observer has been removed");
  ok(beforeDisableAddon.called, "The beforeDisableAddon callback was fired");
  is(await popup.userHasConfirmed(id), false, "The user has not confirmed");
  is(addon.userDisabled, true, "The extension is now disabled");

  await extension.unload();
});
