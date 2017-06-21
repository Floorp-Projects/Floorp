/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use bincode;
use serde::{Deserialize, Serialize, Serializer};
use serde::ser::{SerializeSeq, SerializeMap};
use time::precise_time_ns;
use {BorderDetails, BorderDisplayItem, BorderWidths, BoxShadowClipMode, BoxShadowDisplayItem};
use {ClipAndScrollInfo, ClipDisplayItem, ClipId, ClipRegion, ColorF, ComplexClipRegion};
use {DisplayItem, ExtendMode, FilterOp, FontKey, GlyphInstance, GlyphOptions, Gradient};
use {GradientDisplayItem, GradientStop, IframeDisplayItem, ImageDisplayItem, ImageKey, ImageMask};
use {ImageRendering, LayoutPoint, LayoutRect, LayoutSize, LayoutTransform, MixBlendMode};
use {PipelineId, PropertyBinding, PushStackingContextDisplayItem, RadialGradient};
use {RadialGradientDisplayItem, RectangleDisplayItem, ScrollPolicy, SpecificDisplayItem};
use {StackingContext, TextDisplayItem, TransformStyle, WebGLContextId, WebGLDisplayItem};
use {YuvColorSpace, YuvData, YuvImageDisplayItem, LayoutVector2D};
use std::marker::PhantomData;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange<T> {
    start: usize,
    length: usize,
    _boo: PhantomData<T>,
}

impl<T> Default for ItemRange<T> {
    fn default() -> Self {
        ItemRange { start: 0, length: 0, _boo: PhantomData }
    }
}

impl<T> ItemRange<T> {
    pub fn is_empty(&self) -> bool {
        // Nothing more than space for a length (0).
        self.length <= ::std::mem::size_of::<u64>()
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
}

pub struct BuiltDisplayListIter<'a> {
    list: &'a BuiltDisplayList,
    data: &'a [u8],
    cur_item: DisplayItem,
    cur_stops: ItemRange<GradientStop>,
    cur_glyphs: ItemRange<GlyphInstance>,
    cur_filters: ItemRange<FilterOp>,
    cur_clip: ClipRegion,
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

impl BuiltDisplayListDescriptor {
}

impl BuiltDisplayList {
    pub fn from_data(data: Vec<u8>, descriptor: BuiltDisplayListDescriptor) -> BuiltDisplayList {
        BuiltDisplayList {
            data: data,
            descriptor: descriptor,
        }
    }

    pub fn into_data(self) -> (Vec<u8>, BuiltDisplayListDescriptor) {
        (self.data, self.descriptor)
    }

    pub fn data(&self) -> &[u8] {
        &self.data[..]
    }

    pub fn descriptor(&self) -> &BuiltDisplayListDescriptor {
        &self.descriptor
    }

    pub fn times(&self) -> (u64, u64) {
      (self.descriptor.builder_start_time, self.descriptor.builder_finish_time)
    }

    pub fn iter(&self) -> BuiltDisplayListIter {
        BuiltDisplayListIter::new(self)
    }

    pub fn get<'de, T: Deserialize<'de>>(&self, range: ItemRange<T>) -> AuxIter<T> {
        AuxIter::new(&self.data[range.start .. range.start + range.length])
    }
}

impl<'a> BuiltDisplayListIter<'a> {
    pub fn new(list: &'a BuiltDisplayList) -> Self {
        BuiltDisplayListIter {
            list: list,
            data: &list.data,
            // Dummy data, will be overwritten by `next`
            cur_item: DisplayItem {
                item: SpecificDisplayItem::PopStackingContext,
                rect: LayoutRect::zero(),
                clip_and_scroll: ClipAndScrollInfo::simple(ClipId::new(0, PipelineId(0, 0))),
            },
            cur_stops: ItemRange::default(),
            cur_glyphs: ItemRange::default(),
            cur_filters: ItemRange::default(),
            cur_clip: ClipRegion::empty(),
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
                return Some(self.as_ref())
            }
            Peek::StartPeeking => {
                self.peeking = Peek::IsPeeking;
            }
            Peek::NotPeeking => { /* do nothing */ }
        }

        // Don't let these bleed into another item
        self.cur_stops = ItemRange::default();
        self.cur_clip = ClipRegion::empty();

