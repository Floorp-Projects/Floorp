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
 * Created: Jamie Zawinski <jwz@netscape.com>,  2 Oct 1997.
 */

package grendel.storage;

import java.io.InputStream;
import java.io.IOException;

class MailSummaryFileFactory {

  static final byte SUMMARY_MAGIC_NUMBER[] =
    "# Netscape folder cache\r\n".getBytes();
  static final int NEOACCESS_MAGIC_NUMBER[] =
    { 0000, 0036, 0204, 0212 };

  static final int VERSION_CHEDDAR = 4;
  static final int VERSION_DOGBERT = 5;
  static final int VERSION_GRENDEL = 6;


  private MailSummaryFileFactory() {}  // all methods are static

  /** Given a folder and a file stream to that folder's summary file,
      this static method parses the header of that file, and returns
      an appropriate MailSummaryFile object (or null if the file is
      not one we recognise.)

      <P> To parse the body of the summary file, use the readSummaryFile()
      on the returned MailSummaryFile object.

      @see MailSummaryFile
      @see MailSummaryFileCheddar
      @see MailSummaryFileGrendel
   */
  static MailSummaryFile ParseFileHeader(BerkeleyFolder folder,
                                         InputStream summary_stream) {
    MailSummaryFile result = null;

    LOST: {
      try {
        int b0 = summary_stream.read(); if (b0 == -1) break LOST;
        int b1 = summary_stream.read(); if (b1 == -1) break LOST;
        int b2 = summary_stream.read(); if (b2 == -1) break LOST;
        int b3 = summary_stream.read(); if (b3 == -1) break LOST;

        long version;
        if (b0 == NEOACCESS_MAGIC_NUMBER[0] &&
            b1 == NEOACCESS_MAGIC_NUMBER[1] &&
            b2 == NEOACCESS_MAGIC_NUMBER[2] &&
            b3 == NEOACCESS_MAGIC_NUMBER[3])
          version = VERSION_DOGBERT;
        else {
          if (b0 != SUMMARY_MAGIC_NUMBER[0] ||
              b1 != SUMMARY_MAGIC_NUMBER[1] ||
              b2 != SUMMARY_MAGIC_NUMBER[2] ||
              b3 != SUMMARY_MAGIC_NUMBER[3])
            return null;

          for (int i = 4; i < SUMMARY_MAGIC_NUMBER.length; i++)
            if (summary_stream.read() != SUMMARY_MAGIC_NUMBER[i])
              return null;

          b0 = summary_stream.read(); if (b0 == -1) break LOST;
          b1 = summary_stream.read(); if (b1 == -1) break LOST;
          b2 = summary_stream.read(); if (b2 == -1) break LOST;
          b3 = summary_stream.read(); if (b3 == -1) break LOST;
          version = ((((long)b0) << 24) | (b1 << 16) | (b2 << 8) | b3);
        }

        if (version == VERSION_CHEDDAR)
          result = new MailSummaryFileCheddar(folder);
        else if (version == VERSION_DOGBERT)
          result = null;  // we don' know how to read no stinkink NeoAccess...
        else if (version == VERSION_GRENDEL)
          result = new MailSummaryFileGrendel(folder);

      } catch (IOException e) {
      }
    }

//    System.err.println("summary file reader: " +
//                       (result == null ? "null" :
//                        result.getClass().getName()));

    return result;
  }
}
