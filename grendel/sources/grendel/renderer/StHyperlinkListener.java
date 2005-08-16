/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * StHyperlinkListener.java
 *
 * Created on 09 August 2005, 15:29
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
package grendel.renderer;

import grendel.renderer.URL.URLExtWrapper;
import grendel.renderer.URL.attachment.AttachmentURLConnection;

import java.io.IOException;

import javax.swing.JTextPane;
import javax.swing.event.HyperlinkEvent;
import javax.swing.event.HyperlinkListener;


/**
 *
 * @author hash9
 */
public class StHyperlinkListener implements HyperlinkListener
{
  private JTextPane txt;

  /** Creates a new instance of StHyperlinkListener */
  public StHyperlinkListener(JTextPane txt)
  {
    this.txt=txt;
  }

  /**
   * Called when a hypertext link is updated.
   *
   * @param e the event responsible for the update
   */
  public void hyperlinkUpdate(HyperlinkEvent e)
  {
    try {
      if (e.getEventType().equals(HyperlinkEvent.EventType.ACTIVATED)) {
        /*if (e.getURL().getProtocol().equalsIgnoreCase("attachment:")) {
                System.out.println("Request for:"+ e.getURL().getQuery());
        }*/
        System.out.println("Request for url: "+e.getURL());
        System.out.println("Request for ref: "+e.getDescription());

        if (e.getURL()!=null) {
          try {
            //e.getURL().openConnection().getContent()
            //System.err.println("Content: " + e.getURL().openConnection().getContent());
            if (e.getURL().getProtocol().equalsIgnoreCase("attachment")) {
              AttachmentURLConnection uca=(AttachmentURLConnection) e.getURL()
                                          .openConnection();
              uca.save();
            } else {
              new URLExtWrapper(e.getURL()).openInBrowser();
            }
          } catch (IOException ioe) {
            ioe.printStackTrace();
          } catch (Exception ex) {
            ex.printStackTrace();
          }
        }
      }
    } catch (Exception e1) {
    }
  }
}
