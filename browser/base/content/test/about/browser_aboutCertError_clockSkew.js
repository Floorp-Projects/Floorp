/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS =
  "services.settings.clock_skew_seconds";
const PREF_SERVICES_SETTINGS_LAST_FETCHED =
  "services.settings.last_update_seconds";

add_task(async function checkWrongSystemTimeWarning() {
  async function setUpPage() {
    let browser;
    let certErrorLoaded;
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      () => {
        gBrowser.selectedTab = BrowserTestUtils.addTab(
          gBrowser,
          "https://expired.example.com/"
        );
        browser = gBrowser.selectedBrowser;
        certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
      },
      false
    );

    info("Loading and waiting for the cert error");
    await certErrorLoaded;

    return SpecialPowers.spawn(browser, [], async function() {
      let doc = content.document;
      let div = doc.getElementById("errorShortDescText");
      let systemDateDiv = doc.getElementById("wrongSystemTime_systemDate1");
      let learnMoreLink = doc.getElementById("learnMoreLink");

      await ContentTaskUtils.waitForCondition(
        () => div.textContent.includes("update your computer clock"),
        "Correct error message found"
      );

      return {
        divDisplay: content.getComputedStyle(div).display,
        text: div.textContent,
        systemDate: systemDateDiv.textContent,
        learnMoreLink: learnMoreLink.href,
      };
    });
  }

  // Pretend that we recently updated our kinto clock skew pref
  Services.prefs.setIntPref(
    PREF_SERVICES_SETTINGS_LAST_FETCHED,
    Math.floor(Date.now() / 1000)
  );

  let formatter = new Intl.DateTimeFormat("default");

  // For this test, we want to trick Firefox into believing that
  // the local system time (as returned by Date.now()) is wrong.
  // Because we don't want to actually change the local system time,
  // we will do the following:

  // Take the validity date of our test page (expired.example.com).
  let expiredDate = new Date("2010/01/05 12:00");
  let localDate = Date.now();

  // Compute the difference between the server date and the correct
  // local system date.
  let skew = Math.floor((localDate - expiredDate) / 1000);

  // Make it seem like our reference server agrees that the certificate
  // date is correct by recording the difference as clock skew.
  Services.prefs.setIntPref(PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew);

  let localDateFmt = formatter.format(localDate);

  info("Loading a bad cert page with a skewed clock");
  let message = await setUpPage();

  isnot(
    message.divDisplay,
    "none",
    "Wrong time message information is visible"
  );
  ok(
    message.text.includes("update your computer clock"),
    "Correct error message found"
  );
  ok(
    message.text.includes("expired.example.com"),
    "URL found in error message"
  );
  ok(message.systemDate.includes(localDateFmt), "Correct local date displayed");
  ok(
    message.learnMoreLink.includes("time-errors"),
    "time-errors in the Learn More URL"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref(PREF_SERVICES_SETTINGS_LAST_FETCHED);
  Services.prefs.clearUserPref(PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS);
});

add_task(async function checkCertError() {
  async function setUpPage() {
    let browser;
    let certErrorLoaded;
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      "https://expired.example.com/"
    );
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);

    info("Loading and waiting for the cert error");
    await certErrorLoaded;

    return SpecialPowers.spawn(browser, [], async function() {
      let doc = content.document;
      let el = doc.getElementById("errorWhatToDoText");
      await ContentTaskUtils.waitForCondition(() => el.textContent);
      return el.textContent;
    });
  }

  // The particular error message will be displayed only when clock_skew_seconds is
  // less or equal to a day and the difference between date.now() and last_fetched is less than
  // or equal to 5 days. Setting the prefs accordingly.

  Services.prefs.setIntPref(
    PREF_SERVICES_SETTINGS_LAST_FETCHED,
    Math.floor(Date.now() / 1000)
  );

  let skew = 60 * 60 * 24;
  Services.prefs.setIntPref(PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew);

  info("Loading a bad cert page");
  let message = await setUpPage();

  ok(
    message.includes(
      "The issue is most likely with the website, and there is nothing you can do" +
        " to resolve it. You can notify the website’s administrator about the problem."
    ),
    "Correct error message found"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref(PREF_SERVICES_SETTINGS_LAST_FETCHED);
  Services.prefs.clearUserPref(PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS);
});
