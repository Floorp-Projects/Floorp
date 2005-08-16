/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * RenderInputStream.java
 *
 * Created on 12 August 2005, 14:57
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
package grendel.renderer.tools;

import grendel.renderer.HTMLUtils;
import grendel.renderer.ObjectRender;

import java.io.InputStream;
import java.io.StringWriter;

import java.util.Enumeration;

import javax.mail.Header;
import javax.mail.MessagingException;
import javax.mail.Part;
import javax.mail.internet.InternetHeaders;
import javax.mail.internet.MimeBodyPart;


/**
 *
 * @author hash9
 */
public class RenderInputStream implements ObjectRender
{
  /** Creates a new instance of RenderInputStream */
  public RenderInputStream()
  {
  }

  public boolean accept(Object o)
  {
    if (o instanceof InputStream) {
      return true;
    } else {
      return false;
    }
  }

  public Class acceptable()
  {
    return InputStream.class;
  }

  public StringBuilder objectRenderer(
                                      Object o, String index, javax.mail.Part p,
                                      grendel.renderer.Renderer master)
                               throws javax.mail.MessagingException, 
                                      java.io.IOException
  {
    StringWriter sw=new StringWriter();
    InputStream is=(InputStream) o;
    int i=0;

    while (i!=-1) {
      i=is.read();
      sw.write(i);
    }

    StringBuilder buf=new StringBuilder();

    if (p.getContentType().contains("text/")) {
      buf.append("<br>\n<hr>\n<br>\n");
      buf.append("<table border=\"1\" cellspacing=\"0\">");

      Enumeration enumm=p.getAllHeaders();

      for (; enumm.hasMoreElements();) {
        Object o_1=enumm.nextElement();

        if (o_1 instanceof Header) {
          Header h=(Header) o_1;
          buf.append(HTMLUtils.genRow(h.getName(), h.getValue()));
        } else {
          buf.append(HTMLUtils.genRow("Other", o_1.toString()));
        }
      }

      buf.append("</table>");
      buf.append("<br>");
      buf.append("<blockquote>");
      buf.append("<pre>");
      buf.append(HTMLUtils.quoteToHTML(sw.toString()));
      buf.append("</pre>");
      buf.append("</blockquote>");
    } else {
      try {
        String s=sw.toString();
        InternetHeaders ih=new InternetHeaders();
        Enumeration enumm=p.getAllHeaders();

        for (; enumm.hasMoreElements();) {
          Header h=(Header) enumm.nextElement();
          ih.addHeader(h.getName(), h.getValue());
        }

        Part n=new MimeBodyPart(ih, s.getBytes());
        buf.append(master.makeAttachmentBox(index, n));
      } catch (MessagingException ex) {
        ex.printStackTrace();
      }
    }

    return buf;
  }
}
