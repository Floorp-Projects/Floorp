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
 * Created: Will Scullin <scullin@netscape.com>, 20 Nov 1997.
 */

package grendel.ui;

import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;

import javax.mail.Address;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.internet.InternetAddress;

import javax.swing.JLabel;
import javax.swing.border.BevelBorder;

public class SimpleMessageHeader extends MessageHeader {
  static final int kMargin = 3;

  JLabel fToLabel;
  JLabel fTo;
  JLabel fFromLabel;
  JLabel fFrom;
  JLabel fSubjectLabel;
  JLabel fSubject;

  public SimpleMessageHeader() {
    setLayout(null);
    setBorder(new BevelBorder(BevelBorder.RAISED));

    Font bold = Font.decode("Dialog-bold-12");
    Font plain = Font.decode("Dialog-12");

    fToLabel = new JLabel("To: ");
    fToLabel.setFont(bold);
    add(fToLabel);
    fTo = new JLabel();
    fTo.setFont(plain);
    add(fTo);

    fFromLabel = new JLabel("From: ");
    fFromLabel.setFont(bold);
    add(fFromLabel);
    fFrom = new JLabel();
    fFrom.setFont(plain);
    add(fFrom);

    fSubjectLabel = new JLabel("Subject: ");
    fSubjectLabel.setFont(bold);
    add(fSubjectLabel);
    fSubject = new JLabel();
    fSubject.setFont(plain);
    add(fSubject);
  }

  public void setMessage(Message aMessage) {
    fMessage = aMessage;

    if (fMessage != null) {
      try {
        StringBuffer toString = new StringBuffer();
        StringBuffer fromString = new StringBuffer();
        String personal = null;

        Address recipients[] = fMessage.getRecipients(Message.RecipientType.TO);
        Address sender[] = fMessage.getFrom();

        int i;
        if (recipients != null) {
          for (i = 0; i < recipients.length; i++) {
            personal = null;
            if (recipients[i] instanceof InternetAddress) {
              personal = ((InternetAddress) recipients[i]).getPersonal();
            }
            if (personal != null) {
              toString.append(personal);
            } else {
              toString.append(recipients[i].toString());
            }
            if (i < (recipients.length - 1)) {
              toString.append(", ");
            }
          }
        }

        personal = null;
        if (sender != null && sender[0] instanceof InternetAddress) {
          personal = ((InternetAddress) sender[0]).getPersonal();
        }

        if (personal != null) {
          fromString.append(personal);
        } else if (sender != null && sender.length > 0) {
          fromString.append(sender[0].toString());
        }

        fTo.setText(toString.toString());
        fFrom.setText(fromString.toString());
        fSubject.setText(fMessage.getSubject());
      } catch (MessagingException e) {
        System.err.println("SimpleMessageHeader.setMessage(): " + e);
      }
    } else {
      fTo.setText("");
      fFrom.setText("");
      fSubject.setText("");
    }

    invalidate();
    validate();
  }

  public Dimension getPreferredSize() {
    Insets insets = getInsets();
    insets.top += kMargin;
    insets.left += kMargin;
    insets.bottom += kMargin;
    insets.right += kMargin;

    int width1 = 0;
    int height1 = 0;
    int width2 = 0;
    int height2 = 0;
    Dimension dim;

    dim = fToLabel.getPreferredSize();
    width1 = dim.width;
    height1 = dim.height;

    dim = fTo.getPreferredSize();
    width1 += dim.width;
    height1 = Math.max(height1, dim.height);

    width1 += kMargin * 2;

    dim = fFromLabel.getPreferredSize();
    width1 += dim.width;
    height1 = Math.max(height1, dim.height);

    dim = fFrom.getPreferredSize();
    width1 += dim.width;
    height1 = Math.max(height1, dim.height);

    dim = fSubjectLabel.getPreferredSize();
    width2 = dim.width;
    height2 = dim.height;

    dim = fSubject.getPreferredSize();
    width2 += dim.width;
    height2 = Math.max(height2, dim.height);

    return new Dimension(insets.left + insets.right + Math.max(width1, width2),
                         insets.top + insets.bottom + height1 + height2);
  }

  public void doLayout() {
    Insets insets = getInsets();
    insets.top += kMargin;
    insets.left += kMargin;
    insets.bottom += kMargin;
    insets.right += kMargin;

    int width = insets.left;
    int height = insets.top;
    Dimension dim;

    dim = fToLabel.getPreferredSize();
    fToLabel.setBounds(width, insets.top, dim.width, dim.height);
    width += dim.width;
    height += dim.height;

    dim = fTo.getPreferredSize();
    fTo.setBounds(width, insets.top, dim.width, dim.height);
    width += dim.width;
    height = Math.max(height, insets.top + dim.height);

    width += kMargin * 2;

    dim = fFromLabel.getPreferredSize();
    fFromLabel.setBounds(width, insets.top, dim.width, dim.height);
    width += dim.width;
    height = Math.max(height, insets.top + dim.height);

    dim = fFrom.getPreferredSize();
    fFrom.setBounds(width, insets.top, dim.width, dim.height);
    height = Math.max(height, insets.top + dim.height);

    width = insets.left;

    dim = fSubjectLabel.getPreferredSize();
    fSubjectLabel.setBounds(width, height, dim.width, dim.height);
    width += dim.width;

    dim = fSubject.getPreferredSize();
    fSubject.setBounds(width, height, dim.width, dim.height);
  }
}
