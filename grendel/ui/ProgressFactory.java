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
 *
 * Created: Will Scullin <scullin@netscape.com>, 17 Nov 1997.
 */

package grendel.ui;

import java.awt.Frame;
import java.text.MessageFormat;
import java.util.ResourceBundle;
import java.util.Vector;

import javax.mail.Flags;
import javax.mail.Flags.Flag;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;

public class ProgressFactory {
  static Frame fNewMailFrame = null;

  public static synchronized Frame NewMailProgress() {
    if (fNewMailFrame == null) {
      fNewMailFrame = new NewMailProgress();
    } else {
      fNewMailFrame.toFront();
    }
    return fNewMailFrame;
  }

  public static Frame CopyMessagesProgress(Vector aMessages, Folder aDest) {
    return new CopyMessageProgress(aMessages, aDest);
  }

  public static Frame MoveMessagesProgress(Vector aMessages, Folder aDest) {
    return new MoveMessageProgress(aMessages, aDest);
  }

  public static Frame DeleteMessagesProgress(Vector aMessages) {
    return new DeleteMessageProgress(aMessages);
  }
}

class NewMailProgress extends ProgressFrame {
  public NewMailProgress() {
    super("newMailProgressLabel");

    start();
  }

  public void progressLoop() {
    // ### Stubbing out all maildrop stuff for now...
    //      Enumeration maildrops = MasterFactory.Get().getMailDrops();
    //      while (maildrops.hasMoreElements() && !isCanceled()) {
    //        MailDrop maildrop = (MailDrop) maildrops.nextElement();
    //        Object args[] = {};
    //        setStatus(MessageFormat.format(fLabels.getString("newMailStatus"),
    //                                       args));
    //        try {
    //          maildrop.fetchNewMail();
    //        } catch (IOException e) {
    //          System.err.println("NewMailAction: " + e);
    //        }
    //      }
  }
}

//
// CopyMessageProgress class
//

class CopyMessageProgress extends ProgressFrame {
  Vector fMessages;
  Folder fDest;

  CopyMessageProgress(Vector aMessages, Folder aDest) {
    super("copyingMessageLabel");

    fMessages = aMessages;
    fDest = aDest;

    setMax(aMessages.size());

    start();
  }

  public void progressLoop() {
    int idx = 0;

    while (idx < fMessages.size()  && !isCanceled()) {
      Message message = (Message) fMessages.elementAt(idx);
      Object args[] = {Util.GetSubject(message),
                       fDest.getName()};
      setStatus(MessageFormat.format(fLabels.getString("copyingMessageStatus"),
                                     args));
      try {
        Message mlist[] = { message };
        fDest.appendMessages(mlist);
      } catch (MessagingException e) {
        System.err.println("CopyMessageProgress: " + e);
      }
      idx++;
      setProgress(idx);
    }
  }
}

  //
  // MoveMessageProgress class
  //

class MoveMessageProgress extends ProgressFrame {
  Vector fMessages;
  Folder fDest;

  MoveMessageProgress(Vector aMessages, Folder aDest) {
    super("movingMessageLabel");

    fMessages = aMessages;
    fDest = aDest;

    setMax(aMessages.size());

    start();
    }

  public void progressLoop() {
    int idx = 0;

    while (idx < fMessages.size() && !isCanceled()) {
      Message message = (Message) fMessages.elementAt(idx);
      Object args[] = {Util.GetSubject(message),
                       fDest.getName()};
      setStatus(MessageFormat.format(fLabels.getString("movingMessageStatus"),
                                     args));
      try {
        Message mlist[] = { message };
        fDest.appendMessages(mlist);
        message.setFlag(Flags.Flag.DELETED, true);
      } catch (MessagingException e) {
        System.err.println("DeleteMessageProgress: " + e);
      }
      idx++;
      setProgress(idx);
    }
  }
}

  //
  // DeleteMessageProgress class
  //

class DeleteMessageProgress extends ProgressFrame {
  Vector fMessages;

  DeleteMessageProgress(Vector aMessages) {
    super("deletingMessageLabel");

    fMessages = aMessages;
    setMax(aMessages.size());

    start();
  }

  public void progressLoop() {
    int idx = 0;

    while (idx < fMessages.size() && !isCanceled()) {
      Message message = (Message) fMessages.elementAt(idx);
      Folder source = message.getFolder();

      Object args[] = {Util.GetSubject(message)};

     setStatus(MessageFormat.format(fLabels.getString("deletingMessageStatus"),
                                     args));
      // source.deleteMessage(message);
      try {
        Thread.sleep(500);
      } catch (InterruptedException e) {
      }
      idx++;
      setProgress(idx);
    }
  }
}

