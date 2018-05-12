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
use std::io::{Read, Write};
use std::marker::PhantomData;
use std::{io, mem, ptr, slice};
use time::precise_time_ns;
use {AlphaType, BorderDetails, BorderDisplayItem, BorderRadius, BorderWidths, BoxShadowClipMode};
use {BoxShadowDisplayItem, ClipAndScrollInfo, ClipChainId, ClipChainItem, ClipDisplayItem, ClipId};
use {ColorF, ComplexClipRegion, DisplayItem, ExtendMode, ExternalScrollId, FilterOp};
use {FontInstanceKey, GlyphInstance, GlyphOptions, GlyphRasterSpace, Gradient};
use {GradientDisplayItem, GradientStop, IframeDisplayItem, ImageDisplayItem, ImageKey, ImageMask};
use {ImageRendering, LayoutPoint, LayoutPrimitiveInfo, LayoutRect, LayoutSize, LayoutTransform};
use {LayoutVector2D, LineDisplayItem, LineOrientation, LineStyle, MixBlendMode, PipelineId};
use {PropertyBinding, PushStackingContextDisplayItem, RadialGradient, RadialGradientDisplayItem};
use {RectangleDisplayItem, ScrollFrameDisplayItem, ScrollSensitivity, Shadow, SpecificDisplayItem};
use {StackingContext, StickyFrameDisplayItem, StickyOffsetBounds, TextDisplayItem, TransformStyle};
use {YuvColorSpace, YuvData, YuvImageDisplayItem};

// We don't want to push a long text-run. If a text-run is too long, split it into several parts.
// This needs to be set to (renderer::MAX_VERTEX_TEXTURE_WIDTH - VECS_PER_PRIM_HEADER - VECS_PER_TEXT_RUN) * 2
pub const MAX_TEXT_RUN_LENGTH: usize = 2038;

// We start at 2, because the root reference is always 0 and the root scroll node is always 1.
const FIRST_CLIP_ID: usize = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange<T> {
    start: usize,
    length: usize,
    _boo: PhantomData<T>,
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
    /// The amount of clips ids assigned while building this display list.
    total_clip_ids: usize,
}

