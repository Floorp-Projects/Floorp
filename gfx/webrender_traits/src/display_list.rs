/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use std::mem;
use std::slice;
use time::precise_time_ns;
use {BorderDetails, BorderDisplayItem, BorderWidths, BoxShadowClipMode, BoxShadowDisplayItem};
use {ClipAndScrollInfo, ClipDisplayItem, ClipId, ClipRegion, ColorF, ComplexClipRegion};
use {DisplayItem, ExtendMode, FilterOp, FontKey, GlyphInstance, GlyphOptions, Gradient};
use {GradientDisplayItem, GradientStop, IframeDisplayItem, ImageDisplayItem, ImageKey, ImageMask};
use {ImageRendering, ItemRange, LayoutPoint, LayoutRect, LayoutSize, LayoutTransform};
use {MixBlendMode, PipelineId, PropertyBinding, PushStackingContextDisplayItem, RadialGradient};
use {RadialGradientDisplayItem, RectangleDisplayItem, ScrollPolicy, SpecificDisplayItem};
use {StackingContext, TextDisplayItem, TransformStyle, WebGLContextId, WebGLDisplayItem};
use {YuvColorSpace, YuvData, YuvImageDisplayItem};

#[derive(Clone, Deserialize, Serialize)]
pub struct AuxiliaryLists {
    /// The concatenation of: gradient stops, complex clip regions, filters, and glyph instances,
    /// in that order.
    data: Vec<u8>,
    descriptor: AuxiliaryListsDescriptor,
}

/// Describes the memory layout of the auxiliary lists.
///
/// Auxiliary lists consist of some number of gradient stops, complex clip regions, filters, and
/// glyph instances, in that order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct AuxiliaryListsDescriptor {
    gradient_stops_size: usize,
    complex_clip_regions_size: usize,
    filters_size: usize,
    glyph_instances_size: usize,
}

/// A display list.
#[derive(Clone, Deserialize, Serialize)]
pub struct BuiltDisplayList {
    data: Vec<u8>,
    descriptor: BuiltDisplayListDescriptor,
}

/// Describes the memory layout of a display list.
///
/// A display list consists of some number of display list items, followed by a number of display
/// items.
#[repr(C)]
#[derive(Copy, Clone, Deserialize, Serialize)]
pub struct BuiltDisplayListDescriptor {
    /// The size in bytes of the display list items in this display list.
    display_list_items_size: usize,
    /// The first IPC time stamp: before any work has been done
    builder_start_time: u64,
    /// The second IPC time stamp: after serialization
    builder_finish_time: u64,
}

impl BuiltDisplayListDescriptor {
    pub fn size(&self) -> usize {
        self.display_list_items_size
    }
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

    pub fn all_display_items(&self) -> &[DisplayItem] {
        unsafe {
            convert_blob_to_pod(&self.data)
        }
    }

    pub fn into_display_items(self) -> Vec<DisplayItem> {
        unsafe {
            convert_vec_blob_to_pod(self.data)
        }
    }

    pub fn times(&self) -> (u64, u64) {
      (self.descriptor.builder_start_time, self.descriptor.builder_finish_time)
    }
}

#[derive(Clone)]
pub struct DisplayListBuilder {
    pub list: Vec<DisplayItem>,
    auxiliary_lists_builder: AuxiliaryListsBuilder,
    pub pipeline_id: PipelineId,
    clip_stack: Vec<ClipAndScrollInfo>,
    next_clip_id: u64,
    builder_start_time: u64,
}

impl DisplayListBuilder {
    pub fn new(pipeline_id: PipelineId) -> DisplayListBuilder {
        let start_time = precise_time_ns();
        DisplayListBuilder {
            list: Vec::new(),
            auxiliary_lists_builder: AuxiliaryListsBuilder::new(),
            pipeline_id: pipeline_id,
            clip_stack: vec![ClipAndScrollInfo::simple(ClipId::root_scroll_node(pipeline_id))],

            // We start at 1 here, because the root scroll id is always 0.
            next_clip_id: 1,
            builder_start_time: start_time,
        }
    }

    pub fn print_display_list(&mut self) {
        for item in &self.list {
            println!("{:?}", item);
        }
    }

    fn push_item(&mut self, item: SpecificDisplayItem, rect: LayoutRect, clip: ClipRegion) {
        self.list.push(DisplayItem {
            item: item,
            rect: rect,
            clip: clip,
            clip_and_scroll: *self.clip_stack.last().unwrap(),
        });
    }

