/* eslint-env worker */

self.onmessage = function () {
  let myLocale = Intl.NumberFormat().resolvedOptions().locale;
  self.postMessage(myLocale);
};
