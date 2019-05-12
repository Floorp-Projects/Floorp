/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bincode;
use euclid::SideOffsets2D;
#[cfg(feature = "deserialize")]
use serde::de::Deserializer;
#[cfg(feature = "serialize")]
use serde::ser::{Serializer, SerializeSeq};
use serde::{Deserialize, Serialize};
use std::io::{Read, stdout, Write};
use std::marker::PhantomData;
use std::ops::Range;
use std::{io, mem, ptr, slice};
use std::collections::HashMap;
use time::precise_time_ns;
// local imports
use crate::display_item as di;
use crate::api::{PipelineId, PropertyBinding};
use crate::gradient_builder::GradientBuilder;
use crate::color::ColorF;
use crate::font::{FontInstanceKey, GlyphInstance, GlyphOptions};
use crate::image::{ColorDepth, ImageKey};
use crate::units::*;


// We don't want to push a long text-run. If a text-run is too long, split it into several parts.
// This needs to be set to (renderer::MAX_VERTEX_TEXTURE_WIDTH - VECS_PER_TEXT_RUN) * 2
pub const MAX_TEXT_RUN_LENGTH: usize = 2040;

// See ROOT_REFERENCE_FRAME_SPATIAL_ID and ROOT_SCROLL_NODE_SPATIAL_ID
// TODO(mrobinson): It would be a good idea to eliminate the root scroll frame which is only
// used by Servo.
const FIRST_SPATIAL_NODE_INDEX: usize = 2;

// See ROOT_SCROLL_NODE_SPATIAL_ID
const FIRST_CLIP_NODE_INDEX: usize = 1;

#[repr(C)]
#[derive(Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange<T> {
    start: usize,
    length: usize,
    _boo: PhantomData<T>,
}

impl<T> Copy for ItemRange<T> {}
impl<T> Clone for ItemRange<T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T> Default for ItemRange<T> {
    fn default() -> Self {
        ItemRange {
            start: 0,
            length: 0,
            _boo: PhantomData,
        }
    }
}

impl<T> ItemRange<T> {
    pub fn is_empty(&self) -> bool {
        // Nothing more than space for a length (0).
        self.length <= mem::size_of::<u64>()
    }
}

#[derive(Copy, Clone)]
pub struct TempFilterData {
    pub func_types: ItemRange<di::ComponentTransferFuncType>,
    pub r_values: ItemRange<f32>,
    pub g_values: ItemRange<f32>,
    pub b_values: ItemRange<f32>,
    pub a_values: ItemRange<f32>,
}

/// A display list.
#[derive(Clone, Default)]
pub struct BuiltDisplayList {
    /// Serde encoded bytes. Mostly DisplayItems, but some mixed in slices.
    data: Vec<u8>,
    descriptor: BuiltDisplayListDescriptor,
}

/// Describes the memory layout of a display list.
///
/// A display list consists of some number of display list items, followed by a number of display
/// items.
#[repr(C)]
#[derive(Copy, Clone, Default, Deserialize, Serialize)]
pub struct BuiltDisplayListDescriptor {
    /// The first IPC time stamp: before any work has been done
    builder_start_time: u64,
    /// The second IPC time stamp: after serialization
    builder_finish_time: u64,
    /// The third IPC time stamp: just before sending
    send_start_time: u64,
    /// The amount of clipping nodes created while building this display list.
    total_clip_nodes: usize,
    /// The amount of spatial nodes created while building this display list.
    total_spatial_nodes: usize,
}

pub struct BuiltDisplayListIter<'a> {
    list: &'a BuiltDisplayList,
    data: &'a [u8],
    cur_item: di::DisplayItem,
    cur_stops: ItemRange<di::GradientStop>,
    cur_glyphs: ItemRange<GlyphInstance>,
    cur_filters: ItemRange<di::FilterOp>,
    cur_filter_data: Vec<TempFilterData>,
    cur_clip_chain_items: ItemRange<di::ClipId>,
    cur_complex_clip: (ItemRange<di::ComplexClipRegion>, usize),
    peeking: Peek,
    /// Should just be initialized but never populated in release builds
    debug_stats: DebugStats,
}

/// Internal info used for more detailed analysis of serialized display lists
#[allow(dead_code)]
struct DebugStats {
    /// Last address in the buffer we pointed to, for computing serialized sizes
    last_addr: usize,
    stats: HashMap<&'static str, ItemStats>,
}

/// Stats for an individual item
#[derive(Copy, Clone, Debug, Default)]
pub struct ItemStats {
    /// How many instances of this kind of item we deserialized
    pub total_count: usize,
    /// How many bytes we processed for this kind of item
    pub num_bytes: usize,
}

pub struct DisplayItemRef<'a: 'b, 'b> {
    iter: &'b BuiltDisplayListIter<'a>,
}

#[derive(PartialEq)]
enum Peek {
    StartPeeking,
    IsPeeking,
    NotPeeking,
}

#[derive(Clone)]
pub struct AuxIter<'a, T> {
    data: &'a [u8],
    size: usize,
    _boo: PhantomData<T>,
}

impl BuiltDisplayListDescriptor {}

impl BuiltDisplayList {
    pub fn from_data(data: Vec<u8>, descriptor: BuiltDisplayListDescriptor) -> Self {
        BuiltDisplayList { data, descriptor }
    }

    pub fn into_data(mut self) -> (Vec<u8>, BuiltDisplayListDescriptor) {
        self.descriptor.send_start_time = precise_time_ns();
        (self.data, self.descriptor)
    }

    pub fn data(&self) -> &[u8] {
        &self.data[..]
    }

    // Currently redundant with data, but may be useful if we add extra data to dl
    pub fn item_slice(&self) -> &[u8] {
        &self.data[..]
    }

    pub fn descriptor(&self) -> &BuiltDisplayListDescriptor {
        &self.descriptor
    }

    pub fn times(&self) -> (u64, u64, u64) {
        (
            self.descriptor.builder_start_time,
            self.descriptor.builder_finish_time,
            self.descriptor.send_start_time,
        )
    }

    pub fn total_clip_nodes(&self) -> usize {
        self.descriptor.total_clip_nodes
    }

    pub fn total_spatial_nodes(&self) -> usize {
        self.descriptor.total_spatial_nodes
    }

    pub fn iter(&self) -> BuiltDisplayListIter {
        BuiltDisplayListIter::new(self)
    }

    pub fn get<'de, T: Deserialize<'de>>(&self, range: ItemRange<T>) -> AuxIter<T> {
        AuxIter::new(&self.data[range.start .. range.start + range.length])
    }
}