    fn push_new_empty_item(&mut self, item: SpecificDisplayItem) {
        self.list.push(DisplayItem {
            item: item,
            rect: LayoutRect::zero(),
            clip: ClipRegion::simple(&LayoutRect::zero()),
            clip_and_scroll: *self.clip_stack.last().unwrap(),
        });
    }

    pub fn push_rect(&mut self,
                     rect: LayoutRect,
                     clip: ClipRegion,
                     color: ColorF) {
        let item = SpecificDisplayItem::Rectangle(RectangleDisplayItem {
            color: color,
        });

        self.push_item(item, rect, clip);
    }

    pub fn push_image(&mut self,
                      rect: LayoutRect,
                      clip: ClipRegion,
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

        self.push_item(item, rect, clip);
    }

    /// Push a yuv image. All planar data in yuv image should use the same buffer type.
    pub fn push_yuv_image(&mut self,
                          rect: LayoutRect,
                          clip: ClipRegion,
                          yuv_data: YuvData,
                          color_space: YuvColorSpace) {
        let item = SpecificDisplayItem::YuvImage(YuvImageDisplayItem {
            yuv_data: yuv_data,
            color_space: color_space,
        });
        self.push_item(item, rect, clip);
    }

    pub fn push_webgl_canvas(&mut self,
                             rect: LayoutRect,
                             clip: ClipRegion,
                             context_id: WebGLContextId) {
        let item = SpecificDisplayItem::WebGL(WebGLDisplayItem {
            context_id: context_id,
        });
        self.push_item(item, rect, clip);
    }

