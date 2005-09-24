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
 * Kieran Maclean
 */


package grendel.storage;

import calypso.util.NetworkDate;

import calypso.util.Assert;
import calypso.util.ByteBuf;

import java.io.*;
import java.text.DateFormat;
import java.util.Date;
import java.util.Enumeration;
import java.util.NoSuchElementException;
import java.util.Vector;

import javax.activation.DataHandler;
import javax.activation.MimeType;
import javax.mail.Address;
import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.MethodNotSupportedException;
import javax.mail.Multipart;
import javax.mail.event.MessageChangedEvent;
import javax.mail.IllegalWriteException;
import javax.mail.Part;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.InternetHeaders;


public abstract class MessageBase extends MessageReadOnly implements MessageExtra {
  
  /* **********************************************************
     Class variables
   **********************************************************
   */
  
  
  /* Don't confuse these flags with the values used in X-Mozilla-Status
     headers: they occupy a different space, and range.  There is a
     difference between the in-memory format, and the storage format.
     This is partially because there are potentially in-memory flags
     that don't get saved to disk.
   
     Parsing of the X-Mozilla-Status header (and conversion to this form)
     happens in the BerkeleyMessage class.
   
     There can be up to 64 flag-bits (the in-memory flags.)
     There can only be up to 16 X-Mozilla-Status flags (the persistent flags.)
   */
  public static final long FLAG_READ              = 0x00000001;
  public static final long FLAG_REPLIED           = 0x00000002;
  public static final long FLAG_FORWARDED         = 0x00000004;
  public static final long FLAG_MARKED            = 0x00000008;
  public static final long FLAG_DELETED           = 0x00000010;
  public static final long FLAG_HAS_RE            = 0x00000020;  // Subject
  public static final long FLAG_SIGNED            = 0x00000040;  // S/MIME
  public static final long FLAG_ENCRYPTED         = 0x00000080;  // S/MIME
  public static final long FLAG_SMTP_AUTH         = 0x00000100;  // Gag
  public static final long FLAG_PARTIAL           = 0x00000200;  // POP3
  public static final long FLAG_QUEUED            = 0x00000400;  // Offline
  
  
  // The mapping between our internal flag bits and javamail's flag strings.
  // The editable entry defines whether this flag can be changed from the
  // outside.
  
  static class FlagMap {
    long flag;
    Flags.Flag builtin;
    String non_builtin;
    boolean editable;
    FlagMap(long f, String n, boolean e) {
      flag = f;
      non_builtin = n;
      editable = e;
    }
    FlagMap(long f, Flags.Flag n, boolean e) {
      flag = f;
      builtin = n;
      editable = e;
    }
  };
  static FlagMap[] FLAGDEFS = new FlagMap[11];
  static {
    int i = 0;
    FLAGDEFS[i++] = new FlagMap(FLAG_READ,       Flags.Flag.SEEN,     true);
    FLAGDEFS[i++] = new FlagMap(FLAG_REPLIED,    Flags.Flag.ANSWERED, true);
    FLAGDEFS[i++] = new FlagMap(FLAG_FORWARDED,  "Forwarded",         true);
    FLAGDEFS[i++] = new FlagMap(FLAG_MARKED,     "Marked",            true);
    FLAGDEFS[i++] = new FlagMap(FLAG_DELETED,    Flags.Flag.DELETED,  true);
    FLAGDEFS[i++] = new FlagMap(FLAG_HAS_RE,     "HasRe",             false);
    FLAGDEFS[i++] = new FlagMap(FLAG_SIGNED,     "Signed",            false);
    FLAGDEFS[i++] = new FlagMap(FLAG_ENCRYPTED,  "Encrypted",         false);
    FLAGDEFS[i++] = new FlagMap(FLAG_SMTP_AUTH,  "SmtpAuth",          false);
    FLAGDEFS[i++] = new FlagMap(FLAG_PARTIAL,    "Partial",           false);
    FLAGDEFS[i++] = new FlagMap(FLAG_QUEUED,     "Queued",            false);
    Assert.Assertion(i == FLAGDEFS.length);
  };
  
