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

import android.util.Log;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import android.os.BatteryManager;

public class GeckoBatteryManager
  extends BroadcastReceiver
{
  private final static float   kDefaultLevel       = 1.0f;
  private final static boolean kDefaultCharging    = true;
  private final static float   kMinimumLevelChange = 0.01f;

  private static boolean sNotificationsEnabled     = false;
  private static float   sLevel                    = kDefaultLevel;
  private static boolean sCharging                 = kDefaultCharging;

  @Override
  public void onReceive(Context context, Intent intent) {
    if (!intent.getAction().equals(Intent.ACTION_BATTERY_CHANGED)) {
      Log.e("GeckoBatteryManager", "Got an unexpected intent!");
      return;
    }

    boolean previousCharging = isCharging();
    float previousLevel = getLevel();

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

      // We need two floats because sLevel is a float.
      float current =  (float)intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
      float max = (float)intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
      if (current == -1 || max == -1) {
        Log.e("GeckoBatteryManager", "Failed to get battery level!");
        sLevel = kDefaultLevel;
      } else {
        sLevel = current / max;
      }
    } else {
      sLevel = kDefaultLevel;
      sCharging = kDefaultCharging;
    }

    /*
     * We want to inform listeners if the following conditions are fulfilled:
     *  - we have at least one observer;
     *  - the charging state has changed
     *    OR
     *    the level has changed of at least kMinimumLevelChange
     */
    if (sNotificationsEnabled &&
        (previousCharging != isCharging() ||
         Math.abs(previousLevel - getLevel()) >= kMinimumLevelChange)) {
      GeckoAppShell.notifyBatteryChange(sLevel, sCharging);
    }
  }

  public static boolean isCharging() {
    return sCharging;
  }

  public static float getLevel() {
    return sLevel;
  }

  public static void enableNotifications() {
    sNotificationsEnabled = true;
  }

  public static void disableNotifications() {
    sNotificationsEnabled = false;
  }

  public static float[] getCurrentInformation() {
    return new float[] { getLevel(), isCharging() ? 1.0f : 0.0f };
  }
}