    pub fn push_text(&mut self,
                     rect: LayoutRect,
                     clip: ClipRegion,
                     glyphs: &[GlyphInstance],
                     font_key: FontKey,
                     color: ColorF,
                     size: Au,
                     blur_radius: Au,
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
                glyphs: self.auxiliary_lists_builder.add_glyph_instances(&glyphs),
                font_key: font_key,
                size: size,
                blur_radius: blur_radius,
                glyph_options: glyph_options,
            });

            self.push_item(item, rect, clip);
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

    pub fn create_gradient(&mut self,
                           start_point: LayoutPoint,
                           end_point: LayoutPoint,
                           mut stops: Vec<GradientStop>,
                           extend_mode: ExtendMode) -> Gradient {
        let (start_offset,
             end_offset) = DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        let start_to_end = end_point - start_point;

        Gradient {
            start_point: start_point + start_to_end * start_offset,
            end_point: start_point + start_to_end * end_offset,
            stops: self.auxiliary_lists_builder.add_gradient_stops(&stops),
            extend_mode: extend_mode,
        }
    }

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

            stops.clear();
            stops.push(GradientStop {
                offset: 0.0,
                color: last_color,
            });
            stops.push(GradientStop {
                offset: 1.0,
                color: last_color,
            });

            return RadialGradient {
                start_center: center,
                start_radius: 0.0,
                end_center: center,
                end_radius: 1.0,
                ratio_xy: 1.0,
                stops: self.auxiliary_lists_builder.add_gradient_stops(&stops),
                extend_mode: extend_mode,
            };
        }

        let (start_offset,
             end_offset) = DisplayListBuilder::normalize_stops(&mut stops, extend_mode);

        RadialGradient {
            start_center: center,
            start_radius: radius.width * start_offset,
            end_center: center,
            end_radius: radius.width * end_offset,
            ratio_xy: radius.width / radius.height,
            stops: self.auxiliary_lists_builder.add_gradient_stops(&stops),
            extend_mode: extend_mode,
        }
    }

    pub fn create_complex_radial_gradient(&mut self,
                                          start_center: LayoutPoint,
                                          start_radius: f32,
                                          end_center: LayoutPoint,
                                          end_radius: f32,
                                          ratio_xy: f32,
                                          stops: Vec<GradientStop>,
                                          extend_mode: ExtendMode) -> RadialGradient {
        RadialGradient {
            start_center: start_center,
            start_radius: start_radius,
            end_center: end_center,
            end_radius: end_radius,
            ratio_xy: ratio_xy,
            stops: self.auxiliary_lists_builder.add_gradient_stops(&stops),
            extend_mode: extend_mode,
        }
    }

    pub fn push_border(&mut self,
                       rect: LayoutRect,
                       clip: ClipRegion,
                       widths: BorderWidths,
                       details: BorderDetails) {
        let item = SpecificDisplayItem::Border(BorderDisplayItem {
            details: details,
            widths: widths,
        });

        self.push_item(item, rect, clip);
    }

    pub fn push_box_shadow(&mut self,
                           rect: LayoutRect,
                           clip: ClipRegion,
                           box_bounds: LayoutRect,
                           offset: LayoutPoint,
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

        self.push_item(item, rect, clip);
    }

    pub fn push_gradient(&mut self,
                         rect: LayoutRect,
                         clip: ClipRegion,
                         gradient: Gradient,
                         tile_size: LayoutSize,
                         tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::Gradient(GradientDisplayItem {
            gradient: gradient,
            tile_size: tile_size,
            tile_spacing: tile_spacing,
        });

        self.push_item(item, rect, clip);
    }

    pub fn push_radial_gradient(&mut self,
                                rect: LayoutRect,
                                clip: ClipRegion,
                                gradient: RadialGradient,
                                tile_size: LayoutSize,
                                tile_spacing: LayoutSize) {
        let item = SpecificDisplayItem::RadialGradient(RadialGradientDisplayItem {
            gradient: gradient,
            tile_size: tile_size,
            tile_spacing: tile_spacing,
        });

        self.push_item(item, rect, clip);
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
                filters: self.auxiliary_lists_builder.add_filters(&filters),
            }
        });

        self.push_item(item, bounds, ClipRegion::simple(&LayoutRect::zero()));
    }

    pub fn pop_stacking_context(&mut self) {
        self.push_new_empty_item(SpecificDisplayItem::PopStackingContext);
    }

    pub fn define_clip(&mut self,
                       content_rect: LayoutRect,
                       clip: ClipRegion,
                       id: Option<ClipId>)
                       -> ClipId {
        let id = match id {
            Some(id) => id,
            None => {
                self.next_clip_id += 1;
                ClipId::Clip(self.next_clip_id - 1, self.pipeline_id)
            }
        };

        let item = SpecificDisplayItem::Clip(ClipDisplayItem {
            id: id,
            parent_id: self.clip_stack.last().unwrap().scroll_node_id,
        });

        self.push_item(item, content_rect, clip);
        id
    }

    pub fn push_clip_node(&mut self,
                          clip: ClipRegion,
                          content_rect: LayoutRect,
                          id: Option<ClipId>) {
        let id = self.define_clip(content_rect, clip, id);
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

    pub fn push_iframe(&mut self, rect: LayoutRect, clip: ClipRegion, pipeline_id: PipelineId) {
        let item = SpecificDisplayItem::Iframe(IframeDisplayItem { pipeline_id: pipeline_id });
        self.push_item(item, rect, clip);
    }

    // Don't use this function. It will go away.
    // We're using it as a hack in Gecko to retain parts sub-parts of display lists so that
    // we can regenerate them without building Gecko display items. 
    pub fn push_built_display_list(&mut self, dl: BuiltDisplayList, aux: AuxiliaryLists) {
        use SpecificDisplayItem::*;
        // It's important for us to make sure that all of ItemRange structures are relocated
        // when copying from one list to another. To avoid this problem we could use a custom
        // derive implementation that would let ItemRanges relocate themselves.
        for i in dl.all_display_items() {
            let mut i = *i;
            match i.item {
                Text(ref mut item) => {
                    item.glyphs = self.auxiliary_lists_builder.add_glyph_instances(aux.glyph_instances(&item.glyphs));
                }
                Gradient(ref mut item) => {
                    item.gradient.stops = self.auxiliary_lists_builder.add_gradient_stops(aux.gradient_stops(&item.gradient.stops));
                }
                RadialGradient(ref mut item) => {
                    item.gradient.stops = self.auxiliary_lists_builder.add_gradient_stops(aux.gradient_stops(&item.gradient.stops));
                }
                PushStackingContext(ref mut item) => {
                    item.stacking_context.filters = self.auxiliary_lists_builder.add_filters(aux.filters(&item.stacking_context.filters));
                }
                Iframe(_) | Clip(_) => {
                    // We don't support relocating these
                    panic!();
                }
                _ => {}
            }
            i.clip.complex =
                self.auxiliary_lists_builder
                    .add_complex_clip_regions(aux.complex_clip_regions(&i.clip.complex));
            i.clip_and_scroll = *self.clip_stack.last().unwrap();
            self.list.push(i);
        }
    }

    pub fn new_clip_region(&mut self,
                           rect: &LayoutRect,
                           complex: Vec<ComplexClipRegion>,
                           image_mask: Option<ImageMask>)
                           -> ClipRegion {
        ClipRegion::new(rect, complex, image_mask, &mut self.auxiliary_lists_builder)
    }

    pub fn finalize(self) -> (PipelineId, BuiltDisplayList, AuxiliaryLists) {
        unsafe {
            let blob = convert_vec_pod_to_blob(self.list);
            let aux_list = self.auxiliary_lists_builder.finalize();

            let end_time = precise_time_ns();

            (self.pipeline_id,
             BuiltDisplayList {
                 descriptor: BuiltDisplayListDescriptor {
                    display_list_items_size: blob.len(),
                    builder_start_time: self.builder_start_time,
                    builder_finish_time: end_time,
                 },
                 data: blob,
             },
             aux_list)
        }
    }
}