  // This bit, when set, means that some change has been made to the flags
  // which should be written out to the X-Mozilla-Status header in the
  // folder's disk file.  This is done so that we can lazily update the
  // file, rather than writing it every time the flags change.
  public static final long FLAG_DIRTY             = 0x00000800;
  
  
  /* Some string-constants in bytebuf form that we use for interrogating
     headers during parsing.
   */
  protected static final ByteBuf FROM             = new ByteBuf("from");
  protected static final ByteBuf TO               = new ByteBuf("to");
  protected static final ByteBuf CC               = new ByteBuf("cc");
  protected static final ByteBuf NEWSGROUPS       = new ByteBuf("newsgroups");
  protected static final ByteBuf DATE             = new ByteBuf("date");
  protected static final ByteBuf SUBJECT          = new ByteBuf("subject");
  protected static final ByteBuf MESSAGE_ID       = new ByteBuf("message-id");
  protected static final ByteBuf REFERENCES       = new ByteBuf("references");
  protected static final ByteBuf IN_REPLY_TO      = new ByteBuf("in-reply-to");
  
  /* For simplifiedDate() */
  private static Date       scratch_date          = new Date();
  private static DateFormat date_format           = null;
  
  
  /* ***********************************************************
     Instance variables -- add them sparingly, memory is scarce.
   ***********************************************************
   */
  long   flags;        // see `FLAG_READ', etc, above.
  long   sentDate;     // milliseconds since the Epoch.
  
  // These slots are ints but really represent ByteStrings: they are indexes
  // into a ByteStringTable.  It's quite likely that we could live with these
  // being of type `short' instead of `int'.  Should memory usage be a problem,
  // we should consider that.
  //
  int author_name;     // name (not address) of the From or Sender.
  int recipient_name;  // name of first To, or CC, or newsgroup.
  int subject;         // subject minus "Re:" (see `FLAG_HAS_RE').
  
  // These slots are as above, but represent MessageID objects instead of
  // ByteString objects.  These also could stand to be of type `short'.
  //
  int message_id;      // will never be -1 (meaning null).
  int references[];    // may be null; else length > 0.
  
  
  /* **********************************************************
     Methods
   **********************************************************
   */
  
  MessageBase(FolderBase f) {
    super();
    this.folder = f;
  }
  
  // New constructor copied from above but sets msgnum correctly.
  MessageBase(FolderBase f, int num) {
    super(f, num);
    this.folder = f;
  }
  
  MessageBase(FolderBase f, InternetHeaders h) {
    this(f);
    initialize(f, h);
  }
  
  // New constructor copied from above but sets msgnum correctly.
  MessageBase(FolderBase f, int num, InternetHeaders h) {
    this(f, num);
    initialize(f, h);
  }
  
  MessageBase(FolderBase f,
      long date,
      long flags,
      ByteBuf author,
      ByteBuf recipient,
      ByteBuf subj,
      ByteBuf id,
      ByteBuf refs[]) {
    this(f);
    ByteStringTable string_table = f.getStringTable();
    MessageIDTable id_table = f.getMessageIDTable();
    
    if (id == null || id.length() == 0) {
      // #### In previous versions, we did this by getting the MD5 hash
      // #### of the whole header block.  We should do that here too...
      if (id == null) id = new ByteBuf();
      id.append(grendel.util.MessageIDGenerator.generate("missing-id"));
    }
    
    this.folder = f;
    this.flags = flags;
    this.sentDate = date;
    this.author_name = string_table.intern(author);
    this.recipient_name = string_table.intern(recipient);
    this.subject = string_table.intern(subj);
    this.message_id = id_table.intern(id);
    
    if (refs == null || refs.length == 0)
      this.references = null;
    else {
      int L = refs.length;
      references = new int[L];
      for (int i = 0; i < L; i++)
        references[i] = id_table.intern(refs[i]);
    }
  }
  
