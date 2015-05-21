/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

/*
 * Used by principals and the script security manager to represent origin
 * attributes.
 *
 * IMPORTANT: If you add any members here, you need to update the
 * methods on mozilla::OriginAttributes, and bump the CIDs of all
 * the principal implementations that use OriginAttributes in their
 * nsISerializable implementations.
 */
dictionary OriginAttributesDictionary {
  unsigned long appId = 0;
  boolean inBrowser = false;
};
