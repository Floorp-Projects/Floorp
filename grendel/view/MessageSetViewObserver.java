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
 * Created: Terry Weissman <terry@netscape.com>, 17 Sep 1997.
 */

package grendel.view;

import java.util.Enumeration;

/** Anyone who wants to know when messages are added, removed, or tweaked in
 * a MessageSetView can implement this and add themselves to that
 * MessageSetView's list of observers.
 */

public interface MessageSetViewObserver {
  /** Some messages changed.  Each enumeration parameter might be null, or
    * it might be an enumeration of ViewedMessage objects.
    * @param inserted           new messages that have appeared
    * @param deleted            old messages that are no longer considered
    *                           part of the MessageSetView
    * @param changed            Messages that have had their internals tweaked
    *                           in some way
    */

  public void messagesChanged(Enumeration inserted,
                              Enumeration deleted,
                              Enumeration changed);
};
