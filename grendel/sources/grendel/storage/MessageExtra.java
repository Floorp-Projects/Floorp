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
 * Created: Terry Weissman <terry@netscape.com>, 22 Oct 1997.
 * Kieran Maclean
 */

package grendel.storage;

import java.io.IOException;
import java.io.InputStream;
import javax.mail.MessagingException;

/** These are extra interfaces that our message objects implement that are not
  implemented as part of the standard Message class.   If you want to call
  any of these, and you just have a base Message object, use
  MessageExtraFactory to find or create the MessageExtra object for you. */

public interface MessageExtra {
  /** The name of the author of this message (not his email address). */
  public String getAuthor() throws MessagingException;

  /** The name of the recipient of this message (not his email address). */
  public String getRecipient() throws MessagingException;

  /** The subject, minus any "Re:" part. */
  public String simplifiedSubject() throws MessagingException;

  /** Whether the subject has a "Re:" part." */
  public boolean subjectIsReply() throws MessagingException;

  /** A short rendition of the sent date. */
  public String simplifiedDate() throws MessagingException;

  /** A unique object representing the message-id for this message. */
  public Object getMessageID() throws MessagingException;

  /** A list of the above messageid objects that this message has references
   to. */
  public Object[] messageThreadReferences() throws MessagingException;

  public boolean isRead() throws MessagingException;
  public void setIsRead(boolean value) throws MessagingException;
  public boolean isReplied() throws MessagingException;
  public void setReplied(boolean value) throws MessagingException;
  public boolean isForwarded() throws MessagingException;
  public void setForwarded(boolean value) throws MessagingException;
  public boolean isFlagged() throws MessagingException;
  public void setFlagged(boolean value) throws MessagingException;
  public boolean isDeleted() throws MessagingException;
  public void setDeleted(boolean value) throws MessagingException;

  /** Gets the input stream for the message, in RFC822 format: a bunch of
    headers, and then a blank line, and then the message itself. */
  public InputStream getInputStreamWithHeaders() throws MessagingException, IOException;

};

