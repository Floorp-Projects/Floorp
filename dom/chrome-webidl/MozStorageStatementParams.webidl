/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface MozStorageStatementParams
{
  readonly attribute unsigned long length;

  [Throws]
  getter any(unsigned long index);

  [Throws]
  getter any(DOMString name);

  [Throws]
  setter void(unsigned long index, any arg);

  [Throws]
  setter void(DOMString name, any arg);
};
