/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FromParser_h
#define mozilla_dom_FromParser_h

namespace mozilla {
namespace dom {

/**
 * Constants for passing as aFromParser
 */
enum FromParser {
  NOT_FROM_PARSER = 0,
  FROM_PARSER_NETWORK = 1,
  FROM_PARSER_DOCUMENT_WRITE = 1 << 1,
  FROM_PARSER_FRAGMENT = 1 << 2,
  FROM_PARSER_XSLT = 1 << 3
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FromParser_h
