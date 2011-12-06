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

import java.util.ArrayList;
import java.util.Iterator;

import android.util.Log;

import android.app.PendingIntent;
import android.app.Activity;

import android.content.Intent;
import android.content.BroadcastReceiver;
import android.content.Context;

import android.os.Bundle;

import android.telephony.SmsManager;
import android.telephony.SmsMessage;

/**
 * This class is returning unique ids for PendingIntent requestCode attribute.
 * There are only |Integer.MAX_VALUE - Integer.MIN_VALUE| unique IDs available,
 * and they wrap around.
 */
class PendingIntentUID
{
  static private int sUID = Integer.MIN_VALUE;

  static public int generate() { return sUID++; }
}

/**
 * The envelope class contains all information that are needed to keep track of
 * a sent SMS.
 */
class Envelope
{
  protected int     mId;
  protected int     mRemainingParts;
  protected boolean mFailing;

  public Envelope(int aId, int aParts) {
    mId = aId;
    mRemainingParts = aParts;
    mFailing = false;
  }

  public void decreaseRemainingParts() {
    --mRemainingParts;
  }

  public boolean arePartsRemaining() {
    return mRemainingParts != 0;
  }

  public void markAsFailed() {
    mFailing = true;
  }

  public boolean isFailing() {
    return mFailing;
  }
}

/**
 * Postman class is a singleton that manages Envelope instances.
 */
class Postman
{
  public static final int kUnknownEnvelopeId = -1;

  private static final Postman sInstance = new Postman();

  private ArrayList<Envelope> mEnvelopes = new ArrayList<Envelope>(1);

  private Postman() {}

  public static Postman getInstance() {
    return sInstance;
  }

  public int createEnvelope(int aParts) {
    /*
     * We are going to create the envelope in the first empty slot in the array
     * list. If there is no empty slot, we create a new one.
     */
    int size = mEnvelopes.size();

    for (int i=0; i<size; ++i) {
      if (mEnvelopes.get(i) == null) {
        mEnvelopes.set(i, new Envelope(i, aParts));
        return i;
      }
    }

    mEnvelopes.add(new Envelope(size, aParts));
    return size;
  }

  public Envelope getEnvelope(int aId) {
    if (aId < 0 || mEnvelopes.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to get an unknown Envelope!");
      return null;
    }

    Envelope envelope = mEnvelopes.get(aId);
    if (envelope == null) {
      Log.e("GeckoSmsManager", "Trying to get an empty Envelope!");
    }

    return envelope;
  }

  public void destroyEnvelope(int aId) {
    if (aId < 0 || mEnvelopes.size() <= aId) {
      Log.e("GeckoSmsManager", "Trying to destroy an unknown Envelope!");
      return;
    }

    if (mEnvelopes.set(aId, null) == null) {
      Log.e("GeckoSmsManager", "Trying to destroy an empty Envelope!");
    }
  }
}

