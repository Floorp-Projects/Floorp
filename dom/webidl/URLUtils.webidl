/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://url.spec.whatwg.org/#urlutils
 *
 * To the extent possible under law, the editors have waived all copyright
 * and related or neighboring rights to this work. In addition, as of 17
 * February 2013, the editors have made this specification available under
 * the Open Web Foundation Agreement Version 1.0, which is available at
 * http://www.openwebfoundation.org/legal/the-owf-1-0-agreements/owfa-1-0.
 */

[NoInterfaceObject,
 Exposed=(Window, Worker)]
interface URLUtils {
  // Bug 824857: no support for stringifier attributes yet.
  //  stringifier attribute ScalarValueString href;
  [Throws, CrossOriginWritable=Location]
           attribute ScalarValueString href;
  [Throws]
  readonly attribute ScalarValueString origin;

  [Throws]
           attribute ScalarValueString protocol;
  [Throws]
           attribute ScalarValueString username;
  [Throws]
           attribute ScalarValueString password;
  [Throws]
           attribute ScalarValueString host;
  [Throws]
           attribute ScalarValueString hostname;
  [Throws]
           attribute ScalarValueString port;
  [Throws]
           attribute ScalarValueString pathname;
  [Throws]
           attribute ScalarValueString search;

   // Bug 1082734 - attribute URLSearchParams searchParams;

  [Throws]
           attribute ScalarValueString hash;

  // Bug 824857 should remove this.
  [Throws]
  stringifier;
};