  // New constructor copied from above but sets msgnum correctly.
  MessageBase(FolderBase f,
      int num,
      long date,
      long flags,
      ByteBuf author,
      ByteBuf recipient,
      ByteBuf subj,
      ByteBuf id,
      ByteBuf refs[]) {
    this(f, num);
    ByteStringTable string_table = f.getStringTable();
    MessageIDTable id_table = f.getMessageIDTable();
    
    if (id == null || id.length() == 0) {
      // #### In previous versions, we did this by getting the MD5 hash
      // #### of the whole header block.  We should do that here too...
      if (id == null) id = new ByteBuf();
      id.append(grendel.util.MessageIDGenerator.generate("missing-id"));
    }
    
    this.folder = f;
    this.flags = flags;
    this.sentDate = date;
    this.author_name = string_table.intern(author);
    this.recipient_name = string_table.intern(recipient);
    this.subject = string_table.intern(subj);
    this.message_id = id_table.intern(id);
    
    if (refs == null || refs.length == 0)
      this.references = null;
    else {
      int L = refs.length;
      references = new int[L];
      for (int i = 0; i < L; i++)
        references[i] = id_table.intern(refs[i]);
    }
  }
  
  MessageBase(FolderBase f,
      long date,
      long flags,
      ByteBuf author,
      ByteBuf recipient,
      ByteBuf subj,
      MessageID id,
      MessageID refs[]) {
    this(f);
    ByteStringTable string_table = f.getStringTable();
    MessageIDTable id_table = f.getMessageIDTable();
    
    if (id != null) {
      this.message_id = id_table.intern(id);
    } else {
      // #### In previous versions, we did this by getting the MD5 hash
      // #### of the whole header block.  We should do that here too...
      ByteBuf b =
          new ByteBuf(grendel.util.MessageIDGenerator.generate("missing-id"));
      this.message_id = id_table.intern(b);
    }
    
    this.folder = f;
    this.flags = flags;
    this.sentDate = date;
    this.author_name = string_table.intern(author);
    this.recipient_name = string_table.intern(recipient);
    this.subject = string_table.intern(subj);
    
    if (refs == null || refs.length == 0)
      this.references = null;
    else {
      int L = refs.length;
      references = new int[L];
      for (int i = 0; i < L; i++)
        references[i] = id_table.intern(refs[i]);
    }
  }
  
  // New constructor copied from above but sets msgnum correctly.
  MessageBase(FolderBase f,
      int num,
      long date,
      long flags,
      ByteBuf author,
      ByteBuf recipient,
      ByteBuf subj,
      MessageID id,
      MessageID refs[]) {
    this(f, num);
    ByteStringTable string_table = f.getStringTable();
    MessageIDTable id_table = f.getMessageIDTable();
    
    if (id != null) {
      this.message_id = id_table.intern(id);
    } else {
      // #### In previous versions, we did this by getting the MD5 hash
      // #### of the whole header block.  We should do that here too...
      ByteBuf b =
          new ByteBuf(grendel.util.MessageIDGenerator.generate("missing-id"));
      this.message_id = id_table.intern(b);
    }
    
    this.folder = f;
    this.flags = flags;
    this.sentDate = date;
    this.author_name = string_table.intern(author);
    this.recipient_name = string_table.intern(recipient);
    this.subject = string_table.intern(subj);
    
    if (refs == null || refs.length == 0)
      this.references = null;
    else {
      int L = refs.length;
      references = new int[L];
      for (int i = 0; i < L; i++)
        references[i] = id_table.intern(refs[i]);
    }
  }
  