impl ItemRange {
    pub fn new<T>(backing_list: &mut Vec<T>, items: &[T]) -> ItemRange where T: Copy + Clone {
        let start = backing_list.len();
        backing_list.extend_from_slice(items);
        ItemRange {
            start: start,
            length: items.len(),
        }
    }

    pub fn empty() -> ItemRange {
        ItemRange {
            start: 0,
            length: 0,
        }
    }

    pub fn get<'a, T>(&self, backing_list: &'a [T]) -> &'a [T] {
        &backing_list[self.start..(self.start + self.length)]
    }

    pub fn get_mut<'a, T>(&self, backing_list: &'a mut [T]) -> &'a mut [T] {
        &mut backing_list[self.start..(self.start + self.length)]
    }
}

#[derive(Clone, Default)]
pub struct AuxiliaryListsBuilder {
    gradient_stops: Vec<GradientStop>,
    complex_clip_regions: Vec<ComplexClipRegion>,
    filters: Vec<FilterOp>,
    glyph_instances: Vec<GlyphInstance>,
}

impl AuxiliaryListsBuilder {
    pub fn new() -> AuxiliaryListsBuilder {
        AuxiliaryListsBuilder::default()
    }

    pub fn add_gradient_stops(&mut self, gradient_stops: &[GradientStop]) -> ItemRange {
        ItemRange::new(&mut self.gradient_stops, gradient_stops)
    }

    pub fn gradient_stops(&self, gradient_stops_range: &ItemRange) -> &[GradientStop] {
        gradient_stops_range.get(&self.gradient_stops[..])
    }

    pub fn add_complex_clip_regions(&mut self, complex_clip_regions: &[ComplexClipRegion])
                                    -> ItemRange {
        ItemRange::new(&mut self.complex_clip_regions, complex_clip_regions)
    }

    pub fn complex_clip_regions(&self, complex_clip_regions_range: &ItemRange)
                                -> &[ComplexClipRegion] {
        complex_clip_regions_range.get(&self.complex_clip_regions[..])
    }

    pub fn add_filters(&mut self, filters: &[FilterOp]) -> ItemRange {
        ItemRange::new(&mut self.filters, filters)
    }

    pub fn filters(&self, filters_range: &ItemRange) -> &[FilterOp] {
        filters_range.get(&self.filters[..])
    }

    pub fn add_glyph_instances(&mut self, glyph_instances: &[GlyphInstance]) -> ItemRange {
        ItemRange::new(&mut self.glyph_instances, glyph_instances)
    }

    pub fn glyph_instances(&self, glyph_instances_range: &ItemRange) -> &[GlyphInstance] {
        glyph_instances_range.get(&self.glyph_instances[..])
    }

