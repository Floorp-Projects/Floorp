/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com>
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

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.widget.RemoteViews;

public class AlertNotification
    extends Notification
{
    Context mContext;
    int mId;
    int mIcon;
    String mTitle;
    boolean mProgressStyle;
    NotificationManager mNotificationManager;

    public AlertNotification(Context aContext, int aNotificationId, int aIcon, String aTitle, long aWhen) {
        super(aIcon, aTitle, aWhen);

        mContext = aContext;
        mIcon = aIcon;
        mTitle = aTitle;
        mProgressStyle = false;
        mId = aNotificationId;

        mNotificationManager = (NotificationManager)
            mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }

    public boolean isProgressStyle() {
        return mProgressStyle;
    }

    public void show() {
        mNotificationManager.notify(mId, this);
    }

    public void updateProgress(String aAlertText, long aProgress, long aProgressMax) {
        if (!mProgressStyle) {
            // Custom view
            int layout = aAlertText.length() > 0 ? R.layout.notification_progress_text : R.layout.notification_progress;

            RemoteViews view = new RemoteViews("org.mozilla." + GeckoApp.mAppContext.getAppName(), layout);
            view.setImageViewResource(R.id.notificationImage, mIcon);
            view.setTextViewText(R.id.notificationTitle, mTitle);
            contentView = view;
            flags |= FLAG_ONGOING_EVENT;

            mProgressStyle = true;
        }

        String text;
        if (aAlertText.length() > 0) {
            text = aAlertText;
        } else {
            int percent = 0;
            if (aProgressMax > 0)
                percent = (int)(100 * aProgress / aProgressMax);
            text = percent + "%";
        }

        contentView.setTextViewText(R.id.notificationText, text);
        contentView.setProgressBar(R.id.notificationProgressbar, (int)aProgressMax, (int)aProgress, false);

        // Update the notification
        mNotificationManager.notify(mId, this);
    }
}