  protected void initialize(Folder f, InternetHeaders h) {
    folder = f;
    FolderBase fb = (FolderBase) f;
    ByteStringTable string_table = fb.getStringTable();
    MessageIDTable id_table = fb.getMessageIDTable();
    String hh[];
    
    hh = h.getHeader("From");
    author_name = (hh == null || hh.length == 0 ? -1 :
      string_table.intern(hh[0].trim()));
    
    // #### need an address parser here...
    recipient_name = -1;
/*
    hh = h.getHeader("To");
      // #### deal with multiple to fields
    recipient_name = (hh == null || hh.length == 0 ? -1 :
                     string_table.intern(hh[0].trim()));
 
    if (recipient == -1) {
      // #### deal with multiple cc fields
      hh = h.getHeader("CC");
      recipient_name = (hh == null || hh.length == 0 ? -1 :
                       string_table.intern(hh[0].trim()));
    }
 
    if (recipient == -1) {
      hh = h.getHeader("Newsgroups");
      recipient_name = (hh == null || hh.length == 0 ? -1 :
                       string_table.intern(hh[0].trim()));
    }
 */
    
    hh = h.getHeader("Subject");
    if (hh != null && hh.length != 0) {
      // Much of this code is duplicated in MessageExtraFactory.  Sigh. ###
      ByteBuf value = new ByteBuf(hh[0]);
      if (value.length() > 2 &&
          (value.byteAt(0) == 'r' || value.byteAt(0) == 'R') &&
          (value.byteAt(1) == 'e' || value.byteAt(1) == 'E')) {
        byte c = value.byteAt(2);
        if (c == ':') {
          value.remove(0, 3);   // Skip over "Re:"
          value.trim();         // Remove any whitespace after colon
          flags |= FLAG_HAS_RE; // yes, we found it.
        } else if (c == '[' || c == '(') {
          int i = 3;    // skip over "Re[" or "Re("
          
          // Skip forward over digits after the "[" or "(".
          int length = value.length();
          while (i < length &&
              value.byteAt(i) >= '0' &&
              value.byteAt(i) <= '9') {
            i++;
          }
          // Now ensure that the following thing is "]:" or "):"
          // Only if it is do we treat this all as a "Re"-ish thing.
          if (i < (length-1) &&
              (value.byteAt(i) == ']' ||
              value.byteAt(i) == ')') &&
              value.byteAt(i+1) == ':') {
            value.remove(0, i+2); // Skip the whole thing.
            value.trim();         // Remove any whitespace after colon
            flags |= FLAG_HAS_RE; // yes, we found it.
          }
        }
      }
      subject = string_table.intern(value);
    }
    
    hh = h.getHeader("Date");
    if (hh != null && hh.length != 0)
      sentDate = NetworkDate.parseLong(new ByteBuf(hh[0]), true);
    
    hh = h.getHeader("Message-ID");
    if (hh != null && hh.length != 0) {
      ByteBuf value = new ByteBuf(hh[0]);
      value.trim();
      int length = value.length();
      if (length > 0 &&
          value.byteAt(0) == '<' &&
          value.byteAt(length-1) == '>') {
        value.remove(length-1, length);
        value.remove(0, 1);
      }
      message_id = id_table.intern(value.trim());
    }
    
    // There must be a message ID on every message.
    if (message_id == -1) {
      // #### In previous versions, we did this by getting the MD5 hash
      // #### of the whole header block.  We should do that here too...
      String id = grendel.util.MessageIDGenerator.generate("missing-id");
      message_id = id_table.intern(new ByteBuf(id));
    }
    
    hh = h.getHeader("References");
    if (hh != null && hh.length != 0) {
      ByteBuf value = new ByteBuf(hh[0]);
      references = internReferences(id_table, value.trim());
    }
    
    // Only examine the In-Reply-To header if there is no References header.
    if (references == null) {
      
      hh = h.getHeader("In-Reply-To");
      if (hh != null && hh.length != 0) {
        ByteBuf value = new ByteBuf(hh[0]);
        references = internReferences(id_table, value.trim());
      }
    }
    base_headers = h;
  }
  
  
  // Ported from akbar's "msg_intern_references", mailsum.c.
  protected int[] internReferences(MessageIDTable id_table, ByteBuf refs) {
    byte data[] = refs.toBytes();
    int length = refs.length();
    int s;
    int n_refs = 0;
    for (s=0 ; s<length ; s++) {
      if (data[s] == '>') {
        n_refs++;
      }
    }
    
    if (n_refs == 0)
      return null;
    
    int result[] = new int[n_refs];
    int start = 0;
    int cur = 0;
    
    s = 0;
    while (s < length) {
      
      // The old way was to skip over whitespace, then skip an optional "<".
      // The new way is to skip over everything up to and including "<".
      // This lets us deal better with In-Reply-To headers, in addition to
      // References headers: we can cope with
      //
      //     In-Reply-To: NAME's message of TIME <ID@HOST>
      //     In-Reply-To: article <ID@HOST> of TIME
      //     In-Reply-To: <ID@HOST>, from NAME <USER@HOST>
      //
      // In the latter case, we're going to fuck up and think that both
      // of them are IDs, but headers like appear to be extremely rare.
      // In a survey of 22,950 mail messages with In-Reply-To headers:
      //
      //     18,396  had at least one occurence of <>-bracketed text.
      //      4,554  had no <>-bracketed text at all (just names and dates.)
      //        714  contained one <>-bracketed addr-spec and no message IDs.
      //          4  contained multiple message IDs.
      //          1  contained one message ID and one <>-bracketed addr-spec.
      //
      // The most common forms of In-Reply-To seem to be
      //
      //        31%  NAME's message of TIME <ID@HOST>
      //        22%  <ID@HOST>
      //         9%  <ID@HOST> from NAME at "TIME"
      //         8%  USER's message of TIME <ID@HOST>
      //         7%  USER's message of TIME
      //         6%  Your message of "TIME"
      //        17%  hundreds of other variants (average 0.4% each?)
      //
      // jwz, 17 Sep 1997.
      //
      while (start < length && data[start] != '<')
        start++;
      
      // skip over consecutive "<" -- I've seen "<<ID@HOST>>".
      while (start < length && data[start] == '<')
        start++;
      
      s = start;
      while (s < length && data[s] != '>')
        s++;
      
      if (s > start &&
          s < length &&
          data[s] == '>') {
        result[cur++] = id_table.intern(data, start, s - start);
        start = s + 1;
        
        // skip over consecutive ">" -- I've seen "<<ID@HOST>>".
        while (start < length && data[start] == '>')
          start++;
      } else {
        s++;
      }
    }
    
    if (cur != n_refs) {
      // Whoops!  Something's funny about this line, and the number of
      // ">" characters didn't equal the number of IDs we extracted.
      // This will be an extremely rare situation, so when it happens,
      // just make a new array.
      if (cur == 0)
        result = null;
      else {
        int r2[] = new int[cur];
        System.arraycopy(result, 0, r2, 0, cur);
        result = r2;
      }
    }
    
    return result;
  }
  
  
  
