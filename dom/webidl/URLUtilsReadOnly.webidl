/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://url.spec.whatwg.org/#urlutils
 *
 * To the extent possible under law, the editors have waived all copyright
 * and related or neighboring rights to this work. In addition, as of 21
 * May 2013, the editors have made this specification available under
 * the Open Web Foundation Agreement Version 1.0, which is available at
 * http://www.openwebfoundation.org/legal/the-owf-1-0-agreements/owfa-1-0.
 */

[NoInterfaceObject,
 Exposed=(Window, Worker)]
interface URLUtilsReadOnly {
  stringifier;
  readonly attribute ScalarValueString href;

  readonly attribute ScalarValueString protocol;
  readonly attribute ScalarValueString host;
  readonly attribute ScalarValueString hostname;
  readonly attribute ScalarValueString port;
  readonly attribute ScalarValueString pathname;
  readonly attribute ScalarValueString search;
  readonly attribute ScalarValueString hash;
  readonly attribute ScalarValueString origin;
};
