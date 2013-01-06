/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dvcs.w3.org/hg/undomanager/raw-file/tip/undomanager.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface DOMTransaction;

[PrefControlled]
interface UndoManager {
  [Throws] void transact(DOMTransaction transaction, boolean merge);
  [Throws] void undo();
  [Throws] void redo();
  [Throws] sequence<DOMTransaction>? item(unsigned long index);
  [Throws] readonly attribute unsigned long length;
  [Throws] readonly attribute unsigned long position;
  [Throws] void clearUndo();
  [Throws] void clearRedo();
};