  public Object getMessageID() {
    MessageIDTable id_table = ((FolderBase)folder).getMessageIDTable();
    return (MessageID) id_table.getObject(message_id);
  }
  
  public String getSubject() {
    String result = simplifiedSubject();
    if (subjectIsReply()) result = "Re: "  + result;
    return result;
  }
  
  public String getAuthor() {
    ByteStringTable string_table = ((FolderBase)folder).getStringTable();
    ByteString a = (ByteString)
    string_table.getObject(author_name);
    if (a == null) return "";
    else return a.toString();
  }
  
  public String getRecipient() {
    ByteStringTable string_table = ((FolderBase)folder).getStringTable();
    ByteString r = (ByteString) string_table.getObject(recipient_name);
    if (r == null) return "";
    else return r.toString();
  }
  
  public Object[] messageThreadReferences() {
    if (references == null) return null;
    int count = references.length;
    if (count == 0) return null;
    
    // Note: this conses.
    MessageIDTable id_table = ((FolderBase)folder).getMessageIDTable();
    Object result[] = new Object[count];
    for (int i = 0; i < result.length; i++)
      result[i] = id_table.getObject(references[i]);
    
    return result;
  }
  
  public long getSentDateAsLong() {
    return sentDate;
  }
  
  public Date getSentDate() {
    return new Date(sentDate);
  }
  