public class GeckoSmsManager
  extends BroadcastReceiver
{
  public final static String ACTION_SMS_RECEIVED = "android.provider.Telephony.SMS_RECEIVED";
  public final static String ACTION_SMS_SENT     = "org.mozilla.gecko.SMS_SENT";
  private final static int kMaxMessageSize = 160;

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals(ACTION_SMS_RECEIVED)) {
      // TODO: Try to find the receiver number to be able to populate
      //       SmsMessage.receiver.
      // TODO: Get the id and the date from the stock app saved message.
      //       Using the stock app saved message require us to wait for it to
      //       be saved which can lead to race conditions.

      Bundle bundle = intent.getExtras();

      if (bundle == null) {
        return;
      }

      Object[] pdus = (Object[]) bundle.get("pdus");

      for (int i=0; i<pdus.length; ++i) {
        SmsMessage msg = SmsMessage.createFromPdu((byte[])pdus[i]);

        GeckoAppShell.notifySmsReceived(msg.getDisplayOriginatingAddress(),
                                        msg.getDisplayMessageBody(),
                                        System.currentTimeMillis());
      }

      return;
    }

    if (intent.getAction().equals(ACTION_SMS_SENT)) {
      Bundle bundle = intent.getExtras();

      if (bundle == null || !bundle.containsKey("envelopeId") ||
          !bundle.containsKey("number") || !bundle.containsKey("message")) {
        Log.e("GeckoSmsManager", "Got an invalid ACTION_SMS_SENT!");
        return;
      }

      int envelopeId = bundle.getInt("envelopeId");
      Postman postman = Postman.getInstance();
      Envelope envelope = postman.getEnvelope(envelopeId);

      envelope.decreaseRemainingParts();

      if (getResultCode() != Activity.RESULT_OK) {
        // TODO: manage error types.
        Log.i("GeckoSmsManager", "SMS part sending failed!");
        envelope.markAsFailed();
      }

      if (envelope.arePartsRemaining()) {
        return;
      }

      if (envelope.isFailing()) {
        // TODO: inform about the send failure.
        Log.i("GeckoSmsManager", "SMS sending failed!");
      } else {
        // TODO: inform about the send success.
        Log.i("GeckoSmsManager", "SMS sending was successfull!");
      }

      postman.destroyEnvelope(envelopeId);

      return;
    }
  }

  public static int getNumberOfMessagesForText(String aText) {
    return SmsManager.getDefault().divideMessage(aText).size();
  }

  public static void send(String aNumber, String aMessage) {
    /*
     * TODO:
     * This is a basic send method that doesn't handle errors and doesn't listen to
     * delivered messages.
     */
    int envelopeId = Postman.kUnknownEnvelopeId;

    try {
      SmsManager sm = SmsManager.getDefault();

      Intent sentIntent = new Intent(ACTION_SMS_SENT);
      Bundle bundle = new Bundle();
      bundle.putString("number", aNumber);
      bundle.putString("message", aMessage);

      if (aMessage.length() <= kMaxMessageSize) {
        envelopeId = Postman.getInstance().createEnvelope(1);

        bundle.putInt("envelopeId", envelopeId);
        sentIntent.putExtras(bundle);

        /*
         * There are a few things to know about getBroadcast and pending intents:
         * - the pending intents are in a shared pool maintained by the system;
         * - each pending intent is identified by a token;
         * - when a new pending intent is created, if it has the same token as
         *   another intent in the pool, one of them has to be removed.
         *
         * To prevent having a hard time because of this situation, we give a
         * unique id to all pending intents we are creating. This unique id is
         * generated by GetPendingIntentUID().
         */
        PendingIntent sentPendingIntent =
          PendingIntent.getBroadcast(GeckoApp.surfaceView.getContext(),
                                     PendingIntentUID.generate(), sentIntent,
                                     PendingIntent.FLAG_CANCEL_CURRENT);

        sm.sendTextMessage(aNumber, "", aMessage, sentPendingIntent, null);
      } else {
        ArrayList<String> parts = sm.divideMessage(aMessage);
        envelopeId = Postman.getInstance().createEnvelope(parts.size());

        bundle.putInt("envelopeId", envelopeId);
        sentIntent.putExtras(bundle);

        ArrayList<PendingIntent> sentPendingIntents =
          new ArrayList<PendingIntent>(parts.size());

        for (int i=0; i<parts.size(); ++i) {
          sentPendingIntents.add(
            PendingIntent.getBroadcast(GeckoApp.surfaceView.getContext(),
                                       PendingIntentUID.generate(), sentIntent,
                                       PendingIntent.FLAG_CANCEL_CURRENT)
          );
        }

        sm.sendMultipartTextMessage(aNumber, "", parts, sentPendingIntents, null);
      }
    } catch (Exception e) {
      Log.e("GeckoSmsManager", "Failed to send an SMS: ", e);

      if (envelopeId != Postman.kUnknownEnvelopeId) {
        Postman.getInstance().destroyEnvelope(envelopeId);
      }
    }
  }
}
