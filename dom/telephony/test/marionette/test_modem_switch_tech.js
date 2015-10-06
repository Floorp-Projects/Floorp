/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

var settings = [
  // "gsm/wcdma"
  {tech: "gsm",   mask: "gsm/wcdma"},
  {tech: "wcdma", mask: "gsm/wcdma"},

  // "gsm"
  {tech: "gsm",   mask: "gsm"},

  // "wcdma"
  {tech: "wcdma", mask: "wcdma"},

  // "gsm/wcdma-auto"
  {tech: "gsm",   mask: "gsm/wcdma-auto"},
  {tech: "wcdma", mask: "gsm/wcdma-auto"},

  // "cdma/evdo"
  {tech: "cdma",  mask: "cdma/evdo"},
  {tech: "evdo",  mask: "cdma/evdo"},

  // "cdma"
  {tech: "cdma",  mask: "cdma"},

  // "evdo"
  {tech: "evdo",  mask: "evdo"},

  // "gsm/wcdma/cdma/evdo"
  {tech: "gsm",   mask: "gsm/wcdma/cdma/evdo"},
  {tech: "wcdma", mask: "gsm/wcdma/cdma/evdo"},
  {tech: "cdma",  mask: "gsm/wcdma/cdma/evdo"},
  {tech: "evdo",  mask: "gsm/wcdma/cdma/evdo"}
];

startTest(function() {

  let promise = settings.reduce((aPromise, aSetting) => {
    return aPromise.then(() => Modem.changeTech(aSetting.tech, aSetting.mask));
  }, Promise.resolve());

  return promise
    // Exception Catching
    .catch(error => ok(false, "Promise reject: " + error))

    // Switch to the default modem tech
    .then(() => Modem.changeTech("wcdma", "gsm/wcdma"))
    .catch(error => ok(false, "Fetal Error: Promise reject: " + error))

    .then(finish);
});
