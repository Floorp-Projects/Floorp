/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.lang.Math;
import java.util.Date;

import android.util.Log;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import android.os.BatteryManager;

public class GeckoBatteryManager
  extends BroadcastReceiver
{
  // Those constants should be keep in sync with the ones in:
  // dom/battery/Constants.h
  private final static double  kDefaultLevel         = 1.0;
  private final static boolean kDefaultCharging      = true;
  private final static double  kDefaultRemainingTime = -1.0;
  private final static double  kUnknownRemainingTime = -1.0;

  private static Date    sLastLevelChange            = new Date(0);
  private static boolean sNotificationsEnabled       = false;
  private static double  sLevel                      = kDefaultLevel;
  private static boolean sCharging                   = kDefaultCharging;
  private static double  sRemainingTime              = kDefaultRemainingTime;;

  @Override
  public void onReceive(Context context, Intent intent) {
    if (!intent.getAction().equals(Intent.ACTION_BATTERY_CHANGED)) {
      Log.e("GeckoBatteryManager", "Got an unexpected intent!");
      return;
    }

    boolean previousCharging = isCharging();
    double previousLevel = getLevel();

    if (intent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false)) {
      int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
      if (plugged == -1) {
        sCharging = kDefaultCharging;
        Log.e("GeckoBatteryManager", "Failed to get the plugged status!");
      } else {
        // Likely, if plugged > 0, it's likely plugged and charging but the doc
        // isn't clear about that.
        sCharging = plugged != 0;
      }

      if (sCharging != previousCharging) {
        sRemainingTime = kUnknownRemainingTime;
        // The new remaining time is going to take some time to show up but
        // it's the best way to show a not too wrong value.
        sLastLevelChange = new Date(0);
      }

      // We need two doubles because sLevel is a double.
      double current =  (double)intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
      double max = (double)intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
      if (current == -1 || max == -1) {
        Log.e("GeckoBatteryManager", "Failed to get battery level!");
        sLevel = kDefaultLevel;
      } else {
        sLevel = current / max;
      }

      if (sLevel == 1.0 && sCharging) {
        sRemainingTime = 0.0;
      } else if (sLevel != previousLevel) {
        // Estimate remaining time.
        if (sLastLevelChange.getTime() != 0) {
          Date currentTime = new Date();
          long dt = (currentTime.getTime() - sLastLevelChange.getTime()) / 1000;
          double dLevel = sLevel - previousLevel;

          if (sCharging) {
            if (dLevel < 0) {
              Log.w("GeckoBatteryManager", "When charging, level should increase!");
              sRemainingTime = kUnknownRemainingTime;
            } else {
              sRemainingTime = Math.round(dt / dLevel * (1.0 - sLevel));
            }
          } else {
            if (dLevel > 0) {
              Log.w("GeckoBatteryManager", "When discharging, level should decrease!");
              sRemainingTime = kUnknownRemainingTime;
            } else {
              sRemainingTime = Math.round(dt / -dLevel * sLevel);
            }
          }

          sLastLevelChange = currentTime;
        } else {
          // That's the first time we got an update, we can't do anything.
          sLastLevelChange = new Date();
        }
      }
    } else {
      sLevel = kDefaultLevel;
      sCharging = kDefaultCharging;
      sRemainingTime = kDefaultRemainingTime;
    }

    /*
     * We want to inform listeners if the following conditions are fulfilled:
     *  - we have at least one observer;
     *  - the charging state or the level has changed.
     *
     * Note: no need to check for a remaining time change given that it's only
     * updated if there is a level change or a charging change.
     *
     * The idea is to prevent doing all the way to the DOM code in the child
     * process to finally not send an event.
     */
    if (sNotificationsEnabled &&
        (previousCharging != isCharging() || previousLevel != getLevel())) {
      GeckoAppShell.notifyBatteryChange(getLevel(), isCharging(), getRemainingTime());
    }
  }

  public static boolean isCharging() {
    return sCharging;
  }

  public static double getLevel() {
    return sLevel;
  }

  public static double getRemainingTime() {
    return sRemainingTime;
  }

  public static void enableNotifications() {
    sNotificationsEnabled = true;
  }

  public static void disableNotifications() {
    sNotificationsEnabled = false;
  }

  public static double[] getCurrentInformation() {
    return new double[] { getLevel(), isCharging() ? 1.0 : 0.0, getRemainingTime() };
  }
}
