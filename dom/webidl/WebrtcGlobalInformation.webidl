/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary WebrtcGlobalStatisticsReport {
  sequence<RTCStatsReportInternal> reports;
};

callback WebrtcGlobalStatisticsCallback = void (WebrtcGlobalStatisticsReport reports);
callback WebrtcGlobalLoggingCallback = void (sequence<DOMString> logMessages);

[ChromeOnly]
interface WebrtcGlobalInformation {

  [Throws]
  static void getAllStats(WebrtcGlobalStatisticsCallback callback);

  [Throws]
  static void getLogging(DOMString pattern,
                         WebrtcGlobalLoggingCallback callback);
};


