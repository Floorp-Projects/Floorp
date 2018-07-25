/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.6.1 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --lockfile Cargo.lock --crate style -o layout/style/ServoStyleConsts.h`
 */

#include <cstdint>
#include <cstdlib>

namespace mozilla {

// Defines an element’s display type, which consists of
// the two basic qualities of how an element generates boxes
// <https://drafts.csswg.org/css-display/#propdef-display>
//
//
// NOTE(emilio): Order is important in Gecko!
//
// If you change it, make sure to take a look at the
// FrameConstructionDataByDisplay stuff (both the XUL and non-XUL version), and
// ensure it's still correct!
//
// Also, when you change this from Gecko you may need to regenerate the
// C++-side bindings (see components/style/cbindgen.toml).
enum class StyleDisplay : uint8_t {
  None = 0,
  Block,
  FlowRoot,
  Inline,
  InlineBlock,
  ListItem,
  Table,
  InlineTable,
  TableRowGroup,
  TableColumn,
  TableColumnGroup,
  TableHeaderGroup,
  TableFooterGroup,
  TableRow,
  TableCell,
  TableCaption,
  Flex,
  InlineFlex,
  Grid,
  InlineGrid,
  Ruby,
  RubyBase,
  RubyBaseContainer,
  RubyText,
  RubyTextContainer,
  Contents,
  WebkitBox,
  WebkitInlineBox,
  MozBox,
  MozInlineBox,
  MozGrid,
  MozInlineGrid,
  MozGridGroup,
  MozGridLine,
  MozStack,
  MozInlineStack,
  MozDeck,
  MozGroupbox,
  MozPopup,
};

} // namespace mozilla
