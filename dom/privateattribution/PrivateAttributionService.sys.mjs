/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *
 */
export class PrivateAttributionService {
  onAttributionEvent(sourceHost, type, index, ad, targetHost) {
    dump(
      `onAttributionEvent(${sourceHost}, ${type}, ${index}, ${ad}, ${targetHost})\n`
    );
  }

  onAttributionConversion(
    sourceHost,
    task,
    histogramSize,
    loopbackDays,
    impressionType,
    ads,
    sourceHosts
  ) {
    dump(
      `onAttributionConversion(${sourceHost}, ${task}, ${histogramSize}, ${loopbackDays}, ${impressionType}, ${ads}, ${sourceHosts})\n`
    );
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIPrivateAttributionService]);
}
