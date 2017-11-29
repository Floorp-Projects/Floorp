/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const NOW = Date.now() * 1000;
const URL = "http://fake-site.com/";

var tmp = {};
Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tmp);

var {Sanitizer} = tmp;

add_task(async function() {
  await promiseSanitizeHistory();
  await promiseAddFakeVisits();
  await addNewTabPageTab();

  let cellUrl = await performOnCell(0, cell => { return cell.site.url; });
  is(cellUrl, URL, "first site is our fake site");

  let updatedPromise = whenPagesUpdated();
  await promiseSanitizeHistory();
  await updatedPromise;

  let isGone = await performOnCell(0, cell => { return cell.site == null; });
  ok(isGone, "fake site is gone");
});

function promiseAddFakeVisits() {
  let visits = [];
  for (let i = 59; i > 0; i--) {
    visits.push({
      visitDate: NOW - i * 60 * 1000000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
    });
  }
  let place = {
    uri: makeURI(URL),
    title: "fake site",
    visits
  };
  return new Promise((resolve, reject) => {
    PlacesUtils.asyncHistory.updatePlaces(place, {
      handleError: () => reject(new Error("Couldn't add visit")),
      handleResult() {},
      handleCompletion() {
        NewTabUtils.links.populateCache(function() {
          NewTabUtils.allPages.update();
          resolve();
        }, true);
      }
    });
  });
}

function promiseSanitizeHistory() {
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";

  let prefs = gPrefService.getBranch(s.prefDomain);
  prefs.setBoolPref("history", true);
  prefs.setBoolPref("downloads", false);
  prefs.setBoolPref("cache", false);
  prefs.setBoolPref("cookies", false);
  prefs.setBoolPref("formdata", false);
  prefs.setBoolPref("offlineApps", false);
  prefs.setBoolPref("passwords", false);
  prefs.setBoolPref("sessions", false);
  prefs.setBoolPref("siteSettings", false);

  return s.sanitize();
}
