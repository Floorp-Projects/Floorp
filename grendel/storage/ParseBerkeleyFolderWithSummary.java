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
 * Created: Jamie Zawinski <jwz@netscape.com>, 27 Sep 1997.
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.ByteBuf;

import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.io.FileNotFoundException;


class ParseBerkeleyFolderWithSummary extends ParseBerkeleyFolder {

  synchronized void mapOverMessages(RandomAccessFile infile, BerkeleyFolder f)
    throws IOException {
    fFolder = f;

    long file_date = f.fFile.lastModified();
    long file_size = f.fFile.length();

    total_message_count = 0;            // super.recordOne() updates these.
    undeleted_message_count = 0;
    unread_message_count = 0;
    deleted_message_bytes = 0;

    long summarized_length = readSummaryFile(f);

    if (summarized_length != file_size) {
      mapOverMessages(infile, f, summarized_length);

      if (summarized_length == 0)
        System.err.println("summary file was missing or out of sync; " +
                           "marking folder \"" + f.getName() + "\" dirty.");
      else
        System.err.println("more messages in folder than summary file; " +
                           "marking folder \"" + f.getName() + "\" dirty.");

      if (f.mailSummaryFile != null) {
        // We actually read a summary file: since we also read some messages,
        // update the dates in the summary file *now*, so that when this
        // summary object is *later* used to rewrite the summary file (and
        // include the new messages) the date/size that get written into the
        // new summary file will correspond to the time we actually parsed
        // the folder (as opposed to the time at which we wrote the summary.)
        f.mailSummaryFile.setFolderDateAndSize(file_date, file_size);

        // Also update the message counts, in case we parsed some more messages
        // beyond what the summary file told us about.
        f.mailSummaryFile.setFolderMessageCounts(total_message_count,
                                                 undeleted_message_count,
                                                 unread_message_count,
                                                 deleted_message_bytes);
      }
      f.setSummaryDirty(true);
    }

    // If something went wrong while parsing the summary file, it might want
    // to tweak some of the messages that were actually parsed from the file,
    // based on whatever attrocities it saw in the (broken or out of date)
    // summary, before parsing failed for whatever reason.
    if (f.mailSummaryFile != null)
      f.mailSummaryFile.salvageMessages();
  }



  /** Reads opens the summary file, if any, associated with the folder,
      and if it is in the proper format and otherwise up to date,
      parses it (by calling MailSummaryFile.readSummaryFile().)
      Stores a MailSummaryFile into the folder.  Returns the number of
      bytes which the summary file had summarized.

      @see MailSummaryFileFactory
      @see MailSummaryFile
      @see MailSummaryFileCheddar
      @see MailSummaryFileGrendel
   */
  protected synchronized long readSummaryFile(BerkeleyFolder f) {
    long result = 0;
    InputStream sum = null;
    f.mailSummaryFile = null;

    try {  // make sure sum is closed.
      sum = openSummaryFile(f);

      if (sum != null) {
        try {  // catch IOException
          MailSummaryFile reader =
            MailSummaryFileFactory.ParseFileHeader(f, sum);

          if (reader != null) {
            result = reader.readSummaryFile(sum);
            f.mailSummaryFile = reader;

            total_message_count = reader.totalMessageCount();
            undeleted_message_count = reader.undeletedMessageCount();
            unread_message_count = reader.unreadMessageCount();
            deleted_message_bytes = reader.deletedMessageBytes();
          }
        } catch (IOException e) {
//          System.err.println("exception while reading summary file: " + e);
        }
      }

    } finally {
      if (sum != null) {
        try { sum.close(); }
        catch (IOException e) { }
      }
    }

    return result;
  }


  protected InputStream openSummaryFile(BerkeleyFolder folder) {
    try {
      File sum = folder.summaryFileName();
      InputStream result = new BufferedInputStream(new FileInputStream(sum));
System.err.println("opened summary file " + sum);
      return result;
    } catch (SecurityException e) {
      return null;
    } catch (FileNotFoundException e) {
      return null;
    }
  }

}