/// Returns the byte-range the slice occupied, and the number of elements
/// in the slice.
fn skip_slice<T: for<'de> Deserialize<'de>>(
    list: &BuiltDisplayList,
    mut data: &mut &[u8],
) -> (ItemRange<T>, usize) {
    let base = list.data.as_ptr() as usize;

    let byte_size: usize = bincode::deserialize_from(&mut data)
                                    .expect("MEH: malicious input?");
    let start = data.as_ptr() as usize;
    let item_count: usize = bincode::deserialize_from(&mut data)
                                    .expect("MEH: malicious input?");

    // Remember how many bytes item_count occupied
    let item_count_size = data.as_ptr() as usize - start;

    let range = ItemRange {
        start: start - base,                      // byte offset to item_count
        length: byte_size + item_count_size,      // number of bytes for item_count + payload
        _boo: PhantomData,
    };

    // Adjust data pointer to skip read values
    *data = &data[byte_size ..];
    (range, item_count)
}


impl<'a> BuiltDisplayListIter<'a> {
    pub fn new(list: &'a BuiltDisplayList) -> Self {
        Self::new_with_list_and_data(list, list.item_slice())
    }

    pub fn new_with_list_and_data(list: &'a BuiltDisplayList, data: &'a [u8]) -> Self {
        BuiltDisplayListIter {
            list,
            data,
            cur_item: di::DisplayItem::PopStackingContext,
            cur_stops: ItemRange::default(),
            cur_glyphs: ItemRange::default(),
            cur_filters: ItemRange::default(),
            cur_filter_data: Vec::new(),
            cur_clip_chain_items: ItemRange::default(),
            cur_complex_clip: (ItemRange::default(), 0),
            peeking: Peek::NotPeeking,
            debug_stats: DebugStats {
                last_addr: data.as_ptr() as usize,
                stats: HashMap::default(),
            }
        }
    }

    pub fn display_list(&self) -> &'a BuiltDisplayList {
        self.list
    }

    pub fn next<'b>(&'b mut self) -> Option<DisplayItemRef<'a, 'b>> {
        use crate::DisplayItem::*;

        match self.peeking {
            Peek::IsPeeking => {
                self.peeking = Peek::NotPeeking;
                return Some(self.as_ref());
            }
            Peek::StartPeeking => {
                self.peeking = Peek::IsPeeking;
            }
            Peek::NotPeeking => { /* do nothing */ }
        }

        // Don't let these bleed into another item
        self.cur_stops = ItemRange::default();
        self.cur_complex_clip = (ItemRange::default(), 0);
        self.cur_clip_chain_items = ItemRange::default();

        loop {
            self.next_raw()?;
            match self.cur_item {
                SetGradientStops |
                SetFilterOps |
                SetFilterData => {
                    // These are marker items for populating other display items, don't yield them.
                    continue;
                }
                _ => {
                    break;
                }
            }
        }

        Some(self.as_ref())
    }

    /// Gets the next display item, even if it's a dummy. Also doesn't handle peeking
    /// and may leave irrelevant ranges live (so a Clip may have GradientStops if
    /// for some reason you ask).
    pub fn next_raw<'b>(&'b mut self) -> Option<DisplayItemRef<'a, 'b>> {
        use crate::DisplayItem::*;

        if self.data.is_empty() {
            return None;
        }

        {
            let reader = bincode::IoReader::new(UnsafeReader::new(&mut self.data));
            bincode::deserialize_in_place(reader, &mut self.cur_item)
                .expect("MEH: malicious process?");
        }

        self.log_item_stats();

        match self.cur_item {
            SetGradientStops => {
                self.cur_stops = skip_slice::<di::GradientStop>(self.list, &mut self.data).0;
                let temp = self.cur_stops;
                self.log_slice_stats("set_gradient_stops.stops", temp);
            }
            SetFilterOps => {
                self.cur_filters = skip_slice::<di::FilterOp>(self.list, &mut self.data).0;
                let temp = self.cur_filters;
                self.log_slice_stats("set_filter_ops.ops", temp);
            }
            SetFilterData => {
                self.cur_filter_data.push(TempFilterData {
                    func_types: skip_slice::<di::ComponentTransferFuncType>(self.list, &mut self.data).0,
                    r_values: skip_slice::<f32>(self.list, &mut self.data).0,
                    g_values: skip_slice::<f32>(self.list, &mut self.data).0,
                    b_values: skip_slice::<f32>(self.list, &mut self.data).0,
                    a_values: skip_slice::<f32>(self.list, &mut self.data).0,
                });

                let data = *self.cur_filter_data.last().unwrap();
                self.log_slice_stats("set_filter_data.func_types", data.func_types);
                self.log_slice_stats("set_filter_data.r_values", data.r_values);
                self.log_slice_stats("set_filter_data.g_values", data.g_values);
                self.log_slice_stats("set_filter_data.b_values", data.b_values);
                self.log_slice_stats("set_filter_data.a_values", data.a_values);
            }
            ClipChain(_) => {
                self.cur_clip_chain_items = skip_slice::<di::ClipId>(self.list, &mut self.data).0;
                let temp = self.cur_clip_chain_items;
                self.log_slice_stats("clip_chain.clip_ids", temp);
            }
            Clip(_) | ScrollFrame(_) => {
                self.cur_complex_clip = self.skip_slice::<di::ComplexClipRegion>();

                let name = if let Clip(_) = self.cur_item {
                    "clip.complex_clips"
                } else {
                    "scroll_frame.complex_clips"
                };
                let temp = self.cur_complex_clip.0;
                self.log_slice_stats(name, temp);
            }
            Text(_) => {
                self.cur_glyphs = self.skip_slice::<GlyphInstance>().0;
                let temp = self.cur_glyphs;
                self.log_slice_stats("text.glyphs", temp);
            }
            _ => { /* do nothing */ }
        }

        Some(self.as_ref())
    }

    fn skip_slice<T: for<'de> Deserialize<'de>>(&mut self) -> (ItemRange<T>, usize) {
        skip_slice::<T>(self.list, &mut self.data)
    }

    pub fn as_ref<'b>(&'b self) -> DisplayItemRef<'a, 'b> {
        DisplayItemRef { iter: self }
    }

    pub fn skip_current_stacking_context(&mut self) {
        let mut depth = 0;
        while let Some(item) = self.next() {
            match *item.item() {
                di::DisplayItem::PushStackingContext(..) => depth += 1,
                di::DisplayItem::PopStackingContext if depth == 0 => return,
                di::DisplayItem::PopStackingContext => depth -= 1,
                _ => {}
            }
            debug_assert!(depth >= 0);
        }
    }

    pub fn current_stacking_context_empty(&mut self) -> bool {
        match self.peek() {
            Some(item) => *item.item() == di::DisplayItem::PopStackingContext,
            None => true,
        }
    }

    pub fn peek<'b>(&'b mut self) -> Option<DisplayItemRef<'a, 'b>> {
        if self.peeking == Peek::NotPeeking {
            self.peeking = Peek::StartPeeking;
            self.next()
        } else {
            Some(self.as_ref())
        }
    }

    /// Get the debug stats for what this iterator has deserialized.
    /// Should always be empty in release builds.
    pub fn debug_stats(&mut self) -> Vec<(&'static str, ItemStats)> {
        let mut result = self.debug_stats.stats.drain().collect::<Vec<_>>();
        result.sort_by_key(|stats| stats.0);
        result
    }

    /// Adds the debug stats from another to our own, assuming we are a sub-iter of the other
    /// (so we can ignore where they were in the traversal).
    pub fn merge_debug_stats_from(&mut self, other: &mut Self) {
        for (key, other_entry) in other.debug_stats.stats.iter() {
            let entry = self.debug_stats.stats.entry(key).or_default();

            entry.total_count += other_entry.total_count;
            entry.num_bytes += other_entry.num_bytes;
        }
    }

    /// Logs stats for the last deserialized display item
    #[cfg(feature = "display_list_stats")]
    fn log_item_stats(&mut self) {
        let num_bytes = self.debug_num_bytes();

        let item_name = self.cur_item.debug_name();
        let entry = self.debug_stats.stats.entry(item_name).or_default();

        entry.total_count += 1;
        entry.num_bytes += num_bytes;
    }

    /// Logs the stats for the given serialized slice
    #[cfg(feature = "display_list_stats")]
    fn log_slice_stats<T: for<'de> Deserialize<'de>>(&mut self, slice_name: &'static str, range: ItemRange<T>) {
        // Run this so log_item_stats is accurate, but ignore its result
        // because log_slice_stats may be called after multiple slices have been
        // processed, and the `range` has everything we need.
        self.debug_num_bytes();

        let entry = self.debug_stats.stats.entry(slice_name).or_default();

        entry.total_count += self.list.get(range).size_hint().0;
        entry.num_bytes += range.length;
    }

    /// Computes the number of bytes we've processed since we last called
    /// this method, so we can compute the serialized size of a display item.
    #[cfg(feature = "display_list_stats")]
    fn debug_num_bytes(&mut self) -> usize {
        let old_addr = self.debug_stats.last_addr;
        let new_addr = self.data.as_ptr() as usize;
        let delta = new_addr - old_addr;
        self.debug_stats.last_addr = new_addr;

        delta
    }

    #[cfg(not(feature = "display_list_stats"))]
    fn log_item_stats(&mut self) { /* no-op */ }
    #[cfg(not(feature = "display_list_stats"))]
    fn log_slice_stats<T>(&mut self, _slice_name: &str, _range: ItemRange<T>) { /* no-op */ }
}

