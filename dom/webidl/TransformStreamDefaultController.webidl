/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#ts-default-controller-class-definition
 */

[Exposed=*,
 Pref="dom.streams.transform_streams.enabled"]
interface TransformStreamDefaultController {
  readonly attribute unrestricted double? desiredSize;
  [Throws] void enqueue(optional any chunk);
  [Throws] void error(optional any reason);
  [Throws] void terminate();
};
