/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://wicg.github.io/compression/#compression-stream
 */

enum CompressionFormat {
  "deflate",
  "deflate-raw",
  "gzip",
};

[Exposed=*, Pref="dom.compression_streams.enabled"]
interface CompressionStream {
  [Throws]
  constructor(CompressionFormat format);
};
CompressionStream includes GenericTransformStream;
