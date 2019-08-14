/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const BASE_PARAMS = {
  utm_source: "activity-stream",
  utm_campaign: "firstrun",
  utm_medium: "referral",
};

/**
 * Takes in a url as a string or URL object and returns a URL object with the
 * utm_* parameters added to it. If a URL object is passed in, the paraemeters
 * are added to it (the return value can be ignored in that case as it's the
 * same object).
 */
export function addUtmParams(url, utmTerm) {
  let returnUrl = url;
  if (typeof returnUrl === "string") {
    returnUrl = new URL(url);
  }
  Object.keys(BASE_PARAMS).forEach(key => {
    returnUrl.searchParams.append(key, BASE_PARAMS[key]);
  });
  returnUrl.searchParams.append("utm_term", utmTerm);
  return returnUrl;
}