// Some of these might just become ItemRanges
impl<'a, 'b> DisplayItemRef<'a, 'b> {
    pub fn item(&self) -> &di::DisplayItem {
        &self.iter.cur_item
    }

    pub fn complex_clip(&self) -> (ItemRange<di::ComplexClipRegion>, usize) {
        self.iter.cur_complex_clip
    }

    pub fn gradient_stops(&self) -> ItemRange<di::GradientStop> {
        self.iter.cur_stops
    }

    pub fn glyphs(&self) -> ItemRange<GlyphInstance> {
        self.iter.cur_glyphs
    }

    pub fn filters(&self) -> ItemRange<di::FilterOp> {
        self.iter.cur_filters
    }

    pub fn filter_datas(&self) -> &Vec<TempFilterData> {
        &self.iter.cur_filter_data
    }

    pub fn clip_chain_items(&self) -> ItemRange<di::ClipId> {
        self.iter.cur_clip_chain_items
    }

    pub fn display_list(&self) -> &BuiltDisplayList {
        self.iter.display_list()
    }

    // Creates a new iterator where this element's iterator is, to hack around borrowck.
    pub fn sub_iter(&self) -> BuiltDisplayListIter<'a> {
        BuiltDisplayListIter::new_with_list_and_data(self.iter.list, self.iter.data)
    }
}

impl<'de, 'a, T: Deserialize<'de>> AuxIter<'a, T> {
    pub fn new(mut data: &'a [u8]) -> Self {
        let size: usize = if data.is_empty() {
            0 // Accept empty ItemRanges pointing anywhere
        } else {
            bincode::deserialize_from(&mut UnsafeReader::new(&mut data)).expect("MEH: malicious input?")
        };

        AuxIter {
            data,
            size,
            _boo: PhantomData,
        }
    }
}

impl<'a, T: for<'de> Deserialize<'de>> Iterator for AuxIter<'a, T> {
    type Item = T;

    fn next(&mut self) -> Option<T> {
        if self.size == 0 {
            None
        } else {
            self.size -= 1;
            Some(
                bincode::deserialize_from(&mut UnsafeReader::new(&mut self.data))
                    .expect("MEH: malicious input?"),
            )
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.size, Some(self.size))
    }
}

impl<'a, T: for<'de> Deserialize<'de>> ::std::iter::ExactSizeIterator for AuxIter<'a, T> {}


#[cfg(feature = "serialize")]
impl Serialize for BuiltDisplayList {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        use crate::display_item::DisplayItem as Real;
        use crate::display_item::DebugDisplayItem as Debug;

