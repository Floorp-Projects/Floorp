/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Attachment.java
 *
 * Created on 08 August 2005, 17:15
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

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import javax.mail.MessagingException;
import javax.mail.Part;

import javax.swing.JFileChooser;


/**
 *
 * @author hash9
 */
public class Attachment
{
  private Part p;
  private String url;

  /** Creates a new instance of Attachment */
  public Attachment(Part p, String url)
  {
    this.p=p;
    this.url=url;
    grendel.renderer.URL.attachment.Handler.registerAttachment(url, this);
  }

  /**
   * Return all the headers from this part as an Enumeration of
   * Header objects.
   *
   *
   * @return enumeration of Header objects
   * @exception MessagingException
   */
  public java.util.Enumeration getAllHeaders()
                                      throws javax.mail.MessagingException
  {
    return p.getAllHeaders();
  }

  /**
   * Returns the Content-Type of the content of this part.
   * Returns null if the Content-Type could not be determined. <p>
   *
   * The MIME typing system is used to name Content-types.
   *
   * @return                The ContentType of this part
   * @exception        MessagingException
   * @see                javax.activation.DataHandler
   */
  public String getContentType() throws javax.mail.MessagingException
  {
    return p.getContentType();
  }

  /**
   * Return a DataHandler for the content within this part. The
   * DataHandler allows clients to operate on as well as retrieve
   * the content.
   *
   * @return                DataHandler for the content
   * @exception         MessagingException
   */
  public javax.activation.DataHandler getDataHandler()
    throws javax.mail.MessagingException
  {
    return p.getDataHandler();
  }

  /**
   * Get the filename associated with this part, if possible.
   * Useful if this part represents an "attachment" that was
   * loaded from a file.  The filename will usually be a simple
   * name, not including directory components.
   *
   * @return        Filename to associate with this part
   */
  public String getFileName() throws javax.mail.MessagingException
  {
    return p.getFileName();
  }

  /**
   * Get all the headers for this header name. Returns <code>null</code>
   * if no headers for this header name are available.
   *
   * @param header_name       the name of this header
   * @return                  the value fields for all headers with
   *                                 this name
   * @exception               MessagingException
   */
  public String[] getHeader(String header_name)
                     throws javax.mail.MessagingException
  {
    return p.getHeader(header_name);
  }

  /**
   * Return an input stream for this part's "content". Any
   * mail-specific transfer encodings will be decoded before the
   * input stream is provided. <p>
   *
   * This is typically a convenience method that just invokes
   * the DataHandler's <code>getInputStream()</code> method.
   *
   * @return an InputStream
   * @exception        IOException this is typically thrown by the
   *                         DataHandler. Refer to the documentation for
   *                         javax.activation.DataHandler for more details.
   * @exception        MessagingException
   * @see #getDataHandler
   * @see javax.activation.DataHandler#getInputStream
   */
  public java.io.InputStream getInputStream()
                                     throws java.io.IOException, 
                                            javax.mail.MessagingException
  {
    return p.getInputStream();
  }

  public String getURL()
  {
    return url;
  }

  public void save()
  {
    JFileChooser f_c=new JFileChooser();

    f_c.setDialogTitle("Save Attachment");

    try {
      String filename=p.getFileName();

      if (filename==null) {
        filename="attachment";
      }

      f_c.setSelectedFile(new File(filename));
      f_c.setDialogTitle("Save Attachment: "+p.getFileName());
    } catch (MessagingException me) {
    }

    int result=f_c.showSaveDialog(null); //show the dialog. This blocks.

    if (result==JFileChooser.APPROVE_OPTION) {
      File f=f_c.getSelectedFile();

      if (f!=null) {
        try {
          FileOutputStream fos=new FileOutputStream(f);
          writeTo(fos);
        } catch (IOException ioe) {
          ioe.printStackTrace();
        }
      }
    }
  }

  public void writeTo(OutputStream os) throws IOException
  {
    try {
      InputStream is=new BufferedInputStream(getInputStream());
      int i=0;

      while (i!=-1) {
        i=is.read();
        os.write(i);
      }

      os.flush();
      os.close();
    } catch (MessagingException me) {
      IOException e=new IOException("Mail Exception: "+me.getMessage());
      e.initCause(me);
      throw e;
    }
  }
}
