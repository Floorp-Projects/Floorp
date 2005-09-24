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
 * Created: Jamie Zawinski <jwz@netscape.com>,  1 Dec 1997.
 * Kieran Maclean
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.ByteBuf;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Enumeration;
import javax.mail.Folder;
import javax.mail.Header;
import javax.mail.MessagingException;
import javax.mail.internet.InternetHeaders;

class NewsMessage extends MessageBase
{

  int article_number = -1;

  int byte_length = -1;

  int line_length = -1;

  NewsMessage(NewsFolder f, InternetHeaders h) {
    super(f, h);
  }

  NewsMessage(NewsFolder f,
              long date,
              long flags,
              ByteBuf author,
              ByteBuf recipient,
              ByteBuf subj,
              ByteBuf id,
              ByteBuf refs[]) {
    super(f, date, flags, author, recipient, subj, id, refs);
  }

  NewsMessage(NewsFolder f,
              long date,
              long flags,
              ByteBuf author,
              ByteBuf recipient,
              ByteBuf subj,
              MessageID id,
              MessageID refs[]) {
    super(f, date, flags, author, recipient, subj, id, refs);
  }

  public String getContentType() throws MessagingException {
    String[] content_types = getHeader("Content-Type");
    if ((content_types == null) || (content_types.length == 0)) {
      return getDataHandler().getContentType();
    }
    for (int i = content_types.length - 1; i > -1; i++) {
      if (content_types[i].contains("/")) {
        return content_types[i];
      }
    }
    return content_types[0];
  }

  void setSize(int l) {
    byte_length = l;
  }

  public int getSize() {
    return byte_length;
  }

  void setLines(int l) {
    line_length = l;
  }

  public int getLineCount() {
    return line_length;
  }

  void setStorageFolderIndex(int p) {
    article_number = p;
  }

  int getStorageFolderIndex() {
    return article_number;
  }

  public InputStream getInputStreamWithHeaders() throws MessagingException {
    NewsFolder f = (NewsFolder) getFolder();
    try {
      return f.getMessageStream(this, true);
    } catch (IOException e) {
      throw new MessagingException("I/O Error", e);
    }
  }

  public InputStream getInputStream() throws MessagingException {
    NewsFolder f = (NewsFolder) getFolder();
    try {
      return f.getMessageStream(this, false);
    } catch (IOException e) {
      throw new MessagingException("I/O Error", e);
    }
  }

  protected void setFlagBit(long flag, boolean value) {
    long oflags = flags;
    super.setFlagBit(flag, value);
    if (oflags != flags) {
      setFlagsDirty(true);
      Folder f = getFolder();
      ((NewsFolder) f).setFlagsDirty(true, this, oflags);
    }
  }

  public void writeTo(OutputStream aStream) {
    Assert.NotYetImplemented("NewsFolder.writeTo(IOStream)");
  }
}
