/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Supported resource statistics
 */
enum ResourceType {
  "network",
  "power"
};

/**
 * List of system services supporting resource statistics
 */
enum SystemService {
  "ota",
  "tethering"
};

dictionary ResourceStatsOptions
{
  /**
   * |component| specifies which component's resource usage will be returned.
   * If null or undefined, sum of all components' usage is returned.
   *
   * |component| is expressed in "<component>:<id>", where <component> is the
   * name of the component and <id> is used to identify different entities.
   *
   * The <id> field is mainly used in specifying the identity of different SIMs
   * when quering mobile network usage, e.g. "mobile:<iccid>".
   *
   * Quering statistics of other components should specify the |component| to
   *  "<component>:0", such as "cpu:0" or "wifi:0".
   */
  DOMString? component = null;

  /**
   * |manifestURL| specifies the manifestURL of an application.
   * |systemService| specifies the system service.
   *
   * If both |systemService| and |manifestURL| are null or undefined, then a
   * system-wide resource statistics is returned.
   *
   * If |manifestURL| is specified, then the resource statistics of the
   * specified application is returned.
   *
   * If |systemService| is specified, then the resource statistics of the
   * specified system service is returned.
   *
   * If |systemService| and |manifestURL| are both specified, then the return
   * statistics indicates the resources that the system service consumed for
   * the application.
   */
  SystemService? serviceType = null;
  DOMString? manifestURL = null;
};

dictionary ResourceStatsAlarmOptions
{
  /**
   * |startTime| indicates the start time of counting the resource usage.
   *
   * |data| is used to reflect in the alarm object when the alarm is triggered.
   * |data| should be copied using the structured clone algorithm.
   */
  [EnforceRange] DOMTimeStamp   startTime;  // time in milliseconds since Epoch
  any                           data;
};

[CheckPermissions="resourcestats-manage",
 Pref="dom.resource_stats.enabled",
 AvailableIn="CertifiedApps",
// FeatureDetectible, // This should be specified after Bug 1009645 is resolved.
 JSImplementation="@mozilla.org/resourceStatsAlarm;1"]
interface ResourceStatsAlarm
{
  /**
   * ID of the alarm
   */
  readonly attribute unsigned long          alarmId;

  /**
   * Type of resource this alarm monitor
   */
  readonly attribute ResourceType           type;

  /**
   * The target component this alarm monitor.
   */
  readonly attribute DOMString?             component;

  /**
   * |manifestURL| specifies the manifestURL of an application.
   * |systemService| specifies the system service.
   *
   * Both attributes are null means that this alarm monitors a system-wide
   * resource usage.
   */
  readonly attribute SystemService?         serviceType;
  readonly attribute DOMString?             manifestURL;

  /**
   * |threshold| specifies the limit of resource usage.
   */
  readonly attribute unsigned long long     threshold;

  /**
   * |data| is used to reflect in the alarm object when the alarm is triggered.
   */
  readonly attribute any                    data;
};

[CheckPermissions="resourcestats-manage",
 Pref="dom.resource_stats.enabled",
 Constructor(ResourceType type),
 AvailableIn="CertifiedApps",
// FeatureDetectible, // This should be specified after Bug 1009645 is resolved.
 JSImplementation="@mozilla.org/resourceStatsManager;1"]
interface ResourceStatsManager
{
  /**
   * Query resource statistics.
   *
   * |statsOptions| specifies the detail of statistics of interest.
   *
   * |start| and |end| specifies the time range of interest, both included.
   * If |start| is null or undefined, retrieve the stats since measurements.
   * If |end| is null or undefined. retrieve the stats until the current time.
   *
   * If success, the fulfillment value is a ResourceStats object.
   */
  Promise getStats(optional ResourceStatsOptions statsOptions,
                   [EnforceRange] optional DOMTimeStamp? start = null,
                   [EnforceRange] optional DOMTimeStamp? end = null);

  /**
   * Clear resource statistics stored in database.
   *
   * |statsOptions| specifies the detail of statistics to delete.
   *
   * |start| and |end| specifies the time range of interest, both included.
   * If |start| is null or undefined, delete the stats since measurements.
   * If |end| is null or undefined. delete the stats until the current time.
   */
  Promise clearStats(optional ResourceStatsOptions statsOptions,
                     [EnforceRange] optional DOMTimeStamp? start = null,
                     [EnforceRange] optional DOMTimeStamp? end = null);

  /**
   * Clear all resource statistics stored in database.
   */
  Promise clearAllStats();

  /**
   * Install an alarm to monitor resource usage.
   *
   * The |threshold| specifies the limit of resource usage. When resource
   * usage reaches the threshold, a "resourceStats-alarm" system message
   * is sent to the application.
   *
   * |statsOptions| specifies the detail of statistics of interest.
   *
   * |alarmOptions| is a ResourceStatsAlarmOptions object.
   *
   * If success, the fulfillment value is an alarm ID.
   */
  Promise addAlarm([EnforceRange] unsigned long long threshold,
                   optional ResourceStatsOptions statsOptions,
                   optional ResourceStatsAlarmOptions alarmOptions);

  /**
   * Obtain alarms.
   *
   * If |statsOptions| is provided, then only the alarms monitoring that
   * resource are returned. Otherwise, all alarms set for this resource type
   * is returned.
   *
   * If success, the fulfillment value is an array of ResourceStatsAlarm.
   */
  Promise getAlarms(optional ResourceStatsOptions statsOptions);

  /**
   * Remove the specified alarm.
   *
   * |alarmId| specifies the alarm to be removed.
   */
  Promise removeAlarm([EnforceRange] unsigned long alarmId);

  /**
   * Remove all alarms.
   */
  Promise removeAllAlarms();

  /**
   * Enumerate components that have stored statistics in database.
   *
   * If success, the fulfillment value is an array of DOMString.
   */
  Promise getAvailableComponents();

  /**
   * Return supporting resource statistics, i.e. ["Network", "Power"]
   *
   * This should be specified as static attribute after Bug 863952 is resolved.
   */
  [Cached, Pure]
  readonly attribute sequence<DOMString> resourceTypes;

  /**
   * Time in milliseconds between statistics stored in database.
   *
   * This should be specified as static attribute after Bug 863952 is resolved.
   */
  readonly attribute unsigned long sampleRate;

  /**
   * Time in milliseconds recorded by the API until present time. All
   * statistics older than maxStorageAge from now are deleted.
   *
   * This should be specified as static attribute after Bug 863952 is resolved.
   */
  readonly attribute unsigned long long maxStorageAge;
};
