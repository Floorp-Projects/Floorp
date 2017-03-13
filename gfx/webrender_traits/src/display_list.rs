/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use std::mem;
use std::slice;
use {BorderDetails, BorderDisplayItem, BorderWidths, BoxShadowClipMode, BoxShadowDisplayItem};
use {ClipDisplayItem, ClipRegion, ColorF, ComplexClipRegion, DisplayItem, ExtendMode, FilterOp};
use {FontKey, GlyphInstance, GlyphOptions, Gradient, GradientDisplayItem, GradientStop};
use {IframeDisplayItem, ImageDisplayItem, ImageKey, ImageMask, ImageRendering, ItemRange};
use {LayoutPoint, LayoutRect, LayoutSize, LayoutTransform, MixBlendMode, PipelineId};
use {PropertyBinding, PushStackingContextDisplayItem, RadialGradient, RadialGradientDisplayItem};
use {RectangleDisplayItem, ScrollLayerId, ScrollPolicy, ServoScrollRootId, SpecificDisplayItem};
use {StackingContext, TextDisplayItem, WebGLContextId, WebGLDisplayItem, YuvColorSpace};
use YuvImageDisplayItem;

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

    pub fn data(&self) -> &[u8] {
        &self.data[..]
    }

    pub fn descriptor(&self) -> &BuiltDisplayListDescriptor {
        &self.descriptor
    }

    pub fn all_display_items<'a>(&'a self) -> &'a [DisplayItem] {
        unsafe {
            convert_blob_to_pod(&self.data[0..self.descriptor.display_list_items_size])
        }
    }
}

#[derive(Clone)]
pub struct DisplayListBuilder {
    pub list: Vec<DisplayItem>,
    auxiliary_lists_builder: AuxiliaryListsBuilder,
    pub pipeline_id: PipelineId,
    clip_stack: Vec<ScrollLayerId>,
    next_scroll_layer_id: usize,
}

