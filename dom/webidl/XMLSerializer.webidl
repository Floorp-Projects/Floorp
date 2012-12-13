/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://domparsing.spec.whatwg.org/#the-xmlserializer-interface
 */

interface OutputStream;

[Constructor]
interface XMLSerializer {
  [Throws]
  DOMString serializeToString(Node root);

  // Mozilla-specific stuff
  [Throws, ChromeOnly]
  void serializeToStream(Node root, OutputStream stream, DOMString? charset);
};