        loop {
            if self.data.len() == 0 {
                return None
            }

            self.cur_item = bincode::deserialize_from(&mut self.data, bincode::Infinite)
                                    .expect("MEH: malicious process?");

            match self.cur_item.item {
                SetClipRegion(clip) => {
                    self.cur_clip = clip;
                    let (clip_range, clip_count) = self.skip_slice::<ComplexClipRegion>();
                    self.cur_clip.complex_clip_count = clip_count;
                    self.cur_clip.complex_clips = clip_range;

                    // This is a dummy item, skip over it
                    continue;
                }
                SetGradientStops => {
                    self.cur_stops = self.skip_slice::<GradientStop>().0;

                    // This is a dummy item, skip over it
                    continue;
                }
                Text(_) => {
                    self.cur_glyphs = self.skip_slice::<GlyphInstance>().0;
                }
                PushStackingContext(_) => {
                    self.cur_filters = self.skip_slice::<FilterOp>().0;
                }
                _ => { /* do nothing */ }
            }

            break;
        }

        Some(self.as_ref())
    }

    /// Returns the byte-range the slice occupied, and the number of elements
    /// in the slice.
    fn skip_slice<T: for<'de> Deserialize<'de>>(&mut self) -> (ItemRange<T>, usize) {
        let base = self.list.data.as_ptr() as usize;
        let start = self.data.as_ptr() as usize;

        // Read through the values (this is a bit of a hack to reuse logic)
        let mut iter = AuxIter::<T>::new(self.data);
        let count = iter.len();
        for _ in &mut iter {}
        let end = iter.data.as_ptr() as usize;

        let range = ItemRange { start: start - base, length: end - start, _boo: PhantomData };

        // Adjust data pointer to skip read values
        self.data = &self.data[range.length..];
        (range, count)
    }

    pub fn as_ref<'b>(&'b self) -> DisplayItemRef<'a, 'b> {
        DisplayItemRef { iter: self }
    }

    pub fn starting_stacking_context(&mut self)
        -> Option<(StackingContext, LayoutRect, ItemRange<FilterOp>)> {

        self.next().and_then(|item| match *item.item() {
            SpecificDisplayItem::PushStackingContext(ref specific_item) => {
                Some((specific_item.stacking_context, item.rect(), item.filters()))
            },
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
        self.iter.cur_item.rect
    }

    pub fn clip_and_scroll(&self) -> ClipAndScrollInfo {
        self.iter.cur_item.clip_and_scroll
    }

    pub fn item(&self) -> &SpecificDisplayItem {
        &self.iter.cur_item.item
    }

    pub fn clip_region(&self) -> &ClipRegion {
        &self.iter.cur_clip
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

    pub fn display_list(&self) -> &BuiltDisplayList {
        self.iter.display_list()
    }

    // Creates a new iterator where this element's iterator
    // is, to hack around borrowck.
    pub fn sub_iter(&self) -> BuiltDisplayListIter<'a> {
        BuiltDisplayListIter {
            list: self.iter.list,
            data: self.iter.data,
            // Dummy data, will be overwritten by `next`
            cur_item: DisplayItem {
                item: SpecificDisplayItem::PopStackingContext,
                rect: LayoutRect::zero(),
                clip_and_scroll: ClipAndScrollInfo::simple(ClipId::new(0, PipelineId(0, 0))),
            },
            cur_stops: ItemRange::default(),
            cur_glyphs: ItemRange::default(),
            cur_filters: ItemRange::default(),
            cur_clip: ClipRegion::empty(),
            peeking: Peek::NotPeeking,
        }
    }
}

impl<'de, 'a, T: Deserialize<'de>> AuxIter<'a, T> {
    pub fn new(mut data: &'a [u8]) -> Self {

        let size: usize = if data.len() == 0 {
            0   // Accept empty ItemRanges pointing anywhere
        } else {
            bincode::deserialize_from(&mut data, bincode::Infinite)
                                  .expect("MEH: malicious input?")
        };

        AuxIter {
            data: data,
            size: size,
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
            Some(bincode::deserialize_from(&mut self.data, bincode::Infinite)
                         .expect("MEH: malicious input?"))
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.size, Some(self.size))
    }
}

impl<'a, T: for<'de> Deserialize<'de>> ::std::iter::ExactSizeIterator for AuxIter<'a, T> { }


// This is purely for the JSON writer in wrench
impl Serialize for BuiltDisplayList {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let mut seq = serializer.serialize_seq(None)?;
        let mut traversal = self.iter();
        while let Some(item) = traversal.next() {
            seq.serialize_element(&item)?
        }
        seq.end()
    }
}

