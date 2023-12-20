/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary WebrtcGlobalStatisticsReport {
  sequence<RTCStatsReportInternal> reports = [];
  sequence<RTCSdpHistoryInternal> sdpHistories = [];
};

dictionary WebrtcGlobalMediaContext {
  required boolean hasH264Hardware;
};

callback WebrtcGlobalStatisticsCallback = undefined (WebrtcGlobalStatisticsReport reports);
callback WebrtcGlobalStatisticsHistoryPcIdsCallback = undefined (sequence<DOMString> pcIds);
callback WebrtcGlobalStatisticsHistoryCallback = undefined (WebrtcGlobalStatisticsReport reports);
callback WebrtcGlobalLoggingCallback = undefined (sequence<DOMString> logMessages);

[ChromeOnly, Exposed=Window]
namespace WebrtcGlobalInformation {

  [Throws]
  undefined getAllStats(WebrtcGlobalStatisticsCallback callback,
                        optional DOMString pcIdFilter);

  [Throws]
  undefined getStatsHistoryPcIds(WebrtcGlobalStatisticsHistoryPcIdsCallback callback);

  [Throws]
  undefined getStatsHistorySince(WebrtcGlobalStatisticsHistoryCallback callback,
                                 DOMString pcIdFilter,
                                 optional DOMHighResTimeStamp after,
                                 optional DOMHighResTimeStamp sdpAfter);

  WebrtcGlobalMediaContext getMediaContext();

  undefined clearAllStats();

  [Throws]
  undefined getLogging(DOMString pattern, WebrtcGlobalLoggingCallback callback);

  undefined clearLogging();

  // WebRTC AEC debugging enable
  attribute boolean aecDebug;

  readonly attribute DOMString aecDebugLogDir;
};
