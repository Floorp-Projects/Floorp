/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 */

package grendel.ui;

import java.io.IOException;
import java.util.Enumeration;

import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.EventListenerList;

import calypso.util.PreferencesFactory;
import calypso.util.Preferences;

import grendel.storage.MailDrop;

class BiffThread extends Thread {
  private static BiffThread fThread = null;

  EventListenerList  fListeners = new EventListenerList();
  int                 fBiffState;

  static BiffThread Get() {
    if (fThread == null) {
      fThread = new BiffThread();
      fThread.start();
    }
    return fThread;
  }

  private BiffThread() {
    super("biff");
    setDaemon(true);

  }

  public void addChangeListener(ChangeListener aListener) {
    fListeners.add(ChangeListener.class, aListener);
  }

  public void removeChangeListener(ChangeListener aListener) {
    fListeners.remove(ChangeListener.class, aListener);
  }

  public int getBiffState() {
    return fBiffState;
  }

  public void setBiffState(int aBiffState) {
    if (aBiffState != fBiffState) {
      fBiffState = aBiffState;

      Object[] listeners = fListeners.getListenerList();
      ChangeEvent changeEvent = null;
      for (int i = 0; i < listeners.length - 1; i += 2) {
        if (listeners[i] == ChangeListener.class) {
          // Lazily create the event:
          if (changeEvent == null)
            changeEvent = new ChangeEvent(this);
          ((ChangeListener)listeners[i+1]).stateChanged(changeEvent);
        }
      }
    }
  }

  public void run() {
    // ### Stubbing out maildrop stuff for now.

//    Preferences prefs = PreferencesFactory.Get();
//    try {
//      while (true) {
//        boolean unknown = false;
//        boolean newMail = false;
//        boolean none = false;
//
//        Enumeration maildrops = fMaster.getMailDrops();
//        while (maildrops.hasMoreElements()) {
//          MailDrop maildrop = (MailDrop) maildrops.nextElement();
//
//          try {
//            maildrop.doBiff();
//            switch (maildrop.getBiffState()) {
//            case MailDrop.NEW:
//              newMail = true;
//              break;
//
//            case MailDrop.NONE:
//              none = true;
//              break;
//
//            case MailDrop.UNKNOWN:
//              unknown = true;
//              break;
//            }
//          } catch (IOException e) {
//            System.err.println("BiffIcon: " + e);
//          }
//        }
//
//        if (newMail) {
//          setBiffState(MailDrop.NEW);
//        } else {
//          if (none && !unknown) {
//            setBiffState(MailDrop.NONE);
//          } else {
//            setBiffState(MailDrop.UNKNOWN);
//          }
//        }
//
//        int interval = prefs.getInt("mail.biff_interval", 300);
//        Thread.sleep(interval * 1000);
//      }
//    } catch (InterruptedException e) {
//    }
  }
}