        let mut seq = serializer.serialize_seq(None)?;
        let mut traversal = self.iter();
        while let Some(item) = traversal.next_raw() {
            let serial_di = match *item.item() {
                Real::Clip(v) => Debug::Clip(
                    v,
                    item.iter.list.get(item.iter.cur_complex_clip.0).collect()
                ),
                Real::ClipChain(v) => Debug::ClipChain(
                    v,
                    item.iter.list.get(item.iter.cur_clip_chain_items).collect(),
                ),
                Real::ScrollFrame(v) => Debug::ScrollFrame(
                    v,
                    item.iter.list.get(item.iter.cur_complex_clip.0).collect()
                ),
                Real::Text(v) => Debug::Text(
                    v,
                    item.iter.list.get(item.iter.cur_glyphs).collect()
                ),
                Real::SetFilterOps => Debug::SetFilterOps(
                    item.iter.list.get(item.iter.cur_filters).collect()
                ),
                Real::SetFilterData => {
                    debug_assert!(!item.iter.cur_filter_data.is_empty());
                    let temp_filter_data = &item.iter.cur_filter_data[item.iter.cur_filter_data.len()-1];

                    let func_types: Vec<di::ComponentTransferFuncType> =
                        item.iter.list.get(temp_filter_data.func_types).collect();
                    debug_assert!(func_types.len() == 4);
                    Debug::SetFilterData(di::FilterData {
                        func_r_type: func_types[0],
                        r_values: item.iter.list.get(temp_filter_data.r_values).collect(),
                        func_g_type: func_types[1],
                        g_values: item.iter.list.get(temp_filter_data.g_values).collect(),
                        func_b_type: func_types[2],
                        b_values: item.iter.list.get(temp_filter_data.b_values).collect(),
                        func_a_type: func_types[3],
                        a_values: item.iter.list.get(temp_filter_data.a_values).collect(),
                    })
                },
                Real::SetGradientStops => Debug::SetGradientStops(
                    item.iter.list.get(item.iter.cur_stops).collect()
                ),
                Real::StickyFrame(v) => Debug::StickyFrame(v),
                Real::Rectangle(v) => Debug::Rectangle(v),
                Real::ClearRectangle(v) => Debug::ClearRectangle(v),
                Real::HitTest(v) => Debug::HitTest(v),
                Real::Line(v) => Debug::Line(v),
                Real::Image(v) => Debug::Image(v),
                Real::YuvImage(v) => Debug::YuvImage(v),
                Real::Border(v) => Debug::Border(v),
                Real::BoxShadow(v) => Debug::BoxShadow(v),
                Real::Gradient(v) => Debug::Gradient(v),
                Real::RadialGradient(v) => Debug::RadialGradient(v),
                Real::Iframe(v) => Debug::Iframe(v),
                Real::PushReferenceFrame(v) => Debug::PushReferenceFrame(v),
                Real::PushStackingContext(v) => Debug::PushStackingContext(v),
                Real::PushShadow(v) => Debug::PushShadow(v),

                Real::PopReferenceFrame => Debug::PopReferenceFrame,
                Real::PopStackingContext => Debug::PopStackingContext,
                Real::PopAllShadows => Debug::PopAllShadows,
            };
            seq.serialize_element(&serial_di)?
        }
        seq.end()
    }
}

// The purpose of this implementation is to deserialize
// a display list from one format just to immediately
// serialize then into a "built" `Vec<u8>`.

#[cfg(feature = "deserialize")]
impl<'de> Deserialize<'de> for BuiltDisplayList {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        use crate::display_item::DisplayItem as Real;
        use crate::display_item::DebugDisplayItem as Debug;

        let list = Vec::<Debug>::deserialize(deserializer)?;

        let mut data = Vec::new();
        let mut temp = Vec::new();
        let mut total_clip_nodes = FIRST_CLIP_NODE_INDEX;
        let mut total_spatial_nodes = FIRST_SPATIAL_NODE_INDEX;
        for complete in list {
            let item = match complete {
                Debug::Clip(v, complex_clips) => {
                    total_clip_nodes += 1;
                    DisplayListBuilder::push_iter_impl(&mut temp, complex_clips);
                    Real::Clip(v)
                },
                Debug::ClipChain(v, clip_chain_ids) => {
                    DisplayListBuilder::push_iter_impl(&mut temp, clip_chain_ids);
                    Real::ClipChain(v)
                }
                Debug::ScrollFrame(v, complex_clips) => {
                    total_spatial_nodes += 1;
                    total_clip_nodes += 1;
                    DisplayListBuilder::push_iter_impl(&mut temp, complex_clips);
                    Real::ScrollFrame(v)
                }
                Debug::StickyFrame(v) => {
                    total_spatial_nodes += 1;
                    Real::StickyFrame(v)
                }
                Debug::Text(v, glyphs) => {
                    DisplayListBuilder::push_iter_impl(&mut temp, glyphs);
                    Real::Text(v)
                },
                Debug::Iframe(v) => {
                    total_clip_nodes += 1;
                    Real::Iframe(v)
                }
                Debug::PushReferenceFrame(v) => {
                    total_spatial_nodes += 1;
                    Real::PushReferenceFrame(v)
                }
                Debug::SetFilterOps(filters) => {
                    DisplayListBuilder::push_iter_impl(&mut temp, filters);
                    Real::SetFilterOps
                },
                Debug::SetFilterData(filter_data) => {
                    let func_types: Vec<di::ComponentTransferFuncType> =
                        [filter_data.func_r_type,
                         filter_data.func_g_type,
                         filter_data.func_b_type,
                         filter_data.func_a_type].to_vec();
                    DisplayListBuilder::push_iter_impl(&mut temp, func_types);
                    DisplayListBuilder::push_iter_impl(&mut temp, filter_data.r_values);
                    DisplayListBuilder::push_iter_impl(&mut temp, filter_data.g_values);
                    DisplayListBuilder::push_iter_impl(&mut temp, filter_data.b_values);
                    DisplayListBuilder::push_iter_impl(&mut temp, filter_data.a_values);
                    Real::SetFilterData
                },
                Debug::SetGradientStops(stops) => {
                    DisplayListBuilder::push_iter_impl(&mut temp, stops);
                    Real::SetGradientStops
                },

                Debug::Rectangle(v) => Real::Rectangle(v),
                Debug::ClearRectangle(v) => Real::ClearRectangle(v),
                Debug::HitTest(v) => Real::HitTest(v),
                Debug::Line(v) => Real::Line(v),
                Debug::Image(v) => Real::Image(v),
                Debug::YuvImage(v) => Real::YuvImage(v),
                Debug::Border(v) => Real::Border(v),
                Debug::BoxShadow(v) => Real::BoxShadow(v),
                Debug::Gradient(v) => Real::Gradient(v),
                Debug::RadialGradient(v) => Real::RadialGradient(v),
                Debug::PushStackingContext(v) => Real::PushStackingContext(v),
                Debug::PushShadow(v) => Real::PushShadow(v),

                Debug::PopStackingContext => Real::PopStackingContext,
                Debug::PopReferenceFrame => Real::PopReferenceFrame,
                Debug::PopAllShadows => Real::PopAllShadows,
            };
            serialize_fast(&mut data, &item);
            // the aux data is serialized after the item, hence the temporary
            data.extend(temp.drain(..));
        }

        Ok(BuiltDisplayList {
            data,
            descriptor: BuiltDisplayListDescriptor {
                builder_start_time: 0,
                builder_finish_time: 1,
                send_start_time: 0,
                total_clip_nodes,
                total_spatial_nodes,
            },
        })
    }
}

// This is a replacement for bincode::serialize_into(&vec)
// The default implementation Write for Vec will basically
// call extend_from_slice(). Serde ends up calling that for every
// field of a struct that we're serializing. extend_from_slice()
// does not get inlined and thus we end up calling a generic memcpy()
// implementation. If we instead reserve enough room for the serialized
// struct in the Vec ahead of time we can rely on that and use
// the following UnsafeVecWriter to write into the vec without
// any checks. This writer assumes that size returned by the
// serialize function will not change between calls to serialize_into:
//
// For example, the following struct will cause memory unsafety when
// used with UnsafeVecWriter.
//
// struct S {
//    first: Cell<bool>,
// }
//
// impl Serialize for S {
//    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
//        where S: Serializer
//    {
//        if self.first.get() {
//            self.first.set(false);
//            ().serialize(serializer)
//        } else {
//            0.serialize(serializer)
//        }
//    }
// }
//

