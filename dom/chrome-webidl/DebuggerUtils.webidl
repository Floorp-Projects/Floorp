/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Interfaces and structures for use by the debugger and other devtools.

// Data sent to the devtools when new data has been received in an HTML parse.
dictionary HTMLContent {
  DOMString parserID;
  DOMString uri;
  DOMString contents;
  boolean complete;
};
