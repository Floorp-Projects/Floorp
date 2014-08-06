/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CaretAssociationHint_h___
#define CaretAssociationHint_h___

namespace mozilla {

/**
 * Hint whether a caret is associated with the content before a
 * given character offset (ASSOCIATE_BEFORE), or with the content after a given
 * character offset (ASSOCIATE_AFTER).
 */
enum CaretAssociationHint {
  CARET_ASSOCIATE_BEFORE,
  CARET_ASSOCIATE_AFTER
};

}

#endif