struct UnsafeVecWriter(*mut u8);

impl Write for UnsafeVecWriter {
    #[inline(always)]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        unsafe {
            ptr::copy_nonoverlapping(buf.as_ptr(), self.0, buf.len());
            self.0 = self.0.add(buf.len());
        }
        Ok(buf.len())
    }

    #[inline(always)]
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        unsafe {
            ptr::copy_nonoverlapping(buf.as_ptr(), self.0, buf.len());
            self.0 = self.0.add(buf.len());
        }
        Ok(())
    }

    #[inline(always)]
    fn flush(&mut self) -> io::Result<()> { Ok(()) }
}

struct SizeCounter(usize);

impl<'a> Write for SizeCounter {
    #[inline(always)]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.0 += buf.len();
        Ok(buf.len())
    }

    #[inline(always)]
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.0 += buf.len();
        Ok(())
    }

    #[inline(always)]
    fn flush(&mut self) -> io::Result<()> { Ok(()) }
}

/// Serializes a value assuming the Serialize impl has a stable size across two
/// invocations.
///
/// If this assumption is incorrect, the result will be Undefined Behaviour. This
/// assumption should hold for all derived Serialize impls, which is all we currently
/// use.
fn serialize_fast<T: Serialize>(vec: &mut Vec<u8>, e: T) {
    // manually counting the size is faster than vec.reserve(bincode::serialized_size(&e) as usize) for some reason
    let mut size = SizeCounter(0);
    bincode::serialize_into(&mut size, &e).unwrap();
    vec.reserve(size.0);

    let old_len = vec.len();
    let ptr = unsafe { vec.as_mut_ptr().add(old_len) };
    let mut w = UnsafeVecWriter(ptr);
    bincode::serialize_into(&mut w, &e).unwrap();

    // fix up the length
    unsafe { vec.set_len(old_len + size.0); }

    // make sure we wrote the right amount
    debug_assert_eq!(((w.0 as usize) - (vec.as_ptr() as usize)), vec.len());
}

/// Serializes an iterator, assuming:
///
/// * The Clone impl is trivial (e.g. we're just memcopying a slice iterator)
/// * The ExactSizeIterator impl is stable and correct across a Clone
/// * The Serialize impl has a stable size across two invocations
///
/// If the first is incorrect, WebRender will be very slow. If the other two are
/// incorrect, the result will be Undefined Behaviour! The ExactSizeIterator
/// bound would ideally be replaced with a TrustedLen bound to protect us a bit
/// better, but that trait isn't stable (and won't be for a good while, if ever).
///
/// Debug asserts are included that should catch all Undefined Behaviour, but
/// we can't afford to include these in release builds.
fn serialize_iter_fast<I>(vec: &mut Vec<u8>, iter: I) -> usize
where I: ExactSizeIterator + Clone,
      I::Item: Serialize,
{
    // manually counting the size is faster than vec.reserve(bincode::serialized_size(&e) as usize) for some reason
    let mut size = SizeCounter(0);
    let mut count1 = 0;

    for e in iter.clone() {
        bincode::serialize_into(&mut size, &e).unwrap();
        count1 += 1;
    }

    vec.reserve(size.0);

    let old_len = vec.len();
    let ptr = unsafe { vec.as_mut_ptr().add(old_len) };
    let mut w = UnsafeVecWriter(ptr);
    let mut count2 = 0;

    for e in iter {
        bincode::serialize_into(&mut w, &e).unwrap();
        count2 += 1;
    }

    // fix up the length
    unsafe { vec.set_len(old_len + size.0); }

    // make sure we wrote the right amount
    debug_assert_eq!(((w.0 as usize) - (vec.as_ptr() as usize)), vec.len());
    debug_assert_eq!(count1, count2);

    count1
}

// This uses a (start, end) representation instead of (start, len) so that
// only need to update a single field as we read through it. This
// makes it easier for llvm to understand what's going on. (https://github.com/rust-lang/rust/issues/45068)
// We update the slice only once we're done reading
struct UnsafeReader<'a: 'b, 'b> {
    start: *const u8,
    end: *const u8,
    slice: &'b mut &'a [u8],
}

impl<'a, 'b> UnsafeReader<'a, 'b> {
    #[inline(always)]
    fn new(buf: &'b mut &'a [u8]) -> UnsafeReader<'a, 'b> {
        unsafe {
            let end = buf.as_ptr().add(buf.len());
            let start = buf.as_ptr();
            UnsafeReader { start, end, slice: buf }
        }
    }

    // This read implementation is significantly faster than the standard &[u8] one.
    //
    // First, it only supports reading exactly buf.len() bytes. This ensures that
    // the argument to memcpy is always buf.len() and will allow a constant buf.len()
    // to be propagated through to memcpy which LLVM will turn into explicit loads and
    // stores. The standard implementation does a len = min(slice.len(), buf.len())
    //
    // Second, we only need to adjust 'start' after reading and it's only adjusted by a
    // constant. This allows LLVM to avoid adjusting the length field after ever read
    // and lets it be aggregated into a single adjustment.
    #[inline(always)]
    fn read_internal(&mut self, buf: &mut [u8]) {
        // this is safe because we panic if start + buf.len() > end
        unsafe {
            assert!(self.start.add(buf.len()) <= self.end, "UnsafeReader: read past end of target");
            ptr::copy_nonoverlapping(self.start, buf.as_mut_ptr(), buf.len());
            self.start = self.start.add(buf.len());
        }
    }
}

impl<'a, 'b> Drop for UnsafeReader<'a, 'b> {
    // this adjusts input slice so that it properly represents the amount that's left.
    #[inline(always)]
    fn drop(&mut self) {
        // this is safe because we know that start and end are contained inside the original slice
        unsafe {
            *self.slice = slice::from_raw_parts(self.start, (self.end as usize) - (self.start as usize));
        }
    }
}

impl<'a, 'b> Read for UnsafeReader<'a, 'b> {
    // These methods were not being inlined and we need them to be so that the memcpy
    // is for a constant size
    #[inline(always)]
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.read_internal(buf);
        Ok(buf.len())
    }
    #[inline(always)]
    fn read_exact(&mut self, buf: &mut [u8]) -> io::Result<()> {
        self.read_internal(buf);
        Ok(())
    }
}

#[derive(Clone, Debug)]
pub struct SaveState {
    dl_len: usize,
    next_clip_index: usize,
    next_spatial_index: usize,
    next_clip_chain_id: u64,
}

