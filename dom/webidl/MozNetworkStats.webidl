/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provide the detailed options for specifying different kinds of data filtering
 * in getSamples function.
 */
dictionary NetworkStatsGetOptions
{
  /**
   * App manifest URL is used to filter network stats by app, while service type
   * is used to filter stats by system service.
   * Note that, these two options cannot be specified at the same time for now;
   * others, an NS_ERROR_NOT_IMPLMENTED exception will be thrown.
   */
  DOMString appManifestURL;
  DOMString serviceType;
};

dictionary NetworkStatsAlarmOptions
{
  Date startTime;
  Date data;
};