impl DisplayListBuilder {
    pub fn new(pipeline_id: PipelineId) -> DisplayListBuilder {
        DisplayListBuilder {
            list: Vec::new(),
            auxiliary_lists_builder: AuxiliaryListsBuilder::new(),
            pipeline_id: pipeline_id,
            clip_stack: vec![ScrollLayerId::root_scroll_layer(pipeline_id)],

            // We start at 1 here, because the root scroll id is always 0.
            next_scroll_layer_id: 1,
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
            scroll_layer_id: *self.clip_stack.last().unwrap(),
        });
    }

    fn push_new_empty_item(&mut self, item: SpecificDisplayItem) {
        self.list.push(DisplayItem {
            item: item,
            rect: LayoutRect::zero(),
            clip: ClipRegion::simple(&LayoutRect::zero()),
            scroll_layer_id: *self.clip_stack.last().unwrap(),
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

    pub fn push_yuv_image(&mut self,
                          rect: LayoutRect,
                          clip: ClipRegion,
                          y_key: ImageKey,
                          u_key: ImageKey,
                          v_key: ImageKey,
                          color_space: YuvColorSpace) {
        let item = SpecificDisplayItem::YuvImage(YuvImageDisplayItem {
                y_image_key: y_key,
                u_image_key: u_key,
                v_image_key: v_key,
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
                     glyphs: Vec<GlyphInstance>,
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

    pub fn create_gradient(&mut self,
                           start_point: LayoutPoint,
                           end_point: LayoutPoint,
                           stops: Vec<GradientStop>,
                           extend_mode: ExtendMode) -> Gradient {
        Gradient {
            start_point: start_point,
            end_point: end_point,
            stops: self.auxiliary_lists_builder.add_gradient_stops(&stops),
            extend_mode: extend_mode,
        }
    }

    pub fn create_radial_gradient(&mut self,
                                  start_center: LayoutPoint,
                                  start_radius: f32,
                                  end_center: LayoutPoint,
                                  end_radius: f32,
                                  stops: Vec<GradientStop>,
                                  extend_mode: ExtendMode) -> RadialGradient {
        RadialGradient {
            start_center: start_center,
            start_radius: start_radius,
            end_center: end_center,
            end_radius: end_radius,
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
                         start_point: LayoutPoint,
                         end_point: LayoutPoint,
                         stops: Vec<GradientStop>,
                         extend_mode: ExtendMode) {
        let item = SpecificDisplayItem::Gradient(GradientDisplayItem {
            gradient: self.create_gradient(start_point, end_point, stops, extend_mode),
        });

        self.push_item(item, rect, clip);
    }

    pub fn push_radial_gradient(&mut self,
                                rect: LayoutRect,
                                clip: ClipRegion,
                                start_center: LayoutPoint,
                                start_radius: f32,
                                end_center: LayoutPoint,
                                end_radius: f32,
                                stops: Vec<GradientStop>,
                                extend_mode: ExtendMode) {
        let item = SpecificDisplayItem::RadialGradient(RadialGradientDisplayItem {
            gradient: self.create_radial_gradient(start_center, start_radius,
                                                  end_center, end_radius,
                                                  stops, extend_mode),
        });

        self.push_item(item, rect, clip);
    }

    pub fn push_stacking_context(&mut self,
                                 scroll_policy: ScrollPolicy,
                                 bounds: LayoutRect,
                                 clip: ClipRegion,
                                 z_index: i32,
                                 transform: Option<PropertyBinding<LayoutTransform>>,
                                 perspective: Option<LayoutTransform>,
                                 mix_blend_mode: MixBlendMode,
                                 filters: Vec<FilterOp>) {
        let item = SpecificDisplayItem::PushStackingContext(PushStackingContextDisplayItem {
            stacking_context: StackingContext {
                scroll_policy: scroll_policy,
                bounds: bounds,
                z_index: z_index,
                transform: transform,
                perspective: perspective,
                mix_blend_mode: mix_blend_mode,
                filters: self.auxiliary_lists_builder.add_filters(&filters),
            }
        });

        self.push_item(item, LayoutRect::zero(), clip);
    }

    pub fn pop_stacking_context(&mut self) {
        self.push_new_empty_item(SpecificDisplayItem::PopStackingContext);
    }

    pub fn define_clip(&mut self,
                       clip: ClipRegion,
                       content_size: LayoutSize,
                       scroll_root_id: Option<ServoScrollRootId>)
                       -> ScrollLayerId {
        let scroll_layer_id = self.next_scroll_layer_id;
        self.next_scroll_layer_id += 1;

        let id = ScrollLayerId::new(self.pipeline_id, scroll_layer_id);
        let item = SpecificDisplayItem::Clip(ClipDisplayItem {
            content_size: content_size,
            id: id,
            parent_id: *self.clip_stack.last().unwrap(),
            scroll_root_id: scroll_root_id,
        });

        self.push_item(item, clip.main, clip);
        id
    }

    pub fn push_scroll_layer(&mut self,
                             clip: ClipRegion,
                             content_size: LayoutSize,
                             scroll_root_id: Option<ServoScrollRootId>) {
        let id = self.define_clip(clip, content_size, scroll_root_id);
        self.clip_stack.push(id);
    }

    pub fn push_clip_id(&mut self, id: ScrollLayerId) {
        self.clip_stack.push(id);
    }

    pub fn pop_clip_id(&mut self) {
        self.clip_stack.pop();
        assert!(self.clip_stack.len() > 0);
    }

    pub fn pop_scroll_layer(&mut self) {
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
            i.clip.complex = self.auxiliary_lists_builder.add_complex_clip_regions(aux.complex_clip_regions(&i.clip.complex));
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
            let blob = convert_pod_to_blob(&self.list).to_vec();
            let display_list_items_size = blob.len();

            (self.pipeline_id,
             BuiltDisplayList {
                 descriptor: BuiltDisplayListDescriptor {
                     display_list_items_size: display_list_items_size,
                 },
                 data: blob,
             },
             self.auxiliary_lists_builder.finalize())
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

#[derive(Clone)]
pub struct AuxiliaryListsBuilder {
    gradient_stops: Vec<GradientStop>,
    complex_clip_regions: Vec<ComplexClipRegion>,
    filters: Vec<FilterOp>,
    glyph_instances: Vec<GlyphInstance>,
}

impl AuxiliaryListsBuilder {
    pub fn new() -> AuxiliaryListsBuilder {
        AuxiliaryListsBuilder {
            gradient_stops: Vec::new(),
            complex_clip_regions: Vec::new(),
            filters: Vec::new(),
            glyph_instances: Vec::new(),
        }
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
            let mut blob = convert_pod_to_blob(&self.gradient_stops).to_vec();
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

unsafe fn convert_blob_to_pod<T>(blob: &[u8]) -> &[T] where T: Copy + 'static {
    slice::from_raw_parts(blob.as_ptr() as *const T, blob.len() / mem::size_of::<T>())
}