#[derive(Clone)]
pub struct DisplayListBuilder {
    pub data: Vec<u8>,
    pub pipeline_id: PipelineId,
    next_clip_index: usize,
    next_spatial_index: usize,
    next_clip_chain_id: u64,
    builder_start_time: u64,

    /// The size of the content of this display list. This is used to allow scrolling
    /// outside the bounds of the display list items themselves.
    content_size: LayoutSize,
    save_state: Option<SaveState>,
}

impl DisplayListBuilder {
    pub fn new(pipeline_id: PipelineId, content_size: LayoutSize) -> Self {
        Self::with_capacity(pipeline_id, content_size, 0)
    }

    pub fn with_capacity(
        pipeline_id: PipelineId,
        content_size: LayoutSize,
        capacity: usize,
    ) -> Self {
        let start_time = precise_time_ns();

        DisplayListBuilder {
            data: Vec::with_capacity(capacity),
            pipeline_id,
            next_clip_index: FIRST_CLIP_NODE_INDEX,
            next_spatial_index: FIRST_SPATIAL_NODE_INDEX,
            next_clip_chain_id: 0,
            builder_start_time: start_time,
            content_size,
            save_state: None,
        }
    }

    /// Return the content size for this display list
    pub fn content_size(&self) -> LayoutSize {
        self.content_size
    }

    /// Saves the current display list state, so it may be `restore()`'d.
    ///
    /// # Conditions:
    ///
    /// * Doesn't support popping clips that were pushed before the save.
    /// * Doesn't support nested saves.
    /// * Must call `clear_save()` if the restore becomes unnecessary.
    pub fn save(&mut self) {
        assert!(self.save_state.is_none(), "DisplayListBuilder doesn't support nested saves");

        self.save_state = Some(SaveState {
            dl_len: self.data.len(),
            next_clip_index: self.next_clip_index,
            next_spatial_index: self.next_spatial_index,
            next_clip_chain_id: self.next_clip_chain_id,
        });
    }

    /// Restores the state of the builder to when `save()` was last called.
    pub fn restore(&mut self) {
        let state = self.save_state.take().expect("No save to restore DisplayListBuilder from");

        self.data.truncate(state.dl_len);
        self.next_clip_index = state.next_clip_index;
        self.next_spatial_index = state.next_spatial_index;
        self.next_clip_chain_id = state.next_clip_chain_id;
    }

    /// Discards the builder's save (indicating the attempted operation was successful).
    pub fn clear_save(&mut self) {
        self.save_state.take().expect("No save to clear in DisplayListBuilder");
    }

    /// Print the display items in the list to stdout.
    pub fn print_display_list(&mut self) {
        self.emit_display_list(0, Range { start: None, end: None }, stdout());
    }

    /// Emits a debug representation of display items in the list, for debugging
    /// purposes. If the range's start parameter is specified, only display
    /// items starting at that index (inclusive) will be printed. If the range's
    /// end parameter is specified, only display items before that index
    /// (exclusive) will be printed. Calling this function with end <= start is
    /// allowed but is just a waste of CPU cycles. The function emits the
    /// debug representation of the selected display items, one per line, with
    /// the given indent, to the provided sink object. The return value is
    /// the total number of items in the display list, which allows the
    /// caller to subsequently invoke this function to only dump the newly-added
    /// items.
    pub fn emit_display_list<W>(
        &mut self,
        indent: usize,
        range: Range<Option<usize>>,
        mut sink: W,
    ) -> usize
    where
        W: Write
    {
        let mut temp = BuiltDisplayList::default();
        mem::swap(&mut temp.data, &mut self.data);

        let mut index: usize = 0;
        {
            let mut iter = BuiltDisplayListIter::new(&temp);
            while let Some(item) = iter.next_raw() {
                if index >= range.start.unwrap_or(0) && range.end.map_or(true, |e| index < e) {
                    writeln!(sink, "{}{:?}", "  ".repeat(indent), item.item()).unwrap();
                }
                index += 1;
            }
        }

        self.data = temp.data;
        index
    }

    /// Add an item to the display list.
    ///
    /// NOTE: It is usually preferable to use the specialized methods to push
    /// display items. Pushing unexpected or invalid items here may
    /// result in WebRender panicking or behaving in unexpected ways.
    #[inline]
    pub fn push_item(&mut self, item: &di::DisplayItem) {
        serialize_fast(&mut self.data, item);
    }

    fn push_iter_impl<I>(data: &mut Vec<u8>, iter_source: I)
    where
        I: IntoIterator,
        I::IntoIter: ExactSizeIterator + Clone,
        I::Item: Serialize,
    {
        let iter = iter_source.into_iter();
        let len = iter.len();
        // Format:
        // payload_byte_size: usize, item_count: usize, [I; item_count]

        // We write a dummy value so there's room for later
        let byte_size_offset = data.len();
        serialize_fast(data, &0usize);
        serialize_fast(data, &len);
        let payload_offset = data.len();

        let count = serialize_iter_fast(data, iter);

        // Now write the actual byte_size
        let final_offset = data.len();
        let byte_size = final_offset - payload_offset;

        // Note we don't use serialize_fast because we don't want to change the Vec's len
        bincode::serialize_into(
            &mut &mut data[byte_size_offset..],
            &byte_size,
        ).unwrap();

        debug_assert_eq!(len, count);
    }

    /// Push items from an iterator to the display list.
    ///
    /// NOTE: Pushing unexpected or invalid items to the display list
    /// may result in panic and confusion.
    pub fn push_iter<I>(&mut self, iter: I)
    where
        I: IntoIterator,
        I::IntoIter: ExactSizeIterator + Clone,
        I::Item: Serialize,
    {
        Self::push_iter_impl(&mut self.data, iter);
    }

    pub fn push_rect(
        &mut self,
        common: &di::CommonItemProperties,
        color: ColorF,
    ) {
        let item = di::DisplayItem::Rectangle(di::RectangleDisplayItem {
            common: *common,
            color
        });
        self.push_item(&item);
    }

    pub fn push_clear_rect(
        &mut self,
        common: &di::CommonItemProperties,
    ) {
        let item = di::DisplayItem::ClearRectangle(di::ClearRectangleDisplayItem {
            common: *common,
        });
        self.push_item(&item);
    }

    pub fn push_hit_test(
        &mut self,
        common: &di::CommonItemProperties,
    ) {
        let item = di::DisplayItem::HitTest(di::HitTestDisplayItem {
            common: *common,
        });
        self.push_item(&item);
    }

    pub fn push_line(
        &mut self,
        common: &di::CommonItemProperties,
        area: &LayoutRect,
        wavy_line_thickness: f32,
        orientation: di::LineOrientation,
        color: &ColorF,
        style: di::LineStyle,
    ) {
        let item = di::DisplayItem::Line(di::LineDisplayItem {
            common: *common,
            area: *area,
            wavy_line_thickness,
            orientation,
            color: *color,
            style,
        });

        self.push_item(&item);
    }

