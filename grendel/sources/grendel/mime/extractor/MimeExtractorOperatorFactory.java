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
 * Created: Jamie Zawinski <jwz@netscape.com>,  2 Sep 1997.
 */

package grendel.mime.extractor;

import calypso.util.ByteBuf;
import grendel.mime.IMimeOperator;
import grendel.mime.IMimeObject;


/** Creates an IMimeOperator to extract the raw data of a MIME sub-part.
    The ID is of the form used by IMimeObject.partID() -- it is a string
    listing the index of the part in the MIME tree, like "1.2.5".
  */
public class MimeExtractorOperatorFactory {
  public static IMimeOperator Make(IMimeObject root_object, String target_id) {
    return new MimeExtractorOperator(root_object, target_id);
  }
}


/** This class extracts a particular MIME part, by number.
 */
class MimeExtractorOperator implements IMimeOperator {

  String target_id = null;
  boolean this_one = false;
  boolean done = false;

  MimeExtractorOperator(IMimeObject object, String target_id) {
    this.target_id = target_id;
    if (!done) {
      String id = object.partID();
      if (id == target_id ||
          (id != null && target_id != null &&
           id.equalsIgnoreCase(target_id)))
        this_one = true;
    }
  }

  public IMimeOperator createChild(IMimeObject child_object) {
    return new MimeExtractorOperator(child_object, target_id);
  }

  public void pushBytes(ByteBuf b) {
    if (this_one) {
      System.out.print(b.toString());
    }
  }

  public void pushEOF() {
    if (this_one)
      done = true;
  }
}
