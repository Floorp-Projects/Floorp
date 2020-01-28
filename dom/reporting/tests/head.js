"use strict";

/* exported storeReportingHeader */
async function storeReportingHeader(browser, reportingURL, extraParams = "") {
  await SpecialPowers.spawn(
    browser,
    [{ url: reportingURL, extraParams }],
    async obj => {
      await content
        .fetch(
          obj.url +
            "?task=header" +
            (obj.extraParams.length ? "&" + obj.extraParams : "")
        )
        .then(r => r.text())
        .then(text => {
          is(text, "OK", "Report-to header sent");
        });
    }
  );
}

/* exported checkReport */
function checkReport(reportingURL) {
  return new Promise(resolve => {
    let id = setInterval(_ => {
      fetch(reportingURL + "?task=check")
        .then(r => r.text())
        .then(text => {
          if (text) {
            resolve(JSON.parse(text));
            clearInterval(id);
          }
        });
    }, 1000);
  });
}
