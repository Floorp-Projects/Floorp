/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://url.spec.whatwg.org/#urlsearchparams
 *
 * To the extent possible under law, the editors have waived all copyright
 * and related or neighboring rights to this work. In addition, as of 17
 * February 2013, the editors have made this specification available under
 * the Open Web Foundation Agreement Version 1.0, which is available at
 * http://www.openwebfoundation.org/legal/the-owf-1-0-agreements/owfa-1-0.
 */

[Exposed=(Window,Worker,WorkerDebugger)]
interface URLSearchParams {
  [Throws]
  constructor(optional (sequence<sequence<UTF8String>> or
                        record<UTF8String, UTF8String> or UTF8String) init = "");

  readonly attribute unsigned long size;

  undefined append(UTF8String name, UTF8String value);
  undefined delete(UTF8String name, optional UTF8String value);
  UTF8String? get(UTF8String name);
  sequence<UTF8String> getAll(UTF8String name);
  boolean has(UTF8String name, optional UTF8String value);
  undefined set(UTF8String name, UTF8String value);

  [Throws]
  undefined sort();

  iterable<UTF8String, UTF8String>;
  stringifier;
};