  public Date getReceivedDate() {
    // ### We don't currently remember this info.  Should we?
    return getSentDate();
  }
  
  
  public Folder getFolder() {
    return folder;
  }
  
  
  // #### Warning, this is untested -- the "javax.mail.Flags" class changed
  // around a bunch since the last time we tried to use this code, and I had
  // to beat on this.
  
  public Flags getFlags() {
    Flags result = new Flags();
    for (int i=0 ; i<FLAGDEFS.length ; i++) {
      if ((flags & FLAGDEFS[i].flag) != 0) {
        if (FLAGDEFS[i].builtin != null) {
          result.add(FLAGDEFS[i].builtin);
        } else {
          result.add(FLAGDEFS[i].non_builtin);
        }
      }
    }
    return result;
  }
  
  public boolean isSet(Flags.Flag flag) {
    for (int i=0; i < FLAGDEFS.length ; i++) {
      if (flag.equals(FLAGDEFS[i].builtin)) {
        return (flags & FLAGDEFS[i].flag) != 0;
      }
    }
    return false;
  }
  
  public boolean isSet(String flag) {
    for (int i=0; i < FLAGDEFS.length ; i++) {
      if (flag.equals(FLAGDEFS[i].non_builtin)) {
        return (flags & FLAGDEFS[i].flag) != 0;
      }
    }
    return false;
  }
  
  public void setFlags(Flags flag, boolean set) throws MessagingException {
    
    Flags.Flag[]  builtin_flags = flag.getSystemFlags();
    String [] non_builtin_flags = flag.getUserFlags();
    
    NEXT_BUILTIN:
      for (int i = 0; i < builtin_flags.length ; i++) {
        Flags.Flag name = builtin_flags[i];
        for (int j=0 ; j<FLAGDEFS.length ; j++) {
          if (FLAGDEFS[j].editable && name.equals(FLAGDEFS[j].builtin)) {
            setFlagBit(FLAGDEFS[j].flag, set);
            continue NEXT_BUILTIN;
          }
        }
        throw new IllegalWriteException("Can't change flag " + name +
            " on message " + this);
      }
      
      NEXT_NON_BUILTIN:
        for (int i = 0; i < non_builtin_flags.length ; i++) {
          String name = non_builtin_flags[i];
          for (int j=0 ; j<FLAGDEFS.length ; j++) {
            if (FLAGDEFS[j].editable && name.equals(FLAGDEFS[j].non_builtin)) {
              setFlagBit(FLAGDEFS[j].flag, set);
              continue NEXT_NON_BUILTIN;
            }
          }
          throw new IllegalWriteException("Can't change flag " + name +
              " on message " + this);
        }
  }
  
  protected synchronized void setFlagBit(long flag, boolean value) {
    long newflags = flags;
    if (value) {
      newflags |= flag;
    } else {
      newflags &= ~flag;
    }
    if (flags != newflags) {
      flags = newflags;
      ((FolderBase)folder).doNotifyMessageChangedListeners
          (MessageChangedEvent.FLAGS_CHANGED, this);
    }
  }
  
  
  // #### are these still part of the JavaMail spec? I don't see them in
  // http://www.javasoft.com/products/javamail/javadocs/javax/mail/Message.html
  
  public boolean isRead() {
    return ((flags & FLAG_READ) != 0);
  }
  public void setIsRead(boolean value) {
    setFlagBit(FLAG_READ, value);
  }
  