impl<'a, 'b> Serialize for DisplayItemRef<'a, 'b> {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let mut map = serializer.serialize_map(None)?;

        map.serialize_entry("item", self.display_item())?;

        match *self.item() {
            SpecificDisplayItem::Text(_) => {
                map.serialize_entry("glyphs",
                    &self.iter.list.get(self.glyphs()).collect::<Vec<_>>())?;
            }
            SpecificDisplayItem::PushStackingContext(_) => {
                map.serialize_entry("filters",
                    &self.iter.list.get(self.filters()).collect::<Vec<_>>())?;
            }
            _ => { }
        }

        let clip_region = self.clip_region();
        let gradient_stops = self.gradient_stops();

        map.serialize_entry("clip_region", &clip_region)?;
        if !clip_region.complex_clips.is_empty() {
            map.serialize_entry("complex_clips",
                &self.iter.list.get(clip_region.complex_clips).collect::<Vec<_>>())?;
        }

        if !gradient_stops.is_empty() {
            map.serialize_entry("gradient_stops",
                &self.iter.list.get(gradient_stops).collect::<Vec<_>>())?;
        }

        map.end()
    }
}

#[derive(Clone)]
pub struct DisplayListBuilder {
    pub data: Vec<u8>,
    pub pipeline_id: PipelineId,
    clip_stack: Vec<ClipAndScrollInfo>,
    next_clip_id: u64,
    builder_start_time: u64,

    /// The size of the content of this display list. This is used to allow scrolling
    /// outside the bounds of the display list items themselves.
    content_size: LayoutSize,
}

impl DisplayListBuilder {
    pub fn new(pipeline_id: PipelineId, content_size: LayoutSize) -> DisplayListBuilder {
        Self::with_capacity(pipeline_id, content_size, 0)
    }

    pub fn with_capacity(pipeline_id: PipelineId,
                         content_size: LayoutSize,
                         capacity: usize) -> DisplayListBuilder {
        let start_time = precise_time_ns();

        // We start at 1 here, because the root scroll id is always 0.
        const FIRST_CLIP_ID : u64 = 1;

        DisplayListBuilder {
            data: Vec::with_capacity(capacity),
            pipeline_id: pipeline_id,
            clip_stack: vec![ClipAndScrollInfo::simple(ClipId::root_scroll_node(pipeline_id))],
            next_clip_id: FIRST_CLIP_ID,
            builder_start_time: start_time,
            content_size: content_size,
        }
    }

    pub fn print_display_list(&mut self) {
        let mut temp = BuiltDisplayList::default();
        ::std::mem::swap(&mut temp.data, &mut self.data);

        {
            let mut iter = BuiltDisplayListIter::new(&temp);
            while let Some(item) = iter.next() {
                println!("{:?}", item.display_item());
            }
        }

        self.data = temp.data;
    }

    fn push_item(&mut self, item: SpecificDisplayItem, rect: LayoutRect) {
        bincode::serialize_into(&mut self.data, &DisplayItem {
            item: item,
            rect: rect,
            clip_and_scroll: *self.clip_stack.last().unwrap(),
        }, bincode::Infinite).unwrap();
    }

    fn push_new_empty_item(&mut self, item: SpecificDisplayItem) {
        bincode::serialize_into(&mut self.data, &DisplayItem {
            item: item,
            rect: LayoutRect::zero(),
            clip_and_scroll: *self.clip_stack.last().unwrap(),
        }, bincode::Infinite).unwrap();
    }

    fn push_iter<I>(&mut self, iter: I)
    where I: IntoIterator,
          I::IntoIter: ExactSizeIterator,
          I::Item: Serialize,
    {
        let iter = iter.into_iter();
        let len = iter.len();
        let mut count = 0;

        bincode::serialize_into(&mut self.data, &len, bincode::Infinite).unwrap();
        for elem in iter {
            count += 1;
            bincode::serialize_into(&mut self.data, &elem, bincode::Infinite).unwrap();
        }

        debug_assert_eq!(len, count);
    }

    pub fn push_rect(&mut self,
                     rect: LayoutRect,
                     _token: ClipRegionToken,
                     color: ColorF) {
        let item = SpecificDisplayItem::Rectangle(RectangleDisplayItem {
            color: color,
        });

        self.push_item(item, rect);
    }

