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
 * Created: Terry Weissman <terry@netscape.com>, 25 Aug 1997.
 */

package grendel.view;

import javax.mail.Message;

/** This is a message in a MessageSetView.  It represents a message, and also
    knows where it is in relationship with other messages in the same view. */

public interface ViewedMessage {
  /** Gets the view that this message is a part of. */
  public MessageSetView getView();

  /** Gets the message itself.  You need to go through this to find out
      stuff about the message (like its subject, author, etc.). */
  public Message getMessage();

  /** Returns the parent of this message.  (This is always null unless
      threading is turned on in the view.) */
  public ViewedMessage getParent();

  /** Returns the first child of this message.  (This is always null unless
      threading is turned on in the view.) */
  public ViewedMessage getChild();

  /** Returns the next message.  This is the next message that has the same
      parent as this message.*/
  public ViewedMessage getNext();

  /** This should return true of dummy messages, false otherwise.
      It is legal to pass dummy messages in with the list returned by
      elements(); the isDummy() method is the mechanism by which they are
      noted and ignored.
   */
  boolean isDummy();

  /** Debugging hack.  Dumps this message (and messages in its tree) out
      to System.out. */
  public void dump();
}
