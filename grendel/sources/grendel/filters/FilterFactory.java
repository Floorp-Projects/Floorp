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
 * Class FilterFactory
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */
package grendel.filters;

import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.search.SearchTerm;

import grendel.filters.IFilter;
import grendel.filters.IFilterAction;
import grendel.filters.FilterBase;

public class FilterFactory extends Object {
  static public IFilter Make(String name, SearchTerm term,
                             IFilterAction action) {
        return new FilterBase(name, term, action);
  }
}
