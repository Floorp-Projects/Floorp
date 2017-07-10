/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use bincode;
use serde::{Deserialize, Serialize, Serializer};
use serde::ser::{SerializeSeq, SerializeMap};
use time::precise_time_ns;
use {BorderDetails, BorderDisplayItem, BorderWidths, BoxShadowClipMode, BoxShadowDisplayItem};
use {ClipAndScrollInfo, ClipDisplayItem, ClipId, ColorF, ComplexClipRegion, DisplayItem};
use {ExtendMode, FilterOp, FontKey, GlyphInstance, GlyphOptions, Gradient, GradientDisplayItem};
use {GradientStop, IframeDisplayItem, ImageDisplayItem, ImageKey, ImageMask, ImageRendering};
use {LayoutPoint, LayoutRect, LayoutSize, LayoutTransform, LayoutVector2D, LocalClip};
use {MixBlendMode, PipelineId, PropertyBinding, PushStackingContextDisplayItem, RadialGradient};
use {RadialGradientDisplayItem, RectangleDisplayItem, ScrollPolicy, SpecificDisplayItem};
use {StackingContext, TextDisplayItem, TransformStyle, WebGLContextId, WebGLDisplayItem};
use {YuvColorSpace, YuvData, YuvImageDisplayItem};
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

impl BuiltDisplayListDescriptor {
}

