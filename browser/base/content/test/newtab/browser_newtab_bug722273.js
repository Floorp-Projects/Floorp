/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const NOW = Date.now() * 1000;
const URL = "http://fake-site.com/";

add_task(async function() {
  await Sanitizer.sanitize(["history"]);
  await promiseAddFakeVisits();
  await addNewTabPageTab();

  let cellUrl = await performOnCell(0, cell => { return cell.site.url; });
  is(cellUrl, URL, "first site is our fake site");

  let updatedPromise = whenPagesUpdated();
  await Sanitizer.sanitize(["history"]);
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
