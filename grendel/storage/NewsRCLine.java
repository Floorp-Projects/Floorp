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
 * Created: Jamie Zawinski <jwz@netscape.com>, 10 Sep 1997.
 */

package grendel.storage;

import calypso.util.ByteBuf;


/** This class represents a single line from a newsrc file:
    a group name, whether it is subscribed, and a set of message numbers.
  */
class NewsRCLine extends NewsSet {
  private NewsRC rc;
  private String group_name;
  private boolean subscribed;

  /** Creates a NewsRCLine with no messages marked as read. */
  NewsRCLine(NewsRC parent, String group, boolean subbed) {
    super();
    rc = parent;
    subscribed = subbed;
  }

  /** Creates a NewsRCLine with the indicated messages marked as read.
   */
  NewsRCLine(NewsRC parent, String group, boolean subbed, ByteBuf numbers) {
    this(parent, group, subbed, numbers.toBytes(), 0, numbers.length());
  }

  /** As above, but takes a byte array and a subsequence into it, instead
      of a ByteBuf object. */
  NewsRCLine(NewsRC parent, String name, boolean subbed,
             byte[] line, int start, int end) {
    super(line, start, end);
    rc = parent;
    group_name = name;
    subscribed = subbed;
  }

  /** Returns the name of this newsgroup.
   */
  String name() {
    return group_name;
  }

  /** Returns whether this newsgroup is subscribed.
   */
  boolean subscribed() {
    return subscribed;
  }

  /** Change whether this newsgroup is subscribed.
   */
  void setSubscribed(boolean subscribed) {
    if (this.subscribed != subscribed) {
      this.subscribed = subscribed;
      markDirty();
    }
  }

  /** Converts the object back into a printed representation for the file.
      This will be something like "alt.group: 1-29627,32861-32863".
    */
  public void write(ByteBuf out) {
    if (group_name == null)
      return;
    out.append(group_name);
    out.append(subscribed ? ":" : "!");
    if (!isEmpty()) out.append(" ");
    super.write(out);
  }

  /** Called when a change is made to the set.
      This just invokes the markDirty() method on the parent NewsRC object.
    */
  public void markDirty() {
    rc.markDirty();
  }
}