impl BuiltDisplayList {
    pub fn from_data(data: Vec<u8>, descriptor: BuiltDisplayListDescriptor) -> BuiltDisplayList {
        BuiltDisplayList {
            data,
            descriptor,
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
        Self::new_with_list_and_data(list, &list.data)
    }

    pub fn new_with_list_and_data(list: &'a BuiltDisplayList, data: &'a [u8]) -> Self {
        BuiltDisplayListIter {
            list,
            data: &data,
            cur_item: DisplayItem { // Dummy data, will be overwritten by `next`
                item: SpecificDisplayItem::PopStackingContext,
                rect: LayoutRect::zero(),
                local_clip: LocalClip::from(LayoutRect::zero()),
                clip_and_scroll: ClipAndScrollInfo::simple(ClipId::new(0, PipelineId(0, 0))),
            },
            cur_stops: ItemRange::default(),
            cur_glyphs: ItemRange::default(),
            cur_filters: ItemRange::default(),
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
                return Some(self.as_ref())
            }
            Peek::StartPeeking => {
                self.peeking = Peek::IsPeeking;
            }
            Peek::NotPeeking => { /* do nothing */ }
        }

        // Don't let these bleed into another item
        self.cur_stops = ItemRange::default();
        self.cur_complex_clip = (ItemRange::default(), 0);

        loop {
            if self.data.len() == 0 {
                return None
            }

            self.cur_item = bincode::deserialize_from(&mut self.data, bincode::Infinite)
                                    .expect("MEH: malicious process?");

            match self.cur_item.item {
                SetGradientStops => {
                    self.cur_stops = self.skip_slice::<GradientStop>().0;

                    // This is a dummy item, skip over it
                    continue;
                }
                Clip(_) | ScrollFrame(_) =>
                    self.cur_complex_clip = self.skip_slice::<ComplexClipRegion>(),
                Text(_) => self.cur_glyphs = self.skip_slice::<GlyphInstance>().0,
                PushStackingContext(_) => self.cur_filters = self.skip_slice::<FilterOp>().0,
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

    pub fn local_clip(&self) -> &LocalClip {
        &self.iter.cur_item.local_clip
    }

    pub fn clip_and_scroll(&self) -> ClipAndScrollInfo {
        self.iter.cur_item.clip_and_scroll
    }

    pub fn item(&self) -> &SpecificDisplayItem {
        &self.iter.cur_item.item
    }

    pub fn complex_clip(&self) -> &(ItemRange<ComplexClipRegion>, usize) {
        &self.iter.cur_complex_clip
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

    // Creates a new iterator where this element's iterator is, to hack around borrowck.
    pub fn sub_iter(&self) -> BuiltDisplayListIter<'a> {
        BuiltDisplayListIter::new_with_list_and_data(self.iter.list, self.iter.data)
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

        let &(complex_clips, number_of_complex_clips) = self.complex_clip();
        let gradient_stops = self.gradient_stops();

        if number_of_complex_clips > 0 {
            map.serialize_entry("complex_clips",
                &self.iter.list.get(complex_clips).collect::<Vec<_>>())?;
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
            pipeline_id,
            clip_stack: vec![ClipAndScrollInfo::simple(ClipId::root_scroll_node(pipeline_id))],
            next_clip_id: FIRST_CLIP_ID,
            builder_start_time: start_time,
            content_size,
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

    fn push_item(&mut self,
                 item: SpecificDisplayItem,
                 rect: LayoutRect,
                 local_clip: Option<LocalClip>) {
        let local_clip = local_clip.unwrap_or_else(|| LocalClip::from(rect));
        bincode::serialize_into(&mut self.data, &DisplayItem {
            item,
            rect,
            local_clip: local_clip,
            clip_and_scroll: *self.clip_stack.last().unwrap(),
        }, bincode::Infinite).unwrap();
    }

    fn push_new_empty_item(&mut self, item: SpecificDisplayItem) {
        bincode::serialize_into(&mut self.data, &DisplayItem {
            item,
            rect: LayoutRect::zero(),
            local_clip: LocalClip::from(LayoutRect::zero()),
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

    pub fn push_rect(&mut self, rect: LayoutRect, local_clip: Option<LocalClip>, color: ColorF) {
        let item = SpecificDisplayItem::Rectangle(RectangleDisplayItem {
            color,
        });

        self.push_item(item, rect, local_clip);
    }

    pub fn push_image(&mut self,
                      rect: LayoutRect,
                      local_clip: Option<LocalClip>,
                      stretch_size: LayoutSize,
                      tile_spacing: LayoutSize,
                      image_rendering: ImageRendering,
                      key: ImageKey) {
        let item = SpecificDisplayItem::Image(ImageDisplayItem {
            image_key: key,
            stretch_size,
            tile_spacing,
            image_rendering,
        });

        self.push_item(item, rect, local_clip);
    }

    /// Push a yuv image. All planar data in yuv image should use the same buffer type.
    pub fn push_yuv_image(&mut self,
                          rect: LayoutRect,
                          local_clip: Option<LocalClip>,
                          yuv_data: YuvData,
                          color_space: YuvColorSpace,
                          image_rendering: ImageRendering) {
        let item = SpecificDisplayItem::YuvImage(YuvImageDisplayItem {
            yuv_data,
            color_space,
            image_rendering,
        });
        self.push_item(item, rect, local_clip);
    }

    pub fn push_webgl_canvas(&mut self,
                             rect: LayoutRect,
                             local_clip: Option<LocalClip>,
                             context_id: WebGLContextId) {
        let item = SpecificDisplayItem::WebGL(WebGLDisplayItem {
            context_id,
        });
        self.push_item(item, rect, local_clip);
    }

    pub fn push_text(&mut self,
                     rect: LayoutRect,
                     local_clip: Option<LocalClip>,
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
                color,
                font_key,
                size,
                blur_radius,
                glyph_options,
            });

            self.push_item(item, rect, local_clip);
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
    fn normalize_stops(stops: &mut Vec<GradientStop>, extend_mode: ExtendMode) -> (f32, f32) {
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
            extend_mode,
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
                extend_mode,
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
            extend_mode,
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
            start_center,
            start_radius,
            end_center,
            end_radius,
            ratio_xy,
            extend_mode,
        }
    }

    pub fn push_border(&mut self,
                       rect: LayoutRect,
                       local_clip: Option<LocalClip>,
                       widths: BorderWidths,
                       details: BorderDetails) {
        let item = SpecificDisplayItem::Border(BorderDisplayItem {
            details,
            widths,
        });

        self.push_item(item, rect, local_clip);
    }

    pub fn push_box_shadow(&mut self,
                           rect: LayoutRect,
                           local_clip: Option<LocalClip>,
                           box_bounds: LayoutRect,
                           offset: LayoutVector2D,
                           color: ColorF,
                           blur_radius: f32,
                           spread_radius: f32,
                           border_radius: f32,
                           clip_mode: BoxShadowClipMode) {
        let item = SpecificDisplayItem::BoxShadow(BoxShadowDisplayItem {
            box_bounds,
            offset,
            color,
            blur_radius,
            spread_radius,
            border_radius,
            clip_mode,
        });

        self.push_item(item, rect, local_clip);
    }

    pub fn push_gradient(&mut self,
                         rect: LayoutRect,
                         local_clip: Option<LocalClip>,
                         gradient: Gradient,
                         tile_size: LayoutSize,
                         tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::Gradient(GradientDisplayItem {
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(item, rect, local_clip);
    }

    pub fn push_radial_gradient(&mut self,
                                rect: LayoutRect,
                                local_clip: Option<LocalClip>,
                                gradient: RadialGradient,
                                tile_size: LayoutSize,
                                tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::RadialGradient(RadialGradientDisplayItem {
            gradient,
            tile_size,
            tile_spacing,
        });

        self.push_item(item, rect, local_clip);
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
                scroll_policy,
                transform,
                transform_style,
                perspective,
                mix_blend_mode,
            }
        });

        self.push_item(item, bounds, None);
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

    fn generate_clip_id(&mut self, id: Option<ClipId>) -> ClipId {
        id.unwrap_or_else(|| {
            self.next_clip_id += 1;
            ClipId::Clip(self.next_clip_id - 1, 0, self.pipeline_id)
        })
    }

    pub fn define_scroll_frame<I>(&mut self,
                                  id: Option<ClipId>,
                                  content_rect: LayoutRect,
                                  clip_rect: LayoutRect,
                                  complex_clips: I,
                                  image_mask: Option<ImageMask>)
                                  -> ClipId
                                  where I: IntoIterator<Item = ComplexClipRegion>,
                                        I::IntoIter: ExactSizeIterator {
        let id = self.generate_clip_id(id);
        let item = SpecificDisplayItem::ScrollFrame(ClipDisplayItem {
            id: id,
            parent_id: self.clip_stack.last().unwrap().scroll_node_id,
            image_mask: image_mask,
        });

        self.push_item(item, content_rect, Some(LocalClip::from(clip_rect)));
        self.push_iter(complex_clips);
        id
    }

    pub fn define_clip<I>(&mut self,
                          id: Option<ClipId>,
                          clip_rect: LayoutRect,
                          complex_clips: I,
                          image_mask: Option<ImageMask>)
                          -> ClipId
                          where I: IntoIterator<Item = ComplexClipRegion>,
                                I::IntoIter: ExactSizeIterator {
        let id = self.generate_clip_id(id);
        let item = SpecificDisplayItem::Clip(ClipDisplayItem {
            id,
            parent_id: self.clip_stack.last().unwrap().scroll_node_id,
            image_mask: image_mask,
        });

        self.push_item(item, clip_rect, Some(LocalClip::from(clip_rect)));
        self.push_iter(complex_clips);
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
        assert!(self.clip_stack.len() > 0);
    }

    pub fn push_iframe(&mut self, rect: LayoutRect, pipeline_id: PipelineId) {
        let item = SpecificDisplayItem::Iframe(IframeDisplayItem { pipeline_id: pipeline_id });
        self.push_item(item, rect, None);
    }

    // Don't use this function. It will go away.
    //
    // We're using this method as a hack in Gecko to retain parts sub-parts of display
    // lists so that we can regenerate them without building Gecko display items. WebRender
    // will replace references to the root scroll frame id with the current scroll frame
    // id.
    pub fn push_nested_display_list(&mut self, built_display_list: &BuiltDisplayList) {
        self.push_new_empty_item(SpecificDisplayItem::PushNestedDisplayList);
        self.data.extend_from_slice(&built_display_list.data);
        self.push_new_empty_item(SpecificDisplayItem::PopNestedDisplayList);
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
