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
 * Created: Will Scullin <scullin@netscape.com>, 24 Oct 1997.
 */

package grendel.widgets;

import java.util.EventObject;

/**
 * Class for passing status messages to listeners
 */

public class StatusEvent extends EventObject {
  String fStatus;

  /**
   * Creates a status event with the given status. The string
   * is not copied for efficiency, use
   * StatusEvent(Object aSource, String aStatus , boolean aCopy)
   * for a constructor that copies the string.
   */

  public StatusEvent(Object aSource, String aStatus) {
    super(aSource);
    fStatus = aStatus;
  }

  /**
   * Creates a status event with the given string. If aCopy is
   * true, a copy of the status string is stored.
   */

  public StatusEvent(Object aSource, String aStatus, boolean aCopy) {
    super(aSource);

    if (aCopy) {
      fStatus = aStatus.toString();
    } else {
      fStatus = aStatus;
    }
  }

  /**
   * @return The status string for this event.
   */

  public final String getStatus() {
    return fStatus;
  }
}
