/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use display_list::AuxiliaryListsBuilder;
use {FilterOp, MixBlendMode, ScrollPolicy, StackingContext};
use {LayoutTransform, LayoutRect, PropertyBinding};

impl StackingContext {
    pub fn new(scroll_policy: ScrollPolicy,
               bounds: LayoutRect,
               z_index: i32,
               transform: PropertyBinding<LayoutTransform>,
               perspective: LayoutTransform,
               mix_blend_mode: MixBlendMode,
               filters: Vec<FilterOp>,
               auxiliary_lists_builder: &mut AuxiliaryListsBuilder)
               -> StackingContext {
        StackingContext {
            scroll_policy: scroll_policy,
            bounds: bounds,
            z_index: z_index,
            transform: transform,
            perspective: perspective,
            mix_blend_mode: mix_blend_mode,
            filters: auxiliary_lists_builder.add_filters(&filters),
        }
    }
}