    pub fn push_image(&mut self,
                      rect: LayoutRect,
                      _token: ClipRegionToken,
                      stretch_size: LayoutSize,
                      tile_spacing: LayoutSize,
                      image_rendering: ImageRendering,
                      key: ImageKey) {
        let item = SpecificDisplayItem::Image(ImageDisplayItem {
            image_key: key,
            stretch_size: stretch_size,
            tile_spacing: tile_spacing,
            image_rendering: image_rendering,
        });

        self.push_item(item, rect);
    }

    /// Push a yuv image. All planar data in yuv image should use the same buffer type.
    pub fn push_yuv_image(&mut self,
                          rect: LayoutRect,
                          _token: ClipRegionToken,
                          yuv_data: YuvData,
                          color_space: YuvColorSpace,
                          image_rendering: ImageRendering) {
        let item = SpecificDisplayItem::YuvImage(YuvImageDisplayItem {
            yuv_data: yuv_data,
            color_space: color_space,
            image_rendering: image_rendering,
        });
        self.push_item(item, rect);
    }

    pub fn push_webgl_canvas(&mut self,
                             rect: LayoutRect,
                             _token: ClipRegionToken,
                             context_id: WebGLContextId) {
        let item = SpecificDisplayItem::WebGL(WebGLDisplayItem {
            context_id: context_id,
        });
        self.push_item(item, rect);
    }

    pub fn push_text(&mut self,
                     rect: LayoutRect,
                     _token: ClipRegionToken,
                     glyphs: &[GlyphInstance],
                     font_key: FontKey,
                     color: ColorF,
                     size: Au,
                     blur_radius: f32,
                     glyph_options: Option<GlyphOptions>) {
        // Sanity check - anything with glyphs bigger than this
        // is probably going to consume too much memory to render
        // efficiently anyway. This is specifically to work around
        // the font_advance.html reftest, which creates a very large
        // font as a crash test - the rendering is also ignored
        // by the azure renderer.
        if size < Au::from_px(4096) {
            let item = SpecificDisplayItem::Text(TextDisplayItem {
                color: color,
                font_key: font_key,
                size: size,
                blur_radius: blur_radius,
                glyph_options: glyph_options,
            });

            self.push_item(item, rect);
            self.push_iter(glyphs);
        }
    }

