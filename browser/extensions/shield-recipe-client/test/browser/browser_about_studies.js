"use strict";

Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);
Cu.import("resource://shield-recipe-client-content/AboutPages.jsm", this);

function withAboutStudies(testFunc) {
  return async (...args) => (
    BrowserTestUtils.withNewTab("about:studies", async browser => (
      testFunc(...args, browser)
    ))
  );
}

decorate_task(
  withAboutStudies,
  async function testAboutStudiesWorks(browser) {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    ok(browser.contentDocumentAsCPOW.getElementById("app"), "App element was found");
  }
);

decorate_task(
  withPrefEnv({
    set: [["extensions.shield-recipe-client.shieldLearnMoreUrl", "http://test/%OS%/"]],
  }),
  withAboutStudies,
  async function testLearnMore(browser) {
    ContentTask.spawn(browser, null, () => {
      content.document.getElementById("shield-studies-learn-more").click();
    });
    await BrowserTestUtils.waitForLocationChange(gBrowser);

    const location = browser.currentURI.spec;
    is(
      location,
      AboutPages.aboutStudies.getShieldLearnMoreHref(),
      "Clicking Learn More opens the correct page on SUMO.",
    );
    ok(!location.includes("%OS%"), "The Learn More URL is formatted.");
  }
);

decorate_task(
  withAboutStudies,
  async function testUpdatePreferencesNewOrganization(browser) {
    let loadPromise = BrowserTestUtils.firstBrowserLoaded(window);

    // We have to use gBrowser instead of browser in most spots since we're
    // dealing with a new tab outside of the about:studies tab.
    const tab = await BrowserTestUtils.switchTab(gBrowser, () => {
      ContentTask.spawn(browser, null, () => {
        content.document.getElementById("shield-studies-update-preferences").click();
      });
    });

    await loadPromise;

    const location = gBrowser.currentURI.spec;
    is(
      location,
      "about:preferences#privacy",
      "Clicking Update Preferences opens the privacy section of the new about:preferences.",
    );

    await BrowserTestUtils.removeTab(tab);
  }
);

decorate_task(
  AddonStudies.withStudies([
    // Sort order should be study3, study1, study2 (order by enabled, then most recent).
    studyFactory({
      name: "A Fake Study",
      active: true,
      description: "A fake description",
      studyStartDate: new Date(2017),
    }),
    studyFactory({
      name: "B Fake Study",
      active: false,
      description: "A fake description",
      studyStartDate: new Date(2019),
    }),
    studyFactory({
      name: "C Fake Study",
      active: true,
      description: "A fake description",
      studyStartDate: new Date(2018),
    }),
  ]),
  withAboutStudies,
  async function testStudyListing([study1, study2, study3], browser) {
    await ContentTask.spawn(browser, [study1, study2, study3], async ([cStudy1, cStudy2, cStudy3]) => {
      const doc = content.document;

      function getStudyRow(docElem, studyName) {
        return docElem.querySelector(`.study[data-study-name="${studyName}"]`);
      }

      await ContentTaskUtils.waitForCondition(() => doc.querySelectorAll(".study-list .study").length);
      const studyRows = doc.querySelectorAll(".study-list .study");

      const names = Array.from(studyRows).map(row => row.querySelector(".study-name").textContent);
      Assert.deepEqual(
        names,
        [cStudy3.name, cStudy1.name, cStudy2.name],
        "Studies are sorted first by enabled status, and then by descending start date."
      );

      const study1Row = getStudyRow(doc, cStudy1.name);
      ok(
        study1Row.querySelector(".study-description").textContent.includes(cStudy1.description),
        "Study descriptions are shown in about:studies."
      );
      is(
        study1Row.querySelector(".study-status").textContent,
        "Active",
        "Active studies show an 'Active' indicator."
      );
      ok(
        study1Row.querySelector(".remove-button"),
        "Active studies show a remove button"
      );
      is(
        study1Row.querySelector(".study-icon").textContent.toLowerCase(),
        "a",
        "Study icons use the first letter of the study name."
      );

      const study2Row = getStudyRow(doc, cStudy2.name);
      is(
        study2Row.querySelector(".study-status").textContent,
        "Complete",
        "Inactive studies are marked as complete."
      );
      ok(
        !study2Row.querySelector(".remove-button"),
        "Inactive studies do not show a remove button"
      );

      study1Row.querySelector(".remove-button").click();
      await ContentTaskUtils.waitForCondition(() => (
        getStudyRow(doc, cStudy1.name).matches(".disabled")
      ));
      ok(
        getStudyRow(doc, cStudy1.name).matches(".disabled"),
        "Clicking the remove button updates the UI to show that the study has been disabled."
      );
    });

    const updatedStudy1 = await AddonStudies.get(study1.recipeId);
    ok(
      !updatedStudy1.active,
      "Clicking the remove button marks the study as inactive in storage."
    );
  }
);
