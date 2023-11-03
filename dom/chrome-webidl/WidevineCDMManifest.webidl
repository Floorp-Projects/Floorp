/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[GenerateInitFromJSON]
dictionary WidevineCDMManifest {
  required DOMString name;
  required DOMString version;

  DOMString description;
  DOMString x-cdm-module-versions;
  DOMString x-cdm-interface-versions;
  DOMString x-cdm-host-versions;
  DOMString x-cdm-codecs;

  unsigned long manifest_version;
  sequence<DOMString> accept_arch;
};
