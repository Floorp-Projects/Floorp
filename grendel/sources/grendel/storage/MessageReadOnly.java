/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Terry Weissman <terry@netscape.com>, 24 Nov 1997.
 */

package grendel.storage;

import calypso.util.NetworkDate;

import calypso.util.Assert;
import calypso.util.ByteBuf;

import java.io.InputStream;
import java.io.IOException;
import java.util.Date;
import java.util.Enumeration;
import java.util.Vector;

import javax.activation.DataHandler;

import javax.mail.Address;
import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.IllegalWriteException;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.MethodNotSupportedException;
import javax.mail.Multipart;
import javax.mail.Part;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.InternetHeaders;

/** This implements those methods on Message which are commonly used in all
  of our read-only message implementations.  It also provides an
  implementation for the header-reading routines, all of which assume that
  the subclass implements getHeadersObj, a method to get an InternetHeaders
  object for this message. */

abstract class MessageReadOnly extends Message {

  MessageReadOnly() {
    super();
  }

  MessageReadOnly(Folder f, int num) {
    super(f, num);
  }

  protected void readonly() throws MessagingException {
    throw new IllegalWriteException("Can't edit this message.");
  }

  public void addFrom(Address addresses[]) throws MessagingException {
    readonly();
  }

  public void setSubject(String s)throws MessagingException {
    readonly();
  }

  public void setRecipients(Message.RecipientType type, Address addresses[])
    throws MessagingException
  {
    readonly();
  }

  public void addRecipients(Message.RecipientType type, Address addresses[])
    throws MessagingException
  {
    readonly();
  }

  public void setFrom() throws MessagingException {
    readonly();
  }

  public void setFrom(Address address) throws MessagingException {
    readonly();
  }

  public void setSentDate(Date d) throws MessagingException {
    readonly();
  }

  public String getContentType() {
    return "message/rfc822";
  }

  public String getDisposition() {
    return null;
  }

  public String getDescription() {
    return null;
  }

  public String getFileName() {
    return null;
  }

  public void setDisposition(String d) throws MessagingException {
    readonly();
  }

  public void setDescription(String d) throws MessagingException {
    readonly();
  }

  public void setFileName(String f) throws MessagingException {
    readonly();
  }

  public void setDataHandler(DataHandler d) {
    Assert.NotYetImplemented("Can't set a datahandler on our readonly messages!");
  }

  public void setText(String s) throws MessagingException {
    readonly();
  }

  public void setContent(String s) throws MessagingException {
    readonly();
  }

  public void setContent(Object o, String s) {
    Assert.NotYetImplemented("Can't set content on our readonly messages!");
  }

  public void setContent(Multipart m) {
    Assert.NotYetImplemented("Can't set content on our readonly messages!");
  }

  public void addHeader(String s1, String s2) throws MessagingException {
    readonly();
  }

  public void setHeader(String s1, String s2) throws MessagingException {
    readonly();
  }

  public void removeHeader(String s) throws MessagingException {
    readonly();
  }


  /** Get the InternetHeaders object. */
  abstract protected InternetHeaders getHeadersObj() throws MessagingException;

  public String[] getHeader(String name) throws MessagingException {
    return getHeadersObj().getHeader(name);
  }

  protected String getOneHeader(String name) throws MessagingException {
    String[] list = getHeader(name);
    if (list == null || list.length < 1) return null;
    return list[0];
  }

  public Address[] getRecipients(Message.RecipientType type)
    throws MessagingException {
    String[] list;
    if (type.equals(Message.RecipientType.TO)) {
      list = getHeader("To");
    } else if (type.equals(Message.RecipientType.CC)) {
      list = getHeader("Cc");
    } else if (type.equals(Message.RecipientType.BCC)) {
      list = getHeader("Bcc");
    } else {
      throw new IllegalArgumentException("Bad type " + type);
    }
    if (list == null || list.length == 0) {
      return new Address[0];
    }
    if (list.length == 1) {
      return InternetAddress.parse(list[0]);
    }
    Vector result = new Vector();
    for (int i=0 ; i<list.length ; i++) {
      Address[] addrs = InternetAddress.parse(list[i]);
      if (addrs != null) {
        for (int j=0 ; j<addrs.length ; j++) {
          result.addElement(addrs[j]);
        }
      }
    }
    Address[] addrs = new Address[result.size()];
    result.copyInto(addrs);
    return addrs;
  }


  public Address[] getFrom() throws MessagingException {
    // ### Much more needs to be done here.
    String str = getOneHeader("From");
    if (str != null) {
      return InternetAddress.parse(str);
    }
    return null;
  }

  public Date getSentDate() throws MessagingException {
    String str = getOneHeader("Date");
    if (str == null) return null;
    return NetworkDate.parseDate(new ByteBuf(str));
  }

  public long getSentDateAsLong() {
    try {
      return getSentDate().getTime();
    } catch (MessagingException e) {
      return 0;
    }
  }

  public String getSubject() throws MessagingException {
    return getOneHeader("Subject");
  }

  public Enumeration getAllHeaders() {
    Assert.NotYetImplemented("MessageBase.getAllHeaders -- don't know what it's really supposed to do!");
    return null;
  }

  public Enumeration getMatchingHeaders(String s[]) {
    Assert.NotYetImplemented("MessageBase.getMatchingHeaders -- don't know what it's really supposed to do!");
    return null;
  }

  public Enumeration getNonMatchingHeaders(String s[]) {
    Assert.NotYetImplemented("MessageBase.getNonMatchingHeaders -- don't know what it's really supposed to do!");
    return null;
  }


  public Message reply(boolean replyToAll) {
    Assert.NotYetImplemented("MessageReadOnly.reply");
    return null;
  }

  public boolean isMimeType(String mimeType) {
    Assert.NotYetImplemented("MessageReadOnly.isMimeType");
    return false;
  }

  /** This default implementation of getInputStream() works in terms of
    getInputStreamWithHeaders.  Subclasses are encouraged to define this
    more directly if they have an easy way of doing so. */
  public InputStream getInputStream() throws MessagingException {
    InputStream in = getInputStreamWithHeaders();
    if (in == null) return null;
    // Now read until we hit a blank line.
    int b1 = 0;
    int b2 = 0;
    try {
      while ((b2 = in.read()) >= 0) {
        if (b1 == '\r') {
          if (b2 == '\r') {
            // Two CR's in a row; done.
            break;
          }
        } else if (b1 == '\n') {
          if (b2 == '\n') {
            // Two LF's in a row; done.
            break;
          } else if (b2 == '\r') {
            // We have a LFCR.  It's gotta be the middle of a CRLFCRLF.  So,
            // we need to read one more character and then we're done.
            b1 = b2;
            b2 = in.read();
            if (b2 == '\n') break;
          }
        }
        b1 = b2;
      }
    } catch (IOException e) {
      throw new MessagingException("Can't skip headers in stream", e);
    }
    return in;
  }

  public InputStream getInputStreamWithHeaders() throws MessagingException {
    throw new MethodNotSupportedException("MessageBase.getInputStreamWithHeaders");

  }
}