    pub fn push_image(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        image_rendering: di::ImageRendering,
        alpha_type: di::AlphaType,
        key: ImageKey,
        color: ColorF,
    ) {
        let item = di::DisplayItem::Image(di::ImageDisplayItem {
            common: *common,
            bounds,
            image_key: key,
            stretch_size,
            tile_spacing,
            image_rendering,
            alpha_type,
            color,
        });

        self.push_item(&item);
    }

    /// Push a yuv image. All planar data in yuv image should use the same buffer type.
    pub fn push_yuv_image(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        yuv_data: di::YuvData,
        color_depth: ColorDepth,
        color_space: di::YuvColorSpace,
        image_rendering: di::ImageRendering,
    ) {
        let item = di::DisplayItem::YuvImage(di::YuvImageDisplayItem {
            common: *common,
            bounds,
            yuv_data,
            color_depth,
            color_space,
            image_rendering,
        });
        self.push_item(&item);
    }

    pub fn push_text(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        glyphs: &[GlyphInstance],
        font_key: FontInstanceKey,
        color: ColorF,
        glyph_options: Option<GlyphOptions>,
    ) {
        let item = di::DisplayItem::Text(di::TextDisplayItem {
            common: *common,
            bounds: bounds,
            color,
            font_key,
            glyph_options,
        });

        for split_glyphs in glyphs.chunks(MAX_TEXT_RUN_LENGTH) {
            self.push_item(&item);
            self.push_iter(split_glyphs);
        }
    }

    /// NOTE: gradients must be pushed in the order they're created
    /// because create_gradient stores the stops in anticipation.
    pub fn create_gradient(
        &mut self,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stops: Vec<di::GradientStop>,
        extend_mode: di::ExtendMode,
    ) -> di::Gradient {
        let mut builder = GradientBuilder::with_stops(stops);
        let gradient = builder.gradient(start_point, end_point, extend_mode);
        self.push_stops(builder.stops());
        gradient
    }

    /// NOTE: gradients must be pushed in the order they're created
    /// because create_gradient stores the stops in anticipation.
    pub fn create_radial_gradient(
        &mut self,
        center: LayoutPoint,
        radius: LayoutSize,
        stops: Vec<di::GradientStop>,
        extend_mode: di::ExtendMode,
    ) -> di::RadialGradient {
        let mut builder = GradientBuilder::with_stops(stops);
        let gradient = builder.radial_gradient(center, radius, extend_mode);
        self.push_stops(builder.stops());
        gradient
    }

    pub fn push_border(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        widths: LayoutSideOffsets,
        details: di::BorderDetails,
    ) {
        let item = di::DisplayItem::Border(di::BorderDisplayItem {
            common: *common,
            bounds,
            details,
            widths,
        });

        self.push_item(&item);
    }

    pub fn push_box_shadow(
        &mut self,
        common: &di::CommonItemProperties,
        box_bounds: LayoutRect,
        offset: LayoutVector2D,
        color: ColorF,
        blur_radius: f32,
        spread_radius: f32,
        border_radius: di::BorderRadius,
        clip_mode: di::BoxShadowClipMode,
    ) {
        let item = di::DisplayItem::BoxShadow(di::BoxShadowDisplayItem {
            common: *common,
            box_bounds,
            offset,
            color,
            blur_radius,
            spread_radius,
            border_radius,
            clip_mode,
        });

        self.push_item(&item);
    }