    pub fn finalize(self) -> AuxiliaryLists {
        unsafe {
            let mut blob = convert_vec_pod_to_blob(self.gradient_stops);
            let gradient_stops_size = blob.len();
            blob.extend_from_slice(convert_pod_to_blob(&self.complex_clip_regions));
            let complex_clip_regions_size = blob.len() - gradient_stops_size;
            blob.extend_from_slice(convert_pod_to_blob(&self.filters));
            let filters_size = blob.len() - (complex_clip_regions_size + gradient_stops_size);
            blob.extend_from_slice(convert_pod_to_blob(&self.glyph_instances));
            let glyph_instances_size = blob.len() -
                (complex_clip_regions_size + gradient_stops_size + filters_size);

            AuxiliaryLists {
                data: blob,
                descriptor: AuxiliaryListsDescriptor {
                    gradient_stops_size: gradient_stops_size,
                    complex_clip_regions_size: complex_clip_regions_size,
                    filters_size: filters_size,
                    glyph_instances_size: glyph_instances_size,
                },
            }
        }
    }
}

impl AuxiliaryListsDescriptor {
    pub fn size(&self) -> usize {
        self.gradient_stops_size + self.complex_clip_regions_size + self.filters_size +
            self.glyph_instances_size
    }
}

impl AuxiliaryLists {
    /// Creates a new `AuxiliaryLists` instance from a descriptor and data received over a channel.
    pub fn from_data(data: Vec<u8>, descriptor: AuxiliaryListsDescriptor) -> AuxiliaryLists {
        AuxiliaryLists {
            data: data,
            descriptor: descriptor,
        }
    }

    pub fn into_data(self) -> (Vec<u8>, AuxiliaryListsDescriptor) {
        (self.data, self.descriptor)
    }

    pub fn data(&self) -> &[u8] {
        &self.data[..]
    }

    pub fn descriptor(&self) -> &AuxiliaryListsDescriptor {
        &self.descriptor
    }

    /// Returns the gradient stops described by `gradient_stops_range`.
    pub fn gradient_stops(&self, gradient_stops_range: &ItemRange) -> &[GradientStop] {
        unsafe {
            let end = self.descriptor.gradient_stops_size;
            gradient_stops_range.get(convert_blob_to_pod(&self.data[0..end]))
        }
    }

    /// Returns the complex clipping regions described by `complex_clip_regions_range`.
    pub fn complex_clip_regions(&self, complex_clip_regions_range: &ItemRange)
                                -> &[ComplexClipRegion] {
        let start = self.descriptor.gradient_stops_size;
        let end = start + self.descriptor.complex_clip_regions_size;
        unsafe {
            complex_clip_regions_range.get(convert_blob_to_pod(&self.data[start..end]))
        }
    }

    /// Returns the filters described by `filters_range`.
    pub fn filters(&self, filters_range: &ItemRange) -> &[FilterOp] {
        let start = self.descriptor.gradient_stops_size +
            self.descriptor.complex_clip_regions_size;
        let end = start + self.descriptor.filters_size;
        unsafe {
            filters_range.get(convert_blob_to_pod(&self.data[start..end]))
        }
    }

    /// Returns the glyph instances described by `glyph_instances_range`.
    pub fn glyph_instances(&self, glyph_instances_range: &ItemRange) -> &[GlyphInstance] {
        let start = self.descriptor.gradient_stops_size +
            self.descriptor.complex_clip_regions_size + self.descriptor.filters_size;
        unsafe {
            glyph_instances_range.get(convert_blob_to_pod(&self.data[start..]))
        }
    }
}

unsafe fn convert_pod_to_blob<T>(data: &[T]) -> &[u8] where T: Copy + 'static {
    slice::from_raw_parts(data.as_ptr() as *const u8, data.len() * mem::size_of::<T>())
}

// this variant of the above lets us convert without needing to make a copy
unsafe fn convert_vec_pod_to_blob<T>(mut data: Vec<T>) -> Vec<u8> where T: Copy + 'static {
    let v = Vec::from_raw_parts(data.as_mut_ptr() as *mut u8, data.len() * mem::size_of::<T>(), data.capacity() * mem::size_of::<T>());
    mem::forget(data);
    v
}

unsafe fn convert_blob_to_pod<T>(blob: &[u8]) -> &[T] where T: Copy + 'static {
    slice::from_raw_parts(blob.as_ptr() as *const T, blob.len() / mem::size_of::<T>())
}

// this variant of the above lets us convert without needing to make a copy
unsafe fn convert_vec_blob_to_pod<T>(mut data: Vec<u8>) -> Vec<T> where T: Copy + 'static {
    let v = Vec::from_raw_parts(data.as_mut_ptr() as *mut T, data.len() / mem::size_of::<T>(), data.capacity() / mem::size_of::<T>());
    mem::forget(data);
    v
}
