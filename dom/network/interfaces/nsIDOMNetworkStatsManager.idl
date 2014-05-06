/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.idl"

interface nsIDOMDOMRequest;

[scriptable,  uuid(ceb874cd-cc1a-4e65-b404-cc2d3e42425f)]
interface nsIDOMMozNetworkStatsManager : nsISupports
{
  /**
   * Constants for known interface types.
   */
  const long WIFI = 0;
  const long MOBILE = 1;

  /**
   * Find samples between two dates start and end, both included.
   *
   * If options is provided, per-app or per-system service usage will be
   * retrieved; otherwise the target will be overall system usage.
   *
   * If success, the request result will be an nsIDOMMozNetworkStats object.
   */
  nsIDOMDOMRequest getSamples(in nsISupports network,
                              in jsval start,
                              in jsval end,
                   [optional] in jsval options /* NetworkStatsGetOptions */);

  /**
   * Install an alarm on a network. The network must be in the return of
   * getAvailableNetworks() otherwise an "InvalidNetwork" exception will
   * be raised.
   *
   * When total data usage reaches threshold bytes, a "networkstats-alarm"
   * system message is sent to the application, where the optional parameter
   * |data| must be a cloneable object.
   *
   * If success, the |result| field of the DOMRequest keeps the alarm Id.
   */
  nsIDOMDOMRequest addAlarm(in nsISupports network,
                            in long threshold,
                            [optional] in jsval options /* NetworkStatsAlarmOptions */);

  /**
   * Obtain all alarms for those networks returned by getAvailableNetworks().
   * If a network is provided, only retrieves the alarms for that network.
   * The network must be one of those returned by getAvailebleNetworks() or an
   * "InvalidNetwork" exception will be raised.
   *
   * Each alarm object has the same fields as that in the system message:
   *  - alarmId
   *  - network
   *  - threshold
   *  - data
   */
  nsIDOMDOMRequest getAllAlarms([optional] in nsISupports network);

  /**
   * Remove all network alarms. If an |alarmId| is provided, then only that
   * alarm is removed.
   */
  nsIDOMDOMRequest removeAlarms([optional] in long alarmId);

  /**
   * Remove all stats related with the provided network from DB.
   */
  nsIDOMDOMRequest clearStats(in nsISupports network);

  /**
   * Remove all stats in the database.
   */
  nsIDOMDOMRequest clearAllStats();

  /**
   * Return available networks that used to be saved in the database.
   */
  nsIDOMDOMRequest getAvailableNetworks(); // array of MozNetworkStatsInterface.

  /**
   * Return available service types that used to be saved in the database.
   */
  nsIDOMDOMRequest getAvailableServiceTypes(); // array of string.

  /**
   * Minimum time in milliseconds between samples stored in the database.
   */
  readonly attribute long sampleRate;

  /**
   * Time in milliseconds recorded by the API until present time. All samples
   * older than maxStorageAge from now are deleted.
   */
  readonly attribute long long maxStorageAge;
};
