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
 * Created: Terry Weissman <terry@netscape.com>, 28 Aug 1997.
 */

package grendel.storage;

import java.io.IOException;

/** Interface to a maildrop: a place that we get new mail from. */

public interface MailDrop {
  /** Returned by getBiffState() if there are new messages waiting. */
  public static final int NEW = 1;

  /** Returned by getBiffState() if there are no new messages waiting. */
  public static final int NONE = 2;

  /** Returned by getBiffState() if we can't tell if there are new messages
      waiting. */
  public static final int UNKNOWN = 3;

  /** Actually go to maildrop, grab any messages, and stuff them into
      folders. */
  public void fetchNewMail() throws IOException;

  /** Go to the maildrop and update the info on how many messages out there
      are waiting. */
  public void doBiff() throws IOException;

  /** Find out whether we actually know anything about new messages waiting
      on the maildrop. */
  public int getBiffState();

  /** Return how many new messages are out there waiting.  If
      getBiffState() returns NONE, returns zero.  If getBiffState() returns
      UNKNOWN, or if we just can't tell with this maildrop, returns -1. */
  public int getNumMessagesWaiting();
};