pub struct BuiltDisplayListIter<'a> {
    list: &'a BuiltDisplayList,
    data: &'a [u8],
    cur_item: DisplayItem,
    cur_stops: ItemRange<GradientStop>,
    cur_glyphs: ItemRange<GlyphInstance>,
    cur_filters: ItemRange<FilterOp>,
    cur_clip_chain_items: ItemRange<ClipId>,
    cur_complex_clip: (ItemRange<ComplexClipRegion>, usize),
    peeking: Peek,
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
    pub fn from_data(data: Vec<u8>, descriptor: BuiltDisplayListDescriptor) -> BuiltDisplayList {
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

    pub fn total_clip_ids(&self) -> usize {
        self.descriptor.total_clip_ids
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
            cur_item: DisplayItem {
                // Dummy data, will be overwritten by `next`
                item: SpecificDisplayItem::PopStackingContext,
                clip_and_scroll:
                    ClipAndScrollInfo::simple(ClipId::root_scroll_node(PipelineId::dummy())),
                info: LayoutPrimitiveInfo::new(LayoutRect::zero()),
            },
            cur_stops: ItemRange::default(),
            cur_glyphs: ItemRange::default(),
            cur_filters: ItemRange::default(),
            cur_clip_chain_items: ItemRange::default(),
            cur_complex_clip: (ItemRange::default(), 0),
            peeking: Peek::NotPeeking,
        }
    }

    pub fn display_list(&self) -> &'a BuiltDisplayList {
        self.list
    }

    pub fn next<'b>(&'b mut self) -> Option<DisplayItemRef<'a, 'b>> {
        use SpecificDisplayItem::*;

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
            if let SetGradientStops = self.cur_item.item {
                // SetGradientStops is a dummy item that most consumers should ignore
                continue;
            }
            break;
        }

        Some(self.as_ref())
    }

    /// Gets the next display item, even if it's a dummy. Also doesn't handle peeking
    /// and may leave irrelevant ranges live (so a Clip may have GradientStops if
    /// for some reason you ask).
    pub fn next_raw<'b>(&'b mut self) -> Option<DisplayItemRef<'a, 'b>> {
        use SpecificDisplayItem::*;

        if self.data.is_empty() {
            return None;
        }

        {
            let reader = bincode::IoReader::new(UnsafeReader::new(&mut self.data));
            bincode::deserialize_in_place(reader, &mut self.cur_item)
                .expect("MEH: malicious process?");
        }

        match self.cur_item.item {
            SetGradientStops => {
                self.cur_stops = skip_slice::<GradientStop>(self.list, &mut self.data).0;
            }
            ClipChain(_) => {
                self.cur_clip_chain_items = skip_slice::<ClipId>(self.list, &mut self.data).0;
            }
            Clip(_) | ScrollFrame(_) => {
                self.cur_complex_clip = self.skip_slice::<ComplexClipRegion>()
            }
            Text(_) => self.cur_glyphs = self.skip_slice::<GlyphInstance>().0,
            PushStackingContext(_) => self.cur_filters = self.skip_slice::<FilterOp>().0,
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

    pub fn starting_stacking_context(
        &mut self,
    ) -> Option<(StackingContext, LayoutRect, ItemRange<FilterOp>)> {
        self.next().and_then(|item| match *item.item() {
            SpecificDisplayItem::PushStackingContext(ref specific_item) => Some((
                specific_item.stacking_context,
                item.rect(),
                item.filters(),
            )),
            _ => None,
        })
    }

    pub fn skip_current_stacking_context(&mut self) {
        let mut depth = 0;
        while let Some(item) = self.next() {
            match *item.item() {
                SpecificDisplayItem::PushStackingContext(..) => depth += 1,
                SpecificDisplayItem::PopStackingContext if depth == 0 => return,
                SpecificDisplayItem::PopStackingContext => depth -= 1,
                _ => {}
            }
            debug_assert!(depth >= 0);
        }
    }

    pub fn current_stacking_context_empty(&mut self) -> bool {
        match self.peek() {
            Some(item) => *item.item() == SpecificDisplayItem::PopStackingContext,
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
}

// Some of these might just become ItemRanges
impl<'a, 'b> DisplayItemRef<'a, 'b> {
    pub fn display_item(&self) -> &DisplayItem {
        &self.iter.cur_item
    }

    pub fn rect(&self) -> LayoutRect {
        self.iter.cur_item.info.rect
    }

    pub fn get_layout_primitive_info(&self, offset: &LayoutVector2D) -> LayoutPrimitiveInfo {
        let info = self.iter.cur_item.info;
        LayoutPrimitiveInfo {
            rect: info.rect.translate(offset),
            clip_rect: info.clip_rect.translate(offset),
            is_backface_visible: info.is_backface_visible,
            tag: info.tag,
        }
    }

    pub fn clip_rect(&self) -> &LayoutRect {
        &self.iter.cur_item.info.clip_rect
    }

    pub fn clip_and_scroll(&self) -> ClipAndScrollInfo {
        self.iter.cur_item.clip_and_scroll
    }

    pub fn item(&self) -> &SpecificDisplayItem {
        &self.iter.cur_item.item
    }

    pub fn complex_clip(&self) -> (ItemRange<ComplexClipRegion>, usize) {
        self.iter.cur_complex_clip
    }

    pub fn gradient_stops(&self) -> ItemRange<GradientStop> {
        self.iter.cur_stops
    }

    pub fn glyphs(&self) -> ItemRange<GlyphInstance> {
        self.iter.cur_glyphs
    }

    pub fn filters(&self) -> ItemRange<FilterOp> {
        self.iter.cur_filters
    }

    pub fn clip_chain_items(&self) -> ItemRange<ClipId> {
        self.iter.cur_clip_chain_items
    }

    pub fn display_list(&self) -> &BuiltDisplayList {
        self.iter.display_list()
    }

    pub fn is_backface_visible(&self) -> bool {
        self.iter.cur_item.info.is_backface_visible
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
        use display_item::CompletelySpecificDisplayItem::*;
        use display_item::GenericDisplayItem;

        let mut seq = serializer.serialize_seq(None)?;
        let mut traversal = self.iter();
        while let Some(item) = traversal.next_raw() {
            let display_item = item.display_item();
            let serial_di = GenericDisplayItem {
                item: match display_item.item {
                    SpecificDisplayItem::Clip(v) => Clip(
                        v,
                        item.iter.list.get(item.iter.cur_complex_clip.0).collect()
                    ),
                    SpecificDisplayItem::ClipChain(v) => ClipChain(
                        v,
                        item.iter.list.get(item.iter.cur_clip_chain_items).collect(),
                    ),
                    SpecificDisplayItem::ScrollFrame(v) => ScrollFrame(
                        v,
                        item.iter.list.get(item.iter.cur_complex_clip.0).collect()
                    ),
                    SpecificDisplayItem::StickyFrame(v) => StickyFrame(v),
                    SpecificDisplayItem::Rectangle(v) => Rectangle(v),
                    SpecificDisplayItem::ClearRectangle => ClearRectangle,
                    SpecificDisplayItem::Line(v) => Line(v),
                    SpecificDisplayItem::Text(v) => Text(
                        v,
                        item.iter.list.get(item.iter.cur_glyphs).collect()
                    ),
                    SpecificDisplayItem::Image(v) => Image(v),
                    SpecificDisplayItem::YuvImage(v) => YuvImage(v),
                    SpecificDisplayItem::Border(v) => Border(v),
                    SpecificDisplayItem::BoxShadow(v) => BoxShadow(v),
                    SpecificDisplayItem::Gradient(v) => Gradient(v),
                    SpecificDisplayItem::RadialGradient(v) => RadialGradient(v),
                    SpecificDisplayItem::Iframe(v) => Iframe(v),
                    SpecificDisplayItem::PushStackingContext(v) => PushStackingContext(
                        v,
                        item.iter.list.get(item.iter.cur_filters).collect()
                    ),
                    SpecificDisplayItem::PopStackingContext => PopStackingContext,
                    SpecificDisplayItem::SetGradientStops => SetGradientStops(
                        item.iter.list.get(item.iter.cur_stops).collect()
                    ),
                    SpecificDisplayItem::PushShadow(v) => PushShadow(v),
                    SpecificDisplayItem::PopAllShadows => PopAllShadows,
                },
                clip_and_scroll: display_item.clip_and_scroll,
                info: display_item.info,
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
        use display_item::CompletelySpecificDisplayItem::*;
        use display_item::{CompletelySpecificDisplayItem, GenericDisplayItem};

        let list = Vec::<GenericDisplayItem<CompletelySpecificDisplayItem>>
            ::deserialize(deserializer)?;

        let mut data = Vec::new();
        let mut temp = Vec::new();
        let mut total_clip_ids = FIRST_CLIP_ID;
        for complete in list {
            let item = DisplayItem {
                item: match complete.item {
                    Clip(specific_item, complex_clips) => {
                        total_clip_ids += 1;
                        DisplayListBuilder::push_iter_impl(&mut temp, complex_clips);
                        SpecificDisplayItem::Clip(specific_item)
                    },
                    ClipChain(specific_item, clip_chain_ids) => {
                        DisplayListBuilder::push_iter_impl(&mut temp, clip_chain_ids);
                        SpecificDisplayItem::ClipChain(specific_item)
                    }
                    ScrollFrame(specific_item, complex_clips) => {
                        total_clip_ids += 2;
                        DisplayListBuilder::push_iter_impl(&mut temp, complex_clips);
                        SpecificDisplayItem::ScrollFrame(specific_item)
                    },
                    StickyFrame(specific_item) => {
                        total_clip_ids += 1;
                        SpecificDisplayItem::StickyFrame(specific_item)
                    }
                    Rectangle(specific_item) => SpecificDisplayItem::Rectangle(specific_item),
                    ClearRectangle => SpecificDisplayItem::ClearRectangle,
                    Line(specific_item) => SpecificDisplayItem::Line(specific_item),
                    Text(specific_item, glyphs) => {
                        DisplayListBuilder::push_iter_impl(&mut temp, glyphs);
                        SpecificDisplayItem::Text(specific_item)
                    },
                    Image(specific_item) => SpecificDisplayItem::Image(specific_item),
                    YuvImage(specific_item) => SpecificDisplayItem::YuvImage(specific_item),
                    Border(specific_item) => SpecificDisplayItem::Border(specific_item),
                    BoxShadow(specific_item) => SpecificDisplayItem::BoxShadow(specific_item),
                    Gradient(specific_item) => SpecificDisplayItem::Gradient(specific_item),
                    RadialGradient(specific_item) =>
                        SpecificDisplayItem::RadialGradient(specific_item),
                    Iframe(specific_item) => {
                        total_clip_ids += 1;
                        SpecificDisplayItem::Iframe(specific_item)
                    }
                    PushStackingContext(specific_item, filters) => {
                        if specific_item.stacking_context.reference_frame_id.is_some() {
                            total_clip_ids += 1;
                        }
                        DisplayListBuilder::push_iter_impl(&mut temp, filters);
                        SpecificDisplayItem::PushStackingContext(specific_item)
                    },
                    PopStackingContext => SpecificDisplayItem::PopStackingContext,
                    SetGradientStops(stops) => {
                        DisplayListBuilder::push_iter_impl(&mut temp, stops);
                        SpecificDisplayItem::SetGradientStops
                    },
                    PushShadow(specific_item) => SpecificDisplayItem::PushShadow(specific_item),
                    PopAllShadows => SpecificDisplayItem::PopAllShadows,
                },
                clip_and_scroll: complete.clip_and_scroll,
                info: complete.info,
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
                total_clip_ids,
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
            self.0 = self.0.offset(buf.len() as isize);
        }
        Ok(buf.len())
    }

    #[inline(always)]
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        unsafe {
            ptr::copy_nonoverlapping(buf.as_ptr(), self.0, buf.len());
            self.0 = self.0.offset(buf.len() as isize);
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
fn serialize_fast<T: Serialize>(vec: &mut Vec<u8>, e: &T) {
    // manually counting the size is faster than vec.reserve(bincode::serialized_size(&e) as usize) for some reason
    let mut size = SizeCounter(0);
    bincode::serialize_into(&mut size, e).unwrap();
    vec.reserve(size.0);

    let old_len = vec.len();
    let ptr = unsafe { vec.as_mut_ptr().offset(old_len as isize) };
    let mut w = UnsafeVecWriter(ptr);
    bincode::serialize_into(&mut w, e).unwrap();

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
/// If the first is incorrect, webrender will be very slow. If the other two are
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
    let ptr = unsafe { vec.as_mut_ptr().offset(old_len as isize) };
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
            let end = buf.as_ptr().offset(buf.len() as isize);
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
            assert!(self.start.offset(buf.len() as isize) <= self.end, "UnsafeReader: read past end of target");
            ptr::copy_nonoverlapping(self.start, buf.as_mut_ptr(), buf.len());
            self.start = self.start.offset(buf.len() as isize);
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
    clip_stack_len: usize,
    next_clip_id: usize,
    next_clip_chain_id: u64,
}

#[derive(Clone)]
pub struct DisplayListBuilder {
    pub data: Vec<u8>,
    pub pipeline_id: PipelineId,
    clip_stack: Vec<ClipAndScrollInfo>,
    next_clip_id: usize,
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
            clip_stack: vec![
                ClipAndScrollInfo::simple(ClipId::root_scroll_node(pipeline_id)),
            ],
            next_clip_id: FIRST_CLIP_ID,
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
            clip_stack_len: self.clip_stack.len(),
            dl_len: self.data.len(),
            next_clip_id: self.next_clip_id,
            next_clip_chain_id: self.next_clip_chain_id,
        });
    }

    /// Restores the state of the builder to when `save()` was last called.
    pub fn restore(&mut self) {
        let state = self.save_state.take().expect("No save to restore DisplayListBuilder from");

        self.clip_stack.truncate(state.clip_stack_len);
        self.data.truncate(state.dl_len);
        self.next_clip_id = state.next_clip_id;
        self.next_clip_chain_id = state.next_clip_chain_id;
    }

    /// Discards the builder's save (indicating the attempted operation was successful).
    pub fn clear_save(&mut self) {
        self.save_state.take().expect("No save to clear in DisplayListBuilder");
    }

    pub fn print_display_list(&mut self) {
        let mut temp = BuiltDisplayList::default();
        mem::swap(&mut temp.data, &mut self.data);

        {
            let mut iter = BuiltDisplayListIter::new(&temp);
            while let Some(item) = iter.next_raw() {
                println!("{:?}", item.display_item());
            }
        }

        self.data = temp.data;
    }

    fn push_item(&mut self, item: SpecificDisplayItem, info: &LayoutPrimitiveInfo) {
        serialize_fast(
            &mut self.data,
            &DisplayItem {
                item,
                clip_and_scroll: *self.clip_stack.last().unwrap(),
                info: *info,
            },
        )
    }

    fn push_item_with_clip_scroll_info(
        &mut self,
        item: SpecificDisplayItem,
        info: &LayoutPrimitiveInfo,
        scrollinfo: ClipAndScrollInfo
    ) {
        serialize_fast(
            &mut self.data,
            &DisplayItem {
                item,
                clip_and_scroll: scrollinfo,
                info: *info,
            },
        )
    }

    fn push_new_empty_item(&mut self, item: SpecificDisplayItem) {
        let info = LayoutPrimitiveInfo::new(LayoutRect::zero());
        serialize_fast(
            &mut self.data,
            &DisplayItem {
                item,
                clip_and_scroll: *self.clip_stack.last().unwrap(),
                info,
            }
        )
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

    fn push_iter<I>(&mut self, iter: I)
    where
        I: IntoIterator,
        I::IntoIter: ExactSizeIterator + Clone,
        I::Item: Serialize,
    {
        Self::push_iter_impl(&mut self.data, iter);
    }

    pub fn push_rect(&mut self, info: &LayoutPrimitiveInfo, color: ColorF) {
        let item = SpecificDisplayItem::Rectangle(RectangleDisplayItem { color });
        self.push_item(item, info);
    }

    pub fn push_clear_rect(&mut self, info: &LayoutPrimitiveInfo) {
        self.push_item(SpecificDisplayItem::ClearRectangle, info);
    }

    pub fn push_line(
        &mut self,
        info: &LayoutPrimitiveInfo,
        wavy_line_thickness: f32,
        orientation: LineOrientation,
        color: &ColorF,
        style: LineStyle,
    ) {
        let item = SpecificDisplayItem::Line(LineDisplayItem {
            wavy_line_thickness,
            orientation,
            color: *color,
            style,
        });

        self.push_item(item, info);
    }

    pub fn push_image(
        &mut self,
        info: &LayoutPrimitiveInfo,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        image_rendering: ImageRendering,
        alpha_type: AlphaType,
        key: ImageKey,
    ) {
        let item = SpecificDisplayItem::Image(ImageDisplayItem {
            image_key: key,
            stretch_size,
            tile_spacing,
            image_rendering,
            alpha_type,
        });

        self.push_item(item, info);
    }

    /// Push a yuv image. All planar data in yuv image should use the same buffer type.
    pub fn push_yuv_image(
        &mut self,
        info: &LayoutPrimitiveInfo,
        yuv_data: YuvData,
        color_space: YuvColorSpace,
        image_rendering: ImageRendering,
    ) {
        let item = SpecificDisplayItem::YuvImage(YuvImageDisplayItem {
            yuv_data,
            color_space,
            image_rendering,
        });
        self.push_item(item, info);
    }

    pub fn push_text(
        &mut self,
        info: &LayoutPrimitiveInfo,
        glyphs: &[GlyphInstance],
        font_key: FontInstanceKey,
        color: ColorF,
        glyph_options: Option<GlyphOptions>,
    ) {
        let item = SpecificDisplayItem::Text(TextDisplayItem {
            color,
            font_key,
            glyph_options,
        });

        for split_glyphs in glyphs.chunks(MAX_TEXT_RUN_LENGTH) {
            self.push_item(item, info);
            self.push_iter(split_glyphs);
        }
    }

    // Gradients can be defined with stops outside the range of [0, 1]
    // when this happens the gradient needs to be normalized by adjusting
    // the gradient stops and gradient line into an equivalent gradient
    // with stops in the range [0, 1]. this is done by moving the beginning
    // of the gradient line to where stop[0] and the end of the gradient line
    // to stop[n-1]. this function adjusts the stops in place, and returns
    // the amount to adjust the gradient line start and stop
    fn normalize_stops(stops: &mut Vec<GradientStop>, extend_mode: ExtendMode) -> (f32, f32) {
        assert!(stops.len() >= 2);

        let first = *stops.first().unwrap();
        let last = *stops.last().unwrap();

        assert!(first.offset <= last.offset);

        let stops_delta = last.offset - first.offset;

        if stops_delta > 0.000001 {
            for stop in stops {
                stop.offset = (stop.offset - first.offset) / stops_delta;
            }

            (first.offset, last.offset)
        } else {
            // We have a degenerate gradient and can't accurately transform the stops
            // what happens here depends on the repeat behavior, but in any case
            // we reconstruct the gradient stops to something simpler and equivalent
            stops.clear();

            match extend_mode {
                ExtendMode::Clamp => {
                    // This gradient is two colors split at the offset of the stops,
                    // so create a gradient with two colors split at 0.5 and adjust
                    // the gradient line so 0.5 is at the offset of the stops
                    stops.push(GradientStop { color: first.color, offset: 0.0, });
                    stops.push(GradientStop { color: first.color, offset: 0.5, });
                    stops.push(GradientStop { color: last.color, offset: 0.5, });
                    stops.push(GradientStop { color: last.color, offset: 1.0, });

                    let offset = last.offset;

                    (offset - 0.5, offset + 0.5)
                }
                ExtendMode::Repeat => {
                    // A repeating gradient with stops that are all in the same
                    // position should just display the last color. I believe the
                    // spec says that it should be the average color of the gradient,
                    // but this matches what Gecko and Blink does
                    stops.push(GradientStop { color: last.color, offset: 0.0, });
                    stops.push(GradientStop { color: last.color, offset: 1.0, });

                    (0.0, 1.0)
                }
            }
        }
    }

    // NOTE: gradients must be pushed in the order they're created
    // because create_gradient stores the stops in anticipation
    pub fn create_gradient(
        &mut self,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        mut stops: Vec<GradientStop>,
        extend_mode: ExtendMode,
    ) -> Gradient {
        let (start_offset, end_offset) =
            DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        let start_to_end = end_point - start_point;

        self.push_stops(&stops);

        Gradient {
            start_point: start_point + start_to_end * start_offset,
            end_point: start_point + start_to_end * end_offset,
            extend_mode,
        }
    }

    // NOTE: gradients must be pushed in the order they're created
    // because create_gradient stores the stops in anticipation
    pub fn create_radial_gradient(
        &mut self,
        center: LayoutPoint,
        radius: LayoutSize,
        mut stops: Vec<GradientStop>,
        extend_mode: ExtendMode,
    ) -> RadialGradient {
        if radius.width <= 0.0 || radius.height <= 0.0 {
            // The shader cannot handle a non positive radius. So
            // reuse the stops vector and construct an equivalent
            // gradient.
            let last_color = stops.last().unwrap().color;

            let stops = [
                GradientStop { offset: 0.0, color: last_color, },
                GradientStop { offset: 1.0, color: last_color, },
            ];

            self.push_stops(&stops);

            return RadialGradient {
                center,
                radius: LayoutSize::new(1.0, 1.0),
                start_offset: 0.0,
                end_offset: 1.0,
                extend_mode,
            };
        }

        let (start_offset, end_offset) =
            DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        self.push_stops(&stops);

        RadialGradient {
            center,
            radius,
            start_offset,
            end_offset,
            extend_mode,
        }
    }

    pub fn push_border(
        &mut self,
        info: &LayoutPrimitiveInfo,
        widths: BorderWidths,
        details: BorderDetails,
    ) {
        let item = SpecificDisplayItem::Border(BorderDisplayItem { details, widths });

        self.push_item(item, info);
    }

    pub fn push_box_shadow(
        &mut self,
        info: &LayoutPrimitiveInfo,
        box_bounds: LayoutRect,
        offset: LayoutVector2D,
        color: ColorF,
        blur_radius: f32,
        spread_radius: f32,
        border_radius: BorderRadius,
        clip_mode: BoxShadowClipMode,
    ) {
        let item = SpecificDisplayItem::BoxShadow(BoxShadowDisplayItem {
            box_bounds,
            offset,
            color,
            blur_radius,
            spread_radius,
            border_radius,
            clip_mode,
        });

        self.push_item(item, info);
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
    /// The origin of the tiles is given in `info.rect.origin`.
    /// If the gradient should only be displayed once limit
    /// the `info.rect.size` to a single tile.
    /// The gradient is only visible within the local clip.
    pub fn push_gradient(
        &mut self,
        info: &LayoutPrimitiveInfo,
        gradient: Gradient,
        tile_size: LayoutSize,
        tile_spacing: LayoutSize,
    ) {
        let item = SpecificDisplayItem::Gradient(GradientDisplayItem {
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(item, info);
    }

    /// Pushes a radial gradient to be displayed.
    ///
    /// See [`push_gradient`](#method.push_gradient) for explanation.
    pub fn push_radial_gradient(
        &mut self,
        info: &LayoutPrimitiveInfo,
        gradient: RadialGradient,
        tile_size: LayoutSize,
        tile_spacing: LayoutSize,
    ) {
        let item = SpecificDisplayItem::RadialGradient(RadialGradientDisplayItem {
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(item, info);
    }

    pub fn push_stacking_context(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_node_id: Option<ClipId>,
        transform: Option<PropertyBinding<LayoutTransform>>,
        transform_style: TransformStyle,
        perspective: Option<LayoutTransform>,
        mix_blend_mode: MixBlendMode,
        filters: Vec<FilterOp>,
        glyph_raster_space: GlyphRasterSpace,
    ) -> Option<ClipId> {
        let reference_frame_id = if transform.is_some() || perspective.is_some() {
            Some(self.generate_clip_id())
        } else {
            None
        };

        let item = SpecificDisplayItem::PushStackingContext(PushStackingContextDisplayItem {
            stacking_context: StackingContext {
                transform,
                transform_style,
                perspective,
                mix_blend_mode,
                reference_frame_id,
                clip_node_id,
                glyph_raster_space,
            },
        });

        self.push_item(item, info);
        self.push_iter(&filters);

        reference_frame_id
    }

    pub fn pop_stacking_context(&mut self) {
        self.push_new_empty_item(SpecificDisplayItem::PopStackingContext);
    }

    pub fn push_stops(&mut self, stops: &[GradientStop]) {
        if stops.is_empty() {
            return;
        }
        self.push_new_empty_item(SpecificDisplayItem::SetGradientStops);
        self.push_iter(stops);
    }

    fn generate_clip_id(&mut self) -> ClipId {
        self.next_clip_id += 1;
        ClipId::Clip(self.next_clip_id - 1, self.pipeline_id)
    }

    fn generate_clip_chain_id(&mut self) -> ClipChainId {
        self.next_clip_chain_id += 1;
        ClipChainId(self.next_clip_chain_id - 1, self.pipeline_id)
    }

    pub fn define_scroll_frame<I>(
        &mut self,
        external_id: Option<ExternalScrollId>,
        content_rect: LayoutRect,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<ImageMask>,
        scroll_sensitivity: ScrollSensitivity,
    ) -> ClipId
    where
        I: IntoIterator<Item = ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let parent = self.clip_stack.last().unwrap().scroll_node_id;
        self.define_scroll_frame_with_parent(
            parent,
            external_id,
            content_rect,
            clip_rect,
            complex_clips,
            image_mask,
            scroll_sensitivity)
    }

    pub fn define_scroll_frame_with_parent<I>(
        &mut self,
        parent: ClipId,
        external_id: Option<ExternalScrollId>,
        content_rect: LayoutRect,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<ImageMask>,
        scroll_sensitivity: ScrollSensitivity,
    ) -> ClipId
    where
        I: IntoIterator<Item = ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let clip_id = self.generate_clip_id();
        let scroll_frame_id = self.generate_clip_id();
        let item = SpecificDisplayItem::ScrollFrame(ScrollFrameDisplayItem {
            clip_id,
            scroll_frame_id,
            external_id,
            image_mask,
            scroll_sensitivity,
        });

        self.push_item_with_clip_scroll_info(
            item,
            &LayoutPrimitiveInfo::with_clip_rect(content_rect, clip_rect),
            ClipAndScrollInfo::simple(parent),
        );
        self.push_iter(complex_clips);

        scroll_frame_id
    }

    pub fn define_clip_chain<I>(
        &mut self,
        parent: Option<ClipChainId>,
        clips: I,
    ) -> ClipChainId
    where
        I: IntoIterator<Item = ClipId>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let id = self.generate_clip_chain_id();
        self.push_new_empty_item(SpecificDisplayItem::ClipChain(ClipChainItem { id, parent }));
        self.push_iter(clips);
        id
    }

    pub fn define_clip<I>(
        &mut self,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<ImageMask>,
    ) -> ClipId
    where
        I: IntoIterator<Item = ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let parent = self.clip_stack.last().unwrap().scroll_node_id;
        self.define_clip_with_parent(
            parent,
            clip_rect,
            complex_clips,
            image_mask
        )
    }

    pub fn define_clip_with_parent<I>(
        &mut self,
        parent: ClipId,
        clip_rect: LayoutRect,
        complex_clips: I,
        image_mask: Option<ImageMask>,
    ) -> ClipId
    where
        I: IntoIterator<Item = ComplexClipRegion>,
        I::IntoIter: ExactSizeIterator + Clone,
    {
        let id = self.generate_clip_id();
        let item = SpecificDisplayItem::Clip(ClipDisplayItem {
            id,
            image_mask,
        });

        let info = LayoutPrimitiveInfo::new(clip_rect);

        let scrollinfo = ClipAndScrollInfo::simple(parent);
        self.push_item_with_clip_scroll_info(item, &info, scrollinfo);
        self.push_iter(complex_clips);
        id
    }

    pub fn define_sticky_frame(
        &mut self,
        frame_rect: LayoutRect,
        margins: SideOffsets2D<Option<f32>>,
        vertical_offset_bounds: StickyOffsetBounds,
        horizontal_offset_bounds: StickyOffsetBounds,
        previously_applied_offset: LayoutVector2D,

    ) -> ClipId {
        let id = self.generate_clip_id();
        let item = SpecificDisplayItem::StickyFrame(StickyFrameDisplayItem {
            id,
            margins,
            vertical_offset_bounds,
            horizontal_offset_bounds,
            previously_applied_offset,
        });

        let info = LayoutPrimitiveInfo::new(frame_rect);
        self.push_item(item, &info);
        id
    }

    pub fn push_clip_id(&mut self, id: ClipId) {
        self.clip_stack.push(ClipAndScrollInfo::simple(id));
    }

    pub fn push_clip_and_scroll_info(&mut self, info: ClipAndScrollInfo) {
        self.clip_stack.push(info);
    }

    pub fn pop_clip_id(&mut self) {
        self.clip_stack.pop();
        if let Some(save_state) = self.save_state.as_ref() {
            assert!(self.clip_stack.len() >= save_state.clip_stack_len,
                    "Cannot pop clips that were pushed before the DisplayListBuilder save.");
        }
        assert!(!self.clip_stack.is_empty());
    }

    pub fn push_iframe(&mut self, info: &LayoutPrimitiveInfo, pipeline_id: PipelineId) {
        let item = SpecificDisplayItem::Iframe(IframeDisplayItem {
            clip_id: self.generate_clip_id(),
            pipeline_id,
        });
        self.push_item(item, info);
    }

    pub fn push_shadow(&mut self, info: &LayoutPrimitiveInfo, shadow: Shadow) {
        self.push_item(SpecificDisplayItem::PushShadow(shadow), info);
    }

    pub fn pop_all_shadows(&mut self) {
        self.push_new_empty_item(SpecificDisplayItem::PopAllShadows);
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
                    total_clip_ids: self.next_clip_id,
                },
                data: self.data,
            },
        )
    }
}