    // Gradients can be defined with stops outside the range of [0, 1]
    // when this happens the gradient needs to be normalized by adjusting
    // the gradient stops and gradient line into an equivalent gradient
    // with stops in the range [0, 1]. this is done by moving the beginning
    // of the gradient line to where stop[0] and the end of the gradient line
    // to stop[n-1]. this function adjusts the stops in place, and returns
    // the amount to adjust the gradient line start and stop
    fn normalize_stops(stops: &mut Vec<GradientStop>,
                       extend_mode: ExtendMode) -> (f32, f32) {
        assert!(stops.len() >= 2);

        let first = *stops.first().unwrap();
        let last = *stops.last().unwrap();

        assert!(first.offset <= last.offset);

        let stops_origin = first.offset;
        let stops_delta = last.offset - first.offset;

        if stops_delta > 0.000001 {
            for stop in stops {
                stop.offset = (stop.offset - stops_origin) / stops_delta;
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
                    stops.push(GradientStop {
                        color: first.color,
                        offset: 0.0,
                    });
                    stops.push(GradientStop {
                        color: first.color,
                        offset: 0.5,
                    });
                    stops.push(GradientStop {
                        color: last.color,
                        offset: 0.5,
                    });
                    stops.push(GradientStop {
                        color: last.color,
                        offset: 1.0,
                    });

                    let offset = last.offset;

                    (offset - 0.5, offset + 0.5)
                }
                ExtendMode::Repeat => {
                    // A repeating gradient with stops that are all in the same
                    // position should just display the last color. I believe the
                    // spec says that it should be the average color of the gradient,
                    // but this matches what Gecko and Blink does
                    stops.push(GradientStop {
                        color: last.color,
                        offset: 0.0,
                    });
                    stops.push(GradientStop {
                        color: last.color,
                        offset: 1.0,
                    });

                    (0.0, 1.0)
                }
            }
        }
    }

    // NOTE: gradients must be pushed in the order they're created
    // because create_gradient stores the stops in anticipation
    pub fn create_gradient(&mut self,
                           start_point: LayoutPoint,
                           end_point: LayoutPoint,
                           mut stops: Vec<GradientStop>,
                           extend_mode: ExtendMode) -> Gradient {
        let (start_offset,
             end_offset) = DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        let start_to_end = end_point - start_point;

        self.push_stops(&stops);

        Gradient {
            start_point: start_point + start_to_end * start_offset,
            end_point: start_point + start_to_end * end_offset,
            extend_mode: extend_mode,
        }
    }

    // NOTE: gradients must be pushed in the order they're created
    // because create_gradient stores the stops in anticipation
    pub fn create_radial_gradient(&mut self,
                                  center: LayoutPoint,
                                  radius: LayoutSize,
                                  mut stops: Vec<GradientStop>,
                                  extend_mode: ExtendMode) -> RadialGradient {
        if radius.width <= 0.0 || radius.height <= 0.0 {
            // The shader cannot handle a non positive radius. So
            // reuse the stops vector and construct an equivalent
            // gradient.
            let last_color = stops.last().unwrap().color;

            let stops = [
                GradientStop {
                    offset: 0.0,
                    color: last_color,
                },
                GradientStop {
                    offset: 1.0,
                    color: last_color,
                },
            ];

            self.push_stops(&stops);

            return RadialGradient {
                start_center: center,
                start_radius: 0.0,
                end_center: center,
                end_radius: 1.0,
                ratio_xy: 1.0,
                extend_mode: extend_mode,
            };
        }

        let (start_offset,
             end_offset) = DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        self.push_stops(&stops);

        RadialGradient {
            start_center: center,
            start_radius: radius.width * start_offset,
            end_center: center,
            end_radius: radius.width * end_offset,
            ratio_xy: radius.width / radius.height,
            extend_mode: extend_mode,
        }
    }

    // NOTE: gradients must be pushed in the order they're created
    // because create_gradient stores the stops in anticipation
    pub fn create_complex_radial_gradient(&mut self,
                                          start_center: LayoutPoint,
                                          start_radius: f32,
                                          end_center: LayoutPoint,
                                          end_radius: f32,
                                          ratio_xy: f32,
                                          stops: Vec<GradientStop>,
                                          extend_mode: ExtendMode) -> RadialGradient {

        self.push_stops(&stops);

        RadialGradient {
            start_center: start_center,
            start_radius: start_radius,
            end_center: end_center,
            end_radius: end_radius,
            ratio_xy: ratio_xy,
            extend_mode: extend_mode,
        }
    }

    pub fn push_border(&mut self,
                       rect: LayoutRect,
                       _token: ClipRegionToken,
                       widths: BorderWidths,
                       details: BorderDetails) {
        let item = SpecificDisplayItem::Border(BorderDisplayItem {
            details: details,
            widths: widths,
        });

        self.push_item(item, rect);
    }

    pub fn push_box_shadow(&mut self,
                           rect: LayoutRect,
                           _token: ClipRegionToken,
                           box_bounds: LayoutRect,
                           offset: LayoutVector2D,
                           color: ColorF,
                           blur_radius: f32,
                           spread_radius: f32,
                           border_radius: f32,
                           clip_mode: BoxShadowClipMode) {
        let item = SpecificDisplayItem::BoxShadow(BoxShadowDisplayItem {
            box_bounds: box_bounds,
            offset: offset,
            color: color,
            blur_radius: blur_radius,
            spread_radius: spread_radius,
            border_radius: border_radius,
            clip_mode: clip_mode,
        });

        self.push_item(item, rect);
    }

    pub fn push_gradient(&mut self,
                         rect: LayoutRect,
                         _token: ClipRegionToken,
                         gradient: Gradient,
                         tile_size: LayoutSize,
                         tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::Gradient(GradientDisplayItem {
            gradient: gradient,
            tile_size: tile_size,
            tile_spacing: tile_spacing,
        });

        self.push_item(item, rect);
    }

    pub fn push_radial_gradient(&mut self,
                                rect: LayoutRect,
                                _token: ClipRegionToken,
                                gradient: RadialGradient,
                                tile_size: LayoutSize,
                                tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::RadialGradient(RadialGradientDisplayItem {
            gradient: gradient,
            tile_size: tile_size,
            tile_spacing: tile_spacing,
        });

        self.push_item(item, rect);
    }

    pub fn push_stacking_context(&mut self,
                                 scroll_policy: ScrollPolicy,
                                 bounds: LayoutRect,
                                 transform: Option<PropertyBinding<LayoutTransform>>,
                                 transform_style: TransformStyle,
                                 perspective: Option<LayoutTransform>,
                                 mix_blend_mode: MixBlendMode,
                                 filters: Vec<FilterOp>) {
        let item = SpecificDisplayItem::PushStackingContext(PushStackingContextDisplayItem {
            stacking_context: StackingContext {
                scroll_policy: scroll_policy,
                transform: transform,
                transform_style: transform_style,
                perspective: perspective,
                mix_blend_mode: mix_blend_mode,
            }
        });

        self.push_item(item, bounds);
        self.push_iter(&filters);
    }

    pub fn pop_stacking_context(&mut self) {
        self.push_new_empty_item(SpecificDisplayItem::PopStackingContext);
    }

    pub fn push_stops(&mut self, stops: &[GradientStop]) {
        if stops.is_empty() {
            return
        }
        self.push_new_empty_item(SpecificDisplayItem::SetGradientStops);
        self.push_iter(stops);
    }

    pub fn define_clip(&mut self,
                       content_rect: LayoutRect,
                       _token: ClipRegionToken,
                       id: Option<ClipId>)
                       -> ClipId {
        let id = match id {
            Some(id) => id,
            None => {
                self.next_clip_id += 1;
                ClipId::Clip(self.next_clip_id - 1, 0, self.pipeline_id)
            }
        };

        let item = SpecificDisplayItem::Clip(ClipDisplayItem {
            id: id,
            parent_id: self.clip_stack.last().unwrap().scroll_node_id,
        });

        self.push_item(item, content_rect);
        id
    }

    pub fn push_clip_node(&mut self,
                          content_rect: LayoutRect,
                          token: ClipRegionToken,
                          id: Option<ClipId>){
        let id = self.define_clip(content_rect, token, id);
        self.clip_stack.push(ClipAndScrollInfo::simple(id));
    }

    pub fn push_clip_id(&mut self, id: ClipId) {
        self.clip_stack.push(ClipAndScrollInfo::simple(id));
    }

    pub fn push_clip_and_scroll_info(&mut self, info: ClipAndScrollInfo) {
        self.clip_stack.push(info);
    }

    pub fn pop_clip_id(&mut self) {
        self.clip_stack.pop();
        assert!(self.clip_stack.len() > 0);
    }

    pub fn pop_clip_node(&mut self) {
        self.pop_clip_id();
    }

    pub fn push_iframe(&mut self, rect: LayoutRect, _token: ClipRegionToken, pipeline_id: PipelineId) {
        let item = SpecificDisplayItem::Iframe(IframeDisplayItem { pipeline_id: pipeline_id });
        self.push_item(item, rect);
    }

    // Don't use this function. It will go away.
    //
    // We're using this method as a hack in Gecko to retain parts sub-parts of display
    // lists so that we can regenerate them without building Gecko display items. WebRender
    // will replace references to the root scroll frame id with the current scroll frame
    // id.
    pub fn push_nested_display_list(&mut self, built_display_list: &BuiltDisplayList) {
        self.push_clip_region(&LayoutRect::zero(), vec![], None);
        self.push_new_empty_item(SpecificDisplayItem::PushNestedDisplayList);

        self.data.extend_from_slice(&built_display_list.data);

        self.push_clip_region(&LayoutRect::zero(), vec![], None);
        self.push_new_empty_item(SpecificDisplayItem::PopNestedDisplayList);
    }

    pub fn push_clip_region<I>(&mut self,
                            rect: &LayoutRect,
                            complex: I,
                            image_mask: Option<ImageMask>)
                            -> ClipRegionToken
    where I: IntoIterator<Item = ComplexClipRegion>,
          I::IntoIter: ExactSizeIterator,
    {
        self.push_new_empty_item(SpecificDisplayItem::SetClipRegion(
            ClipRegion::new(rect, image_mask),
        ));
        self.push_iter(complex);

        ClipRegionToken { _unforgeable: () }
    }

    pub fn finalize(self) -> (PipelineId, LayoutSize, BuiltDisplayList) {
        let end_time = precise_time_ns();

        (self.pipeline_id,
         self.content_size,
         BuiltDisplayList {
            descriptor: BuiltDisplayListDescriptor {
                builder_start_time: self.builder_start_time,
                builder_finish_time: end_time,
            },
            data: self.data,
         })
    }
}

/// Verification that push_clip_region was called before
/// pushing an item that requires it.
pub struct ClipRegionToken { _unforgeable: () }