    /// Pushes a linear gradient to be displayed.
    ///
    /// The gradient itself is described in the
    /// `gradient` parameter. It is drawn on
    /// a "tile" with the dimensions from `tile_size`.
    /// These tiles are now repeated to the right and
    /// to the bottom infinitely. If `tile_spacing`
    /// is not zero spacers with the given dimensions
    /// are inserted between the tiles as seams.
    ///
    /// The origin of the tiles is given in `layout.rect.origin`.
    /// If the gradient should only be displayed once limit
    /// the `layout.rect.size` to a single tile.
    /// The gradient is only visible within the local clip.
    pub fn push_gradient(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        gradient: di::Gradient,
        tile_size: LayoutSize,
        tile_spacing: LayoutSize,
    ) {
        let item = di::DisplayItem::Gradient(di::GradientDisplayItem {
            common: *common,
            bounds,
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(&item);
    }

    /// Pushes a radial gradient to be displayed.
    ///
    /// See [`push_gradient`](#method.push_gradient) for explanation.
    pub fn push_radial_gradient(
        &mut self,
        common: &di::CommonItemProperties,
        bounds: LayoutRect,
        gradient: di::RadialGradient,
        tile_size: LayoutSize,
        tile_spacing: LayoutSize,
    ) {
        let item = di::DisplayItem::RadialGradient(di::RadialGradientDisplayItem {
            common: *common,
            bounds,
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(&item);
    }

    pub fn push_reference_frame(
        &mut self,
        origin: LayoutPoint,
        parent_spatial_id: di::SpatialId,
        transform_style: di::TransformStyle,
        transform: PropertyBinding<LayoutTransform>,
        kind: di::ReferenceFrameKind,
    ) -> di::SpatialId {
        let id = self.generate_spatial_index();

        let item = di::DisplayItem::PushReferenceFrame(di::ReferenceFrameDisplayListItem {
            parent_spatial_id,
            origin,
            reference_frame: di::ReferenceFrame {
                transform_style,
                transform,
                kind,
                id,
            },
        });

        self.push_item(&item);
        id
    }

    pub fn pop_reference_frame(&mut self) {
        self.push_item(&di::DisplayItem::PopReferenceFrame);
    }

    pub fn push_stacking_context(
        &mut self,
        origin: LayoutPoint,
        spatial_id: di::SpatialId,
        is_backface_visible: bool,
        clip_id: Option<di::ClipId>,
        transform_style: di::TransformStyle,
        mix_blend_mode: di::MixBlendMode,
        filters: &[di::FilterOp],
        filter_datas: &[di::FilterData],
        raster_space: di::RasterSpace,
        cache_tiles: bool,
    ) {
        if filters.len() > 0 {
            self.push_item(&di::DisplayItem::SetFilterOps);
            self.push_iter(filters);
        }

        for filter_data in filter_datas {
            let func_types = [
                filter_data.func_r_type, filter_data.func_g_type,
                filter_data.func_b_type, filter_data.func_a_type];
            self.push_item(&di::DisplayItem::SetFilterData);
            self.push_iter(&func_types);
            self.push_iter(&filter_data.r_values);
            self.push_iter(&filter_data.g_values);
            self.push_iter(&filter_data.b_values);
            self.push_iter(&filter_data.a_values);
        }

        let item = di::DisplayItem::PushStackingContext(di::PushStackingContextDisplayItem {
            origin,
            spatial_id,
            is_backface_visible,
            stacking_context: di::StackingContext {
                transform_style,
                mix_blend_mode,
                clip_id,
                raster_space,
                cache_tiles,
            },
        });

        self.push_item(&item);
    }

    /// Helper for examples/ code.
    pub fn push_simple_stacking_context(
        &mut self,
        origin: LayoutPoint,
        spatial_id: di::SpatialId,
        is_backface_visible: bool,
    ) {
        self.push_simple_stacking_context_with_filters(origin, spatial_id, is_backface_visible, &[], &[]);
    }

    /// Helper for examples/ code.
    pub fn push_simple_stacking_context_with_filters(
        &mut self,
        origin: LayoutPoint,
        spatial_id: di::SpatialId,
        is_backface_visible: bool,
        filters: &[di::FilterOp],
        filter_datas: &[di::FilterData],
    ) {
        self.push_stacking_context(
            origin,
            spatial_id,
            is_backface_visible,
            None,
            di::TransformStyle::Flat,
            di::MixBlendMode::Normal,
            filters,
            filter_datas,
            di::RasterSpace::Screen,
            /* cache_tiles = */ false,
        );
    }

    pub fn pop_stacking_context(&mut self) {
        self.push_item(&di::DisplayItem::PopStackingContext);
    }

    pub fn push_stops(&mut self, stops: &[di::GradientStop]) {
        if stops.is_empty() {
            return;
        }
        self.push_item(&di::DisplayItem::SetGradientStops);
        self.push_iter(stops);
    }

    fn generate_clip_index(&mut self) -> di::ClipId {
        self.next_clip_index += 1;
        di::ClipId::Clip(self.next_clip_index - 1, self.pipeline_id)
    }

    fn generate_spatial_index(&mut self) -> di::SpatialId {
        self.next_spatial_index += 1;
        di::SpatialId::new(self.next_spatial_index - 1, self.pipeline_id)
    }

    fn generate_clip_chain_id(&mut self) -> di::ClipChainId {
        self.next_clip_chain_id += 1;
        di::ClipChainId(self.next_clip_chain_id - 1, self.pipeline_id)
    }

    pub fn define_scroll_frame<I>(
        &mut self,
        parent_space_and_clip: &di::SpaceAndClipInfo,
        external_id: Option<di::ExternalScrollId>,
        content_rect: LayoutRect,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<di::ImageMask>,
        scroll_sensitivity: di::ScrollSensitivity,
        external_scroll_offset: LayoutVector2D,
    ) -> di::SpaceAndClipInfo
    where
        I: IntoIterator<Item = di::ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let clip_id = self.generate_clip_index();
        let scroll_frame_id = self.generate_spatial_index();
        let item = di::DisplayItem::ScrollFrame(di::ScrollFrameDisplayItem {
            content_rect,
            clip_rect,
            parent_space_and_clip: *parent_space_and_clip,
            clip_id,
            scroll_frame_id,
            external_id,
            image_mask,
            scroll_sensitivity,
            external_scroll_offset,
        });

        self.push_item(&item);
        self.push_iter(complex_clips);

        di::SpaceAndClipInfo {
            spatial_id: scroll_frame_id,
            clip_id,
        }
    }

    pub fn define_clip_chain<I>(
        &mut self,
        parent: Option<di::ClipChainId>,
        clips: I,
    ) -> di::ClipChainId
    where
        I: IntoIterator<Item = di::ClipId>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let id = self.generate_clip_chain_id();
        self.push_item(&di::DisplayItem::ClipChain(di::ClipChainItem { id, parent }));
        self.push_iter(clips);
        id
    }

    pub fn define_clip<I>(
        &mut self,
        parent_space_and_clip: &di::SpaceAndClipInfo,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<di::ImageMask>,
    ) -> di::ClipId
    where
        I: IntoIterator<Item = di::ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let id = self.generate_clip_index();
        let item = di::DisplayItem::Clip(di::ClipDisplayItem {
            id,
            parent_space_and_clip: *parent_space_and_clip,
            clip_rect,
            image_mask,
        });

        self.push_item(&item);
        self.push_iter(complex_clips);
        id
    }

    pub fn define_sticky_frame(
        &mut self,
        parent_spatial_id: di::SpatialId,
        frame_rect: LayoutRect,
        margins: SideOffsets2D<Option<f32>>,
        vertical_offset_bounds: di::StickyOffsetBounds,
        horizontal_offset_bounds: di::StickyOffsetBounds,
        previously_applied_offset: LayoutVector2D,
    ) -> di::SpatialId {
        let id = self.generate_spatial_index();
        let item = di::DisplayItem::StickyFrame(di::StickyFrameDisplayItem {
            parent_spatial_id,
            id,
            bounds: frame_rect,
            margins,
            vertical_offset_bounds,
            horizontal_offset_bounds,
            previously_applied_offset,
        });

        self.push_item(&item);
        id
    }

    pub fn push_iframe(
        &mut self,
        bounds: LayoutRect,
        clip_rect: LayoutRect,
        space_and_clip: &di::SpaceAndClipInfo,
        pipeline_id: PipelineId,
        ignore_missing_pipeline: bool
    ) {
        let item = di::DisplayItem::Iframe(di::IframeDisplayItem {
            bounds,
            clip_rect,
            space_and_clip: *space_and_clip,
            pipeline_id,
            ignore_missing_pipeline,
        });
        self.push_item(&item);
    }

    pub fn push_shadow(
        &mut self,
        space_and_clip: &di::SpaceAndClipInfo,
        shadow: di::Shadow,
        should_inflate: bool,
    ) {
        let item = di::DisplayItem::PushShadow(di::PushShadowDisplayItem {
            space_and_clip: *space_and_clip,
            shadow,
            should_inflate,
        });
        self.push_item(&item);
    }

    pub fn pop_all_shadows(&mut self) {
        self.push_item(&di::DisplayItem::PopAllShadows);
    }

    pub fn finalize(self) -> (PipelineId, LayoutSize, BuiltDisplayList) {
        assert!(self.save_state.is_none(), "Finalized DisplayListBuilder with a pending save");

        let end_time = precise_time_ns();

        (
            self.pipeline_id,
            self.content_size,
            BuiltDisplayList {
                descriptor: BuiltDisplayListDescriptor {
                    builder_start_time: self.builder_start_time,
                    builder_finish_time: end_time,
                    send_start_time: 0,
                    total_clip_nodes: self.next_clip_index,
                    total_spatial_nodes: self.next_spatial_index,
                },
                data: self.data,
            },
        )
    }
}