  public boolean isReplied() {
    return ((flags & FLAG_REPLIED) != 0);
  }
  public void setReplied(boolean value) {
    setFlagBit(FLAG_REPLIED, value);
  }
  
  public boolean isForwarded() {
    return ((flags & FLAG_FORWARDED) != 0);
  }
  public void setForwarded(boolean value) {
    setFlagBit(FLAG_FORWARDED, value);
  }
  
  public boolean isFlagged() {
    return ((flags & FLAG_MARKED) != 0);
  }
  public void setFlagged(boolean value) {
    setFlagBit(FLAG_MARKED, value);
  }
  
  public boolean isDeleted() {
    return ((flags & FLAG_DELETED) != 0);
  }
  public void setDeleted(boolean value) {
    setFlagBit(FLAG_DELETED, value);
  }
  
  public boolean isSigned() {
    return ((flags & FLAG_SIGNED) != 0);
  }
  public boolean isEncrypted() {
    return ((flags & FLAG_ENCRYPTED) != 0);
  }
  
  
  public boolean flagsAreDirty() {
    return ((flags & FLAG_DIRTY) != 0);
  }
  public void setFlagsDirty(boolean value) {
    setFlagBit(FLAG_DIRTY, value);
  }
  
  public void saveChanges() {
    // ### Should this actually do something?
  }
  
  public InputStream getRawText() throws IOException {
    return null;
  }
  
  public String simplifiedSubject() {
    ByteStringTable string_table = ((FolderBase)folder).getStringTable();
    ByteString s = (ByteString) string_table.getObject(subject);
    if (s == null) return "";
    else return s.toString();  // #### fix me
  }
  
  public boolean subjectIsReply() {
    return ((flags & FLAG_HAS_RE) != 0);
  }
  
  public String simplifiedDate() {
    return SimplifyADate(sentDate);
  }
  
  public static String SimplifyADate(long ldate) {
    if (ldate <= 0) return "";
    
    synchronized(scratch_date) {
      if (date_format == null) {
        date_format = DateFormat.getDateTimeInstance();
      }
      scratch_date.setTime(ldate);
      
      /* #### This needs to be more clever.  I guess we should make our
         own subclass of DateFormat and put the cleverness there. */
      return date_format.format(scratch_date);
    }
  }
  
  private DataHandler data_handler = null;
  
  public DataHandler getDataHandler() throws MessagingException {
    if (data_handler==null) {
      String type = "text/plain";  //XXX lie to it...
      String[] content_types = getHeader("Content-Type");
      if ((content_types!=null)&&(content_types.length>0)) {
        type = content_types[0];
      }
      data_handler = new DataHandler(getContentAsString(),type);
    }
    return data_handler;
  }
  
  private String getContentAsString() {
    try {
      StringWriter writer =new StringWriter();
      Reader r=new InputStreamReader(getInputStream());
      int read = r.read();;
      while(read!=-1) {
        writer.write(read);
        read = r.read();
      }
      r.close();
      return writer.toString();
    } catch(Exception e) {e.printStackTrace(); return null;}
  }
  
  public Object getContent() throws IOException, MessagingException {
    return getDataHandler().getContent();
  }
  
  private InternetHeaders base_headers = null;
  private InternetHeaders full_headers = null;
  
  /** Get the InternetHeaders object.  Someday, maybe we'll keep this around
   * in a weak link or something.  But for now, we always recreate it.
   * Subclasses might want to redefine just this, or they might want to
   * redefine all the routines below that use this. */
  protected InternetHeaders getHeadersObj() throws MessagingException {
    if (full_headers==null) {
      try {
        InputStream is=getInputStreamWithHeaders();
        full_headers=new InternetHeaders(is);
        try { is.close(); } catch(Exception e) {e.printStackTrace();}
      } catch (IOException ioe) {
        if (session.getDebug()) {
          ioe.printStackTrace();
        }
        return base_headers;
      }
    }
    return full_headers;
  }
  
}
