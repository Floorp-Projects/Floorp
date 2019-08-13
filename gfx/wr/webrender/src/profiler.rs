/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, ColorU};
use crate::debug_render::DebugRenderer;
use crate::device::query::{GpuSampler, GpuTimer, NamedTag};
use euclid::{Point2D, Rect, Size2D, vec2, default};
use crate::internal_types::FastHashMap;
use crate::renderer::{MAX_VERTEX_TEXTURE_WIDTH, wr_has_been_initialized};
use std::collections::vec_deque::VecDeque;
use std::{f32, mem};
use std::ffi::CStr;
use std::time::Duration;
use time::precise_time_ns;

const GRAPH_WIDTH: f32 = 1024.0;
const GRAPH_HEIGHT: f32 = 320.0;
const GRAPH_PADDING: f32 = 8.0;
const GRAPH_FRAME_HEIGHT: f32 = 16.0;
const PROFILE_PADDING: f32 = 10.0;

const ONE_SECOND_NS: u64 = 1000000000;

/// Defines the interface for hooking up an external profiler to WR.
pub trait ProfilerHooks : Send + Sync {
    /// Called at the beginning of a profile scope. The label must
    /// be a C string (null terminated).
    fn begin_marker(&self, label: &CStr);

    /// Called at the end of a profile scope. The label must
    /// be a C string (null terminated).
    fn end_marker(&self, label: &CStr);

    /// Called with a duration to indicate a text marker that just ended. Text
    /// markers allow different types of entries to be recorded on the same row
    /// in the timeline, by adding labels to the entry.
    ///
    /// This variant is also useful when the caller only wants to record events
    /// longer than a certain threshold, and thus they don't know in advance
    /// whether the event will qualify.
    fn add_text_marker(&self, label: &CStr, text: &str, duration: Duration);

    /// Returns true if the current thread is being profiled.
    fn thread_is_being_profiled(&self) -> bool;
}

/// The current global profiler callbacks, if set by embedder.
pub static mut PROFILER_HOOKS: Option<&'static dyn ProfilerHooks> = None;

/// Set the profiler callbacks, or None to disable the profiler.
/// This function must only ever be called before any WR instances
/// have been created, or the hooks will not be set.
pub fn set_profiler_hooks(hooks: Option<&'static dyn ProfilerHooks>) {
    if !wr_has_been_initialized() {
        unsafe {
            PROFILER_HOOKS = hooks;
        }
    }
}

/// A simple RAII style struct to manage a profile scope.
pub struct ProfileScope {
    name: &'static CStr,
}

/// Records a marker of the given duration that just ended.
pub fn add_text_marker(label: &CStr, text: &str, duration: Duration) {
    unsafe {
        if let Some(ref hooks) = PROFILER_HOOKS {
            hooks.add_text_marker(label, text, duration);
        }
    }
}

/// Returns true if the current thread is being profiled.
pub fn thread_is_being_profiled() -> bool {
    unsafe {
        PROFILER_HOOKS.map_or(false, |h| h.thread_is_being_profiled())
    }
}

impl ProfileScope {
    /// Begin a new profile scope
    pub fn new(name: &'static CStr) -> Self {
        unsafe {
            if let Some(ref hooks) = PROFILER_HOOKS {
                hooks.begin_marker(name);
            }
        }

        ProfileScope {
            name,
        }
    }
}

impl Drop for ProfileScope {
    fn drop(&mut self) {
        unsafe {
            if let Some(ref hooks) = PROFILER_HOOKS {
                hooks.end_marker(self.name);
            }
        }
    }
}

/// A helper macro to define profile scopes.
macro_rules! profile_marker {
    ($string:expr) => {
        let _scope = $crate::profiler::ProfileScope::new(cstr!($string));
    };
}

#[derive(Debug, Clone)]
pub struct GpuProfileTag {
    pub label: &'static str,
    pub color: ColorF,
}

impl NamedTag for GpuProfileTag {
    fn get_label(&self) -> &str {
        self.label
    }
}

trait ProfileCounter {
    fn description(&self) -> &'static str;
    fn value(&self) -> String;
}

#[derive(Clone)]
pub struct IntProfileCounter {
    description: &'static str,
    value: usize,
}

impl IntProfileCounter {
    fn new(description: &'static str) -> Self {
        IntProfileCounter {
            description,
            value: 0,
        }
    }

    fn reset(&mut self) {
        self.value = 0;
    }

    #[inline(always)]
    pub fn inc(&mut self) {
        self.value += 1;
    }

    #[inline(always)]
    pub fn add(&mut self, amount: usize) {
        self.value += amount;
    }

    #[inline(always)]
    pub fn set(&mut self, amount: usize) {
        self.value = amount;
    }

    pub fn get(&self) -> usize {
        self.value
    }
}

impl ProfileCounter for IntProfileCounter {
    fn description(&self) -> &'static str {
        self.description
    }

    fn value(&self) -> String {
        format!("{}", self.value)
    }
}

pub struct PercentageProfileCounter {
    description: &'static str,
    value: f32,
}

impl ProfileCounter for PercentageProfileCounter {
    fn description(&self) -> &'static str {
        self.description
    }

    fn value(&self) -> String {
        format!("{:.2}%", self.value * 100.0)
    }
}

#[derive(Clone)]
pub struct ResourceProfileCounter {
    description: &'static str,
    value: usize,
    size: usize,
}

impl ResourceProfileCounter {
    fn new(description: &'static str) -> Self {
        ResourceProfileCounter {
            description,
            value: 0,
            size: 0,
        }
    }

    #[allow(dead_code)]
    fn reset(&mut self) {
        self.value = 0;
        self.size = 0;
    }

    #[inline(always)]
    pub fn inc(&mut self, size: usize) {
        self.value += 1;
        self.size += size;
    }

    pub fn set(&mut self, count: usize, size: usize) {
        self.value = count;
        self.size = size;
    }
}

impl ProfileCounter for ResourceProfileCounter {
    fn description(&self) -> &'static str {
        self.description
    }

    fn value(&self) -> String {
        let size = self.size as f32 / (1024.0 * 1024.0);
        format!("{} ({:.2} MB)", self.value, size)
    }
}

#[derive(Clone)]
pub struct TimeProfileCounter {
    description: &'static str,
    nanoseconds: u64,
    invert: bool,
}

pub struct Timer<'a> {
    start: u64,
    result: &'a mut u64,
}

impl<'a> Drop for Timer<'a> {
    fn drop(&mut self) {
        let end = precise_time_ns();
        *self.result += end - self.start;
    }
}

impl TimeProfileCounter {
    pub fn new(description: &'static str, invert: bool) -> Self {
        TimeProfileCounter {
            description,
            nanoseconds: 0,
            invert,
        }
    }

    fn reset(&mut self) {
        self.nanoseconds = 0;
    }

    #[allow(dead_code)]
    pub fn set(&mut self, ns: u64) {
        self.nanoseconds = ns;
    }

    pub fn profile<T, F>(&mut self, callback: F) -> T
    where
        F: FnOnce() -> T,
    {
        let t0 = precise_time_ns();
        let val = callback();
        let t1 = precise_time_ns();
        let ns = t1 - t0;
        self.nanoseconds += ns;
        val
    }

    pub fn timer(&mut self) -> Timer {
        Timer {
            start: precise_time_ns(),
            result: &mut self.nanoseconds,
        }
    }

    pub fn inc(&mut self, ns: u64) {
        self.nanoseconds += ns;
    }

    pub fn get(&self) -> u64 {
        self.nanoseconds
    }
}

impl ProfileCounter for TimeProfileCounter {
    fn description(&self) -> &'static str {
        self.description
    }

    fn value(&self) -> String {
        if self.invert {
            format!("{:.2} fps", 1000000000.0 / self.nanoseconds as f64)
        } else {
            format!("{:.2} ms", self.nanoseconds as f64 / 1000000.0)
        }
    }
}

#[derive(Clone)]
pub struct AverageTimeProfileCounter {
    description: &'static str,
    average_over_ns: u64,
    start_ns: u64,
    sum_ns: u64,
    max_ns: u64,
    num_samples: u64,
    avg_nanoseconds: u64,
    // Maximum time over the averaging window.
    // When the timings are noisy, this is more representative of the perceived performance.
    max_nanoseconds: u64,
    invert: bool,
}

impl AverageTimeProfileCounter {
    pub fn new(description: &'static str, invert: bool, average_over_ns: u64) -> Self {
        AverageTimeProfileCounter {
            description,
            average_over_ns,
            start_ns: precise_time_ns(),
            sum_ns: 0,
            max_ns: 0,
            num_samples: 0,
            avg_nanoseconds: 0,
            max_nanoseconds: 0,
            invert,
        }
    }

    #[allow(dead_code)]
    fn reset(&mut self) {
        self.start_ns = precise_time_ns();
        self.avg_nanoseconds = 0;
        self.max_nanoseconds = 0;
        self.sum_ns = 0;
        self.num_samples = 0;
        self.max_ns = 0;
    }

    pub fn set(&mut self, ns: u64) {
        let now = precise_time_ns();
        if (now - self.start_ns) > self.average_over_ns && self.num_samples > 0 {
            self.avg_nanoseconds = self.sum_ns / self.num_samples;
            self.max_nanoseconds = self.max_ns;
            self.start_ns = now;
            self.sum_ns = 0;
            self.num_samples = 0;
            self.max_ns = 0;
        }
        self.max_ns = self.max_ns.max(ns);
        self.sum_ns += ns;
        self.num_samples += 1;
    }

    #[allow(dead_code)]
    pub fn profile<T, F>(&mut self, callback: F) -> T
    where
        F: FnOnce() -> T,
    {
        let t0 = precise_time_ns();
        let val = callback();
        let t1 = precise_time_ns();
        self.set(t1 - t0);
        val
    }
}

impl ProfileCounter for AverageTimeProfileCounter {
    fn description(&self) -> &'static str {
        self.description
    }

    fn value(&self) -> String {
        if self.invert {
            format!("{:.2} fps", 1000000000.0 / self.avg_nanoseconds as f64)
        } else {
            format!(
                "{:.2} ms (max {:.2} ms)",
                self.avg_nanoseconds as f64 / 1000000.0,
                self.max_nanoseconds as f64 / 1000000.0
            )
        }
    }
}


#[derive(Clone)]
pub struct FrameProfileCounters {
    pub total_primitives: IntProfileCounter,
    pub visible_primitives: IntProfileCounter,
    pub targets_used: IntProfileCounter,
    pub targets_changed: IntProfileCounter,
    pub targets_created: IntProfileCounter,
}

impl FrameProfileCounters {
    pub fn new() -> Self {
        FrameProfileCounters {
            total_primitives: IntProfileCounter::new("Total Primitives"),
            visible_primitives: IntProfileCounter::new("Visible Primitives"),
            targets_used: IntProfileCounter::new("Used targets"),
            targets_changed: IntProfileCounter::new("Changed targets"),
            targets_created: IntProfileCounter::new("Created targets"),
        }
    }
    pub fn reset_targets(&mut self) {
        self.targets_used.reset();
        self.targets_changed.reset();
        self.targets_created.reset();
    }
}

#[derive(Clone)]
pub struct TextureCacheProfileCounters {
    pub pages_alpha8_linear: ResourceProfileCounter,
    pub pages_alpha16_linear: ResourceProfileCounter,
    pub pages_color8_linear: ResourceProfileCounter,
    pub pages_color8_nearest: ResourceProfileCounter,
    pub pages_picture: ResourceProfileCounter,
}

impl TextureCacheProfileCounters {
    pub fn new() -> Self {
        TextureCacheProfileCounters {
            pages_alpha8_linear: ResourceProfileCounter::new("Texture A8 cached pages"),
            pages_alpha16_linear: ResourceProfileCounter::new("Texture A16 cached pages"),
            pages_color8_linear: ResourceProfileCounter::new("Texture RGBA8 cached pages (L)"),
            pages_color8_nearest: ResourceProfileCounter::new("Texture RGBA8 cached pages (N)"),
            pages_picture: ResourceProfileCounter::new("Picture cached pages"),
        }
    }
}

#[derive(Clone)]
pub struct GpuCacheProfileCounters {
    pub allocated_rows: IntProfileCounter,
    pub allocated_blocks: IntProfileCounter,
    pub updated_rows: IntProfileCounter,
    pub updated_blocks: IntProfileCounter,
    pub saved_blocks: IntProfileCounter,
}

impl GpuCacheProfileCounters {
    pub fn new() -> Self {
        GpuCacheProfileCounters {
            allocated_rows: IntProfileCounter::new("GPU cache rows: total"),
            updated_rows: IntProfileCounter::new("GPU cache rows: updated"),
            allocated_blocks: IntProfileCounter::new("GPU cache blocks: total"),
            updated_blocks: IntProfileCounter::new("GPU cache blocks: updated"),
            saved_blocks: IntProfileCounter::new("GPU cache blocks: saved"),
        }
    }
}

#[derive(Clone)]
pub struct BackendProfileCounters {
    pub total_time: TimeProfileCounter,
    pub resources: ResourceProfileCounters,
    pub ipc: IpcProfileCounters,
    pub intern: InternProfileCounters,
}

#[derive(Clone)]
pub struct ResourceProfileCounters {
    pub font_templates: ResourceProfileCounter,
    pub image_templates: ResourceProfileCounter,
    pub texture_cache: TextureCacheProfileCounters,
    pub gpu_cache: GpuCacheProfileCounters,
}

#[derive(Clone)]
pub struct IpcProfileCounters {
    pub build_time: TimeProfileCounter,
    pub consume_time: TimeProfileCounter,
    pub send_time: TimeProfileCounter,
    pub total_time: TimeProfileCounter,
    pub display_lists: ResourceProfileCounter,
}

macro_rules! declare_intern_profile_counters {
    ( $( $name:ident : $ty:ty, )+ ) => {
        #[derive(Clone)]
        pub struct InternProfileCounters {
            $(
                pub $name: ResourceProfileCounter,
            )+
        }

        impl InternProfileCounters {
            fn draw(
                &self,
                debug_renderer: &mut DebugRenderer,
                draw_state: &mut DrawState,
            ) {
                Profiler::draw_counters(
                    &[
                        $(
                            &self.$name,
                        )+
                    ],
                    debug_renderer,
                    true,
                    draw_state,
                );
            }
        }
    }
}

enumerate_interners!(declare_intern_profile_counters);

impl IpcProfileCounters {
    pub fn set(
        &mut self,
        build_start: u64,
        build_end: u64,
        send_start: u64,
        consume_start: u64,
        consume_end: u64,
        display_len: usize,
    ) {
        let build_time = build_end - build_start;
        let consume_time = consume_end - consume_start;
        let send_time = consume_start - send_start;
        self.build_time.inc(build_time);
        self.consume_time.inc(consume_time);
        self.send_time.inc(send_time);
        self.total_time.inc(build_time + consume_time + send_time);
        self.display_lists.inc(display_len);
    }
}

impl BackendProfileCounters {
    pub fn new() -> Self {
        BackendProfileCounters {
            total_time: TimeProfileCounter::new("Backend CPU Time", false),
            resources: ResourceProfileCounters {
                font_templates: ResourceProfileCounter::new("Font Templates"),
                image_templates: ResourceProfileCounter::new("Image Templates"),
                texture_cache: TextureCacheProfileCounters::new(),
                gpu_cache: GpuCacheProfileCounters::new(),
            },
            ipc: IpcProfileCounters {
                build_time: TimeProfileCounter::new("Display List Build Time", false),
                consume_time: TimeProfileCounter::new("Display List Consume Time", false),
                send_time: TimeProfileCounter::new("Display List Send Time", false),
                total_time: TimeProfileCounter::new("Total Display List Time", false),
                display_lists: ResourceProfileCounter::new("Display Lists Sent"),
            },
            //TODO: generate this by a macro
            intern: InternProfileCounters {
                prim: ResourceProfileCounter::new("Interned primitives"),
                image: ResourceProfileCounter::new("Interned images"),
                image_border: ResourceProfileCounter::new("Interned image borders"),
                line_decoration: ResourceProfileCounter::new("Interned line decorations"),
                linear_grad: ResourceProfileCounter::new("Interned linear gradients"),
                normal_border: ResourceProfileCounter::new("Interned normal borders"),
                picture: ResourceProfileCounter::new("Interned pictures"),
                radial_grad: ResourceProfileCounter::new("Interned radial gradients"),
                text_run: ResourceProfileCounter::new("Interned text runs"),
                yuv_image: ResourceProfileCounter::new("Interned YUV images"),
                clip: ResourceProfileCounter::new("Interned clips"),
                filter_data: ResourceProfileCounter::new("Interned filter data"),
                backdrop: ResourceProfileCounter::new("Interned backdrops"),
            },
        }
    }

    pub fn reset(&mut self) {
        self.total_time.reset();
        self.ipc.total_time.reset();
        self.ipc.build_time.reset();
        self.ipc.consume_time.reset();
        self.ipc.send_time.reset();
        self.ipc.display_lists.reset();
    }
}

pub struct RendererProfileCounters {
    pub frame_counter: IntProfileCounter,
    pub frame_time: AverageTimeProfileCounter,
    pub draw_calls: IntProfileCounter,
    pub vertices: IntProfileCounter,
    pub vao_count_and_size: ResourceProfileCounter,
    pub color_targets: IntProfileCounter,
    pub alpha_targets: IntProfileCounter,
    pub texture_data_uploaded: IntProfileCounter,
}

pub struct RendererProfileTimers {
    pub cpu_time: TimeProfileCounter,
    pub gpu_graph: TimeProfileCounter,
    pub gpu_samples: Vec<GpuTimer<GpuProfileTag>>,
}

impl RendererProfileCounters {
    pub fn new() -> Self {
        RendererProfileCounters {
            frame_counter: IntProfileCounter::new("Frame"),
            frame_time: AverageTimeProfileCounter::new("FPS", true, ONE_SECOND_NS / 2),
            draw_calls: IntProfileCounter::new("Draw Calls"),
            vertices: IntProfileCounter::new("Vertices"),
            vao_count_and_size: ResourceProfileCounter::new("VAO"),
            color_targets: IntProfileCounter::new("Color Targets"),
            alpha_targets: IntProfileCounter::new("Alpha Targets"),
            texture_data_uploaded: IntProfileCounter::new("Texture data, kb"),
        }
    }

    pub fn reset(&mut self) {
        self.draw_calls.reset();
        self.vertices.reset();
        self.color_targets.reset();
        self.alpha_targets.reset();
        self.texture_data_uploaded.reset();
    }
}

impl RendererProfileTimers {
    pub fn new() -> Self {
        RendererProfileTimers {
            cpu_time: TimeProfileCounter::new("Renderer CPU Time", false),
            gpu_samples: Vec::new(),
            gpu_graph: TimeProfileCounter::new("GPU Time", false),
        }
    }
}

struct GraphStats {
    min_value: f32,
    mean_value: f32,
    max_value: f32,
}

struct ProfileGraph {
    max_samples: usize,
    values: VecDeque<f32>,
    short_description: &'static str,
}

impl ProfileGraph {
    fn new(
        max_samples: usize,
        short_description: &'static str,
    ) -> Self {
        ProfileGraph {
            max_samples,
            values: VecDeque::new(),
            short_description,
        }
    }

    fn push(&mut self, ns: u64) {
        let ms = ns as f64 / 1000000.0;
        if self.values.len() == self.max_samples {
            self.values.pop_back();
        }
        self.values.push_front(ms as f32);
    }

    fn stats(&self) -> GraphStats {
        let mut stats = GraphStats {
            min_value: f32::MAX,
            mean_value: 0.0,
            max_value: -f32::MAX,
        };

        for value in &self.values {
            stats.min_value = stats.min_value.min(*value);
            stats.mean_value = stats.mean_value + *value;
            stats.max_value = stats.max_value.max(*value);
        }

        if !self.values.is_empty() {
            stats.mean_value = stats.mean_value / self.values.len() as f32;
        }

        stats
    }

    fn draw_graph(
        &self,
        x: f32,
        y: f32,
        description: &'static str,
        debug_renderer: &mut DebugRenderer,
    ) -> default::Rect<f32> {
        let size = Size2D::new(600.0, 120.0);
        let line_height = debug_renderer.line_height();
        let graph_rect = Rect::new(Point2D::new(x, y), size);
        let mut rect = graph_rect.inflate(10.0, 10.0);

        let stats = self.stats();

        let text_color = ColorU::new(255, 255, 0, 255);
        let text_origin = rect.origin + vec2(rect.size.width, 20.0);
        debug_renderer.add_text(
            text_origin.x,
            text_origin.y,
            description,
            ColorU::new(0, 255, 0, 255),
            None,
        );
        debug_renderer.add_text(
            text_origin.x,
            text_origin.y + line_height,
            &format!("Min: {:.2} ms", stats.min_value),
            text_color,
            None,
        );
        debug_renderer.add_text(
            text_origin.x,
            text_origin.y + line_height * 2.0,
            &format!("Mean: {:.2} ms", stats.mean_value),
            text_color,
            None,
        );
        debug_renderer.add_text(
            text_origin.x,
            text_origin.y + line_height * 3.0,
            &format!("Max: {:.2} ms", stats.max_value),
            text_color,
            None,
        );

        rect.size.width += 140.0;
        debug_renderer.add_quad(
            rect.origin.x,
            rect.origin.y,
            rect.origin.x + rect.size.width + 10.0,
            rect.origin.y + rect.size.height,
            ColorU::new(25, 25, 25, 200),
            ColorU::new(51, 51, 51, 200),
        );

        let bx1 = graph_rect.max_x();
        let by1 = graph_rect.max_y();

        let w = graph_rect.size.width / self.max_samples as f32;
        let h = graph_rect.size.height;

        let color_t0 = ColorU::new(0, 255, 0, 255);
        let color_b0 = ColorU::new(0, 180, 0, 255);

        let color_t1 = ColorU::new(0, 255, 0, 255);
        let color_b1 = ColorU::new(0, 180, 0, 255);

        let color_t2 = ColorU::new(255, 0, 0, 255);
        let color_b2 = ColorU::new(180, 0, 0, 255);

        for (index, sample) in self.values.iter().enumerate() {
            let sample = *sample;
            let x1 = bx1 - index as f32 * w;
            let x0 = x1 - w;

            let y0 = by1 - (sample / stats.max_value) as f32 * h;
            let y1 = by1;

            let (color_top, color_bottom) = if sample < 1000.0 / 60.0 {
                (color_t0, color_b0)
            } else if sample < 1000.0 / 30.0 {
                (color_t1, color_b1)
            } else {
                (color_t2, color_b2)
            };

            debug_renderer.add_quad(x0, y0, x1, y1, color_top, color_bottom);
        }

        rect
    }
}

impl ProfileCounter for ProfileGraph {
    fn description(&self) -> &'static str {
        self.short_description
    }

    fn value(&self) -> String {
        format!("{:.2}ms", self.stats().mean_value)
    }
}

struct GpuFrame {
    total_time: u64,
    samples: Vec<GpuTimer<GpuProfileTag>>,
}

struct GpuFrameCollection {
    frames: VecDeque<GpuFrame>,
}

impl GpuFrameCollection {
    fn new() -> Self {
        GpuFrameCollection {
            frames: VecDeque::new(),
        }
    }

    fn push(&mut self, total_time: u64, samples: Vec<GpuTimer<GpuProfileTag>>) {
        if self.frames.len() == 20 {
            self.frames.pop_back();
        }
        self.frames.push_front(GpuFrame {
            total_time,
            samples,
        });
    }
}

impl GpuFrameCollection {
    fn draw(&self, x: f32, y: f32, debug_renderer: &mut DebugRenderer) -> default::Rect<f32> {
        let graph_rect = Rect::new(
            Point2D::new(x, y),
            Size2D::new(GRAPH_WIDTH, GRAPH_HEIGHT),
        );
        let bounding_rect = graph_rect.inflate(GRAPH_PADDING, GRAPH_PADDING);

        debug_renderer.add_quad(
            bounding_rect.origin.x,
            bounding_rect.origin.y,
            bounding_rect.origin.x + bounding_rect.size.width,
            bounding_rect.origin.y + bounding_rect.size.height,
            ColorU::new(25, 25, 25, 200),
            ColorU::new(51, 51, 51, 200),
        );

        let w = graph_rect.size.width;
        let mut y0 = graph_rect.origin.y;

        let max_time = self.frames
            .iter()
            .max_by_key(|f| f.total_time)
            .unwrap()
            .total_time as f32;

        let mut tags_present = FastHashMap::default();

        for frame in &self.frames {
            let y1 = y0 + GRAPH_FRAME_HEIGHT;

            let mut current_ns = 0;
            for sample in &frame.samples {
                let x0 = graph_rect.origin.x + w * current_ns as f32 / max_time;
                current_ns += sample.time_ns;
                let x1 = graph_rect.origin.x + w * current_ns as f32 / max_time;
                let mut bottom_color = sample.tag.color;
                bottom_color.a *= 0.5;

                debug_renderer.add_quad(
                    x0,
                    y0,
                    x1,
                    y1,
                    sample.tag.color.into(),
                    bottom_color.into(),
                );

                tags_present.insert(sample.tag.label, sample.tag.color);
            }

            y0 = y1;
        }

        // Add a legend to see which color correspond to what primitive.
        const LEGEND_SIZE: f32 = 20.0;
        const PADDED_LEGEND_SIZE: f32 = 25.0;
        if !tags_present.is_empty() {
            debug_renderer.add_quad(
                bounding_rect.max_x() + GRAPH_PADDING,
                bounding_rect.origin.y,
                bounding_rect.max_x() + GRAPH_PADDING + 200.0,
                bounding_rect.origin.y + tags_present.len() as f32 * PADDED_LEGEND_SIZE + GRAPH_PADDING,
                ColorU::new(25, 25, 25, 200),
                ColorU::new(51, 51, 51, 200),
            );
        }

        for (i, (label, &color)) in tags_present.iter().enumerate() {
            let x0 = bounding_rect.origin.x + bounding_rect.size.width + GRAPH_PADDING * 2.0;
            let y0 = bounding_rect.origin.y + GRAPH_PADDING + i as f32 * PADDED_LEGEND_SIZE;

            debug_renderer.add_quad(
                x0, y0, x0 + LEGEND_SIZE, y0 + LEGEND_SIZE,
                color.into(),
                color.into(),
            );

            debug_renderer.add_text(
                x0 + PADDED_LEGEND_SIZE,
                y0 + LEGEND_SIZE * 0.75,
                label,
                ColorU::new(255, 255, 0, 255),
                None,
            );
        }

        bounding_rect
    }
}

struct DrawState {
    x_left: f32,
    y_left: f32,
    x_right: f32,
    y_right: f32,
}

pub struct Profiler {
    draw_state: DrawState,
    backend_graph: ProfileGraph,
    renderer_graph: ProfileGraph,
    gpu_graph: ProfileGraph,
    ipc_graph: ProfileGraph,
    backend_time: AverageTimeProfileCounter,
    renderer_time: AverageTimeProfileCounter,
    gpu_time: AverageTimeProfileCounter,
    ipc_time: AverageTimeProfileCounter,
    gpu_frames: GpuFrameCollection,
}

impl Profiler {
    pub fn new() -> Self {
        Profiler {
            draw_state: DrawState {
                x_left: 0.0,
                y_left: 0.0,
                x_right: 0.0,
                y_right: 0.0,
            },
            backend_graph: ProfileGraph::new(600, "Backend:"),
            renderer_graph: ProfileGraph::new(600, "Renderer:"),
            gpu_graph: ProfileGraph::new(600, "GPU:"),
            ipc_graph: ProfileGraph::new(600, "IPC:"),
            gpu_frames: GpuFrameCollection::new(),
            backend_time: AverageTimeProfileCounter::new("Backend:", false, ONE_SECOND_NS / 2),
            renderer_time: AverageTimeProfileCounter::new("Renderer:", false, ONE_SECOND_NS / 2),
            ipc_time: AverageTimeProfileCounter::new("IPC:", false, ONE_SECOND_NS / 2),
            gpu_time: AverageTimeProfileCounter::new("GPU:", false, ONE_SECOND_NS / 2),
        }
    }

    fn draw_counters<T: ProfileCounter + ?Sized>(
        counters: &[&T],
        debug_renderer: &mut DebugRenderer,
        left: bool,
        draw_state: &mut DrawState,
    ) {
        let mut label_rect = Rect::zero();
        let mut value_rect = Rect::zero();
        let (mut current_x, mut current_y) = if left {
            (draw_state.x_left, draw_state.y_left)
        } else {
            (draw_state.x_right, draw_state.y_right)
        };
        let mut color_index = 0;
        let line_height = debug_renderer.line_height();

        let colors = [
            ColorU::new(255, 255, 255, 255),
            ColorU::new(255, 255, 0, 255),
        ];

        for counter in counters {
            let rect = debug_renderer.add_text(
                current_x,
                current_y,
                counter.description(),
                colors[color_index],
                None,
            );
            color_index = (color_index + 1) % colors.len();

            label_rect = label_rect.union(&rect);
            current_y += line_height;
        }

        color_index = 0;
        current_x = label_rect.origin.x + label_rect.size.width + 60.0;
        current_y = if left { draw_state.y_left } else { draw_state.y_right };

        for counter in counters {
            let rect = debug_renderer.add_text(
                current_x,
                current_y,
                &counter.value(),
                colors[color_index],
                None,
            );
            color_index = (color_index + 1) % colors.len();

            value_rect = value_rect.union(&rect);
            current_y += line_height;
        }

        let total_rect = label_rect.union(&value_rect).inflate(10.0, 10.0);
        debug_renderer.add_quad(
            total_rect.origin.x,
            total_rect.origin.y,
            total_rect.origin.x + total_rect.size.width,
            total_rect.origin.y + total_rect.size.height,
            ColorF::new(0.1, 0.1, 0.1, 0.8).into(),
            ColorF::new(0.2, 0.2, 0.2, 0.8).into(),
        );
        let new_y = total_rect.origin.y + total_rect.size.height + 30.0;
        if left {
            draw_state.y_left = new_y;
        } else {
            draw_state.y_right = new_y;
        }
    }

    fn draw_bar(
        &mut self,
        label: &str,
        label_color: ColorU,
        counters: &[(ColorU, &IntProfileCounter)],
        debug_renderer: &mut DebugRenderer,
    ) -> default::Rect<f32> {
        let mut rect = debug_renderer.add_text(
            self.draw_state.x_left,
            self.draw_state.y_left,
            label,
            label_color,
            None,
        );

        let x_base = rect.origin.x + rect.size.width + 10.0;
        let height = debug_renderer.line_height();
        let width = (self.draw_state.x_right - 30.0 - x_base).max(0.0);
        let total_value = counters.last().unwrap().1.value;
        let scale = width / total_value as f32;
        let mut x_current = x_base;

        for &(color, counter) in counters {
            let x_stop = x_base + counter.value as f32 * scale;
            debug_renderer.add_quad(
                x_current,
                rect.origin.y,
                x_stop,
                rect.origin.y + height,
                color,
                color,
            );
            x_current = x_stop;
        }

        self.draw_state.y_left += height;

        rect.size.width += width + 10.0;
        rect
    }

    fn draw_gpu_cache_bars(
        &mut self,
        counters: &GpuCacheProfileCounters,
        debug_renderer: &mut DebugRenderer,
    ) {
        let color_updated = ColorU::new(0xFF, 0, 0, 0xFF);
        let color_free = ColorU::new(0, 0, 0xFF, 0xFF);
        let color_saved = ColorU::new(0, 0xFF, 0, 0xFF);

        let requested_blocks = IntProfileCounter {
            description: "",
            value: counters.updated_blocks.value + counters.saved_blocks.value,
        };
        let total_blocks = IntProfileCounter {
            description: "",
            value: counters.allocated_rows.value * MAX_VERTEX_TEXTURE_WIDTH,
        };

        let rect0 = self.draw_bar(
            &format!("GPU cache rows ({}):", counters.allocated_rows.value),
            ColorU::new(0xFF, 0xFF, 0xFF, 0xFF),
            &[
                (color_updated, &counters.updated_rows),
                (color_free, &counters.allocated_rows),
            ],
            debug_renderer,
        );

        let rect1 = self.draw_bar(
            "GPU cache blocks",
            ColorU::new(0xFF, 0xFF, 0, 0xFF),
            &[
                (color_updated, &counters.updated_blocks),
                (color_saved, &requested_blocks),
                (color_free, &counters.allocated_blocks),
                (ColorU::new(0, 0, 0, 0xFF), &total_blocks),
            ],
            debug_renderer,
        );

        let total_rect = rect0.union(&rect1).inflate(10.0, 10.0);
        debug_renderer.add_quad(
            total_rect.origin.x,
            total_rect.origin.y,
            total_rect.origin.x + total_rect.size.width,
            total_rect.origin.y + total_rect.size.height,
            ColorF::new(0.1, 0.1, 0.1, 0.8).into(),
            ColorF::new(0.2, 0.2, 0.2, 0.8).into(),
        );

        self.draw_state.y_left = total_rect.origin.y + total_rect.size.height + 30.0;
    }

    fn draw_frame_bars(
        &mut self,
        counters: &FrameProfileCounters,
        debug_renderer: &mut DebugRenderer,
    ) {
        let rect0 = self.draw_bar(
            &format!("primitives ({}):", counters.total_primitives.value),
            ColorU::new(0xFF, 0xFF, 0xFF, 0xFF),
            &[
                (ColorU::new(0, 0, 0xFF, 0xFF), &counters.visible_primitives),
                (ColorU::new(0, 0, 0, 0xFF), &counters.total_primitives),
            ],
            debug_renderer,
        );

        let rect1 = self.draw_bar(
            &format!("GPU targets ({}):", &counters.targets_used.value),
            ColorU::new(0xFF, 0xFF, 0, 0xFF),
            &[
                (ColorU::new(0, 0, 0xFF, 0xFF), &counters.targets_created),
                (ColorU::new(0xFF, 0, 0, 0xFF), &counters.targets_changed),
                (ColorU::new(0, 0xFF, 0, 0xFF), &counters.targets_used),
            ],
            debug_renderer,
        );

        let total_rect = rect0.union(&rect1).inflate(10.0, 10.0);
        debug_renderer.add_quad(
            total_rect.origin.x,
            total_rect.origin.y,
            total_rect.origin.x + total_rect.size.width,
            total_rect.origin.y + total_rect.size.height,
            ColorF::new(0.1, 0.1, 0.1, 0.8).into(),
            ColorF::new(0.2, 0.2, 0.2, 0.8).into(),
        );

        self.draw_state.y_left = total_rect.origin.y + total_rect.size.height + 30.0;
    }

    fn draw_compact_profile(
        &mut self,
        renderer_profile: &RendererProfileCounters,
        debug_renderer: &mut DebugRenderer,
    ) {
        Profiler::draw_counters(
            &[
                &renderer_profile.frame_time as &dyn ProfileCounter,
                &renderer_profile.color_targets,
                &renderer_profile.alpha_targets,
                &renderer_profile.draw_calls,
                &renderer_profile.vertices,
                &renderer_profile.texture_data_uploaded,
                &self.ipc_time,
                &self.backend_time,
                &self.renderer_time,
                &self.gpu_time,
            ],
            debug_renderer,
            true,
            &mut self.draw_state,
        );
    }

    fn draw_full_profile(
        &mut self,
        frame_profiles: &[FrameProfileCounters],
        backend_profile: &BackendProfileCounters,
        renderer_profile: &RendererProfileCounters,
        renderer_timers: &mut RendererProfileTimers,
        gpu_samplers: &[GpuSampler<GpuProfileTag>],
        screen_fraction: f32,
        debug_renderer: &mut DebugRenderer,
    ) {
        Profiler::draw_counters(
            &[
                &renderer_profile.frame_time as &dyn ProfileCounter,
                &renderer_profile.frame_counter,
                &renderer_profile.color_targets,
                &renderer_profile.alpha_targets,
                &renderer_profile.texture_data_uploaded,
            ],
            debug_renderer,
            true,
            &mut self.draw_state
        );

        self.draw_gpu_cache_bars(
            &backend_profile.resources.gpu_cache,
            debug_renderer,
        );

        Profiler::draw_counters(
            &[
                &backend_profile.resources.font_templates,
                &backend_profile.resources.image_templates,
            ],
            debug_renderer,
            true,
            &mut self.draw_state
        );

        backend_profile.intern.draw(debug_renderer, &mut self.draw_state);

        Profiler::draw_counters(
            &[
                &backend_profile.resources.texture_cache.pages_alpha8_linear,
                &backend_profile.resources.texture_cache.pages_color8_linear,
                &backend_profile.resources.texture_cache.pages_color8_nearest,
                &backend_profile.ipc.display_lists,
            ],
            debug_renderer,
            true,
            &mut self.draw_state
        );

        Profiler::draw_counters(
            &[
                &backend_profile.ipc.build_time,
                &backend_profile.ipc.send_time,
                &backend_profile.ipc.consume_time,
                &backend_profile.ipc.total_time,
            ],
            debug_renderer,
            true,
            &mut self.draw_state
        );

        for frame_profile in frame_profiles {
            self.draw_frame_bars(frame_profile, debug_renderer);
        }

        Profiler::draw_counters(
            &[&renderer_profile.draw_calls, &renderer_profile.vertices],
            debug_renderer,
            true,
            &mut self.draw_state
        );

        Profiler::draw_counters(
            &[
                &backend_profile.total_time,
                &renderer_timers.cpu_time,
                &renderer_timers.gpu_graph,
            ],
            debug_renderer,
            false,
            &mut self.draw_state
        );

        if !gpu_samplers.is_empty() {
            let mut samplers = Vec::<PercentageProfileCounter>::new();
            // Gathering unique GPU samplers. This has O(N^2) complexity,
            // but we only have a few samplers per target.
            let mut total = 0.0;
            for sampler in gpu_samplers {
                let value = sampler.count as f32 * screen_fraction;
                total += value;
                match samplers.iter().position(|s| {
                    s.description as *const _ == sampler.tag.label as *const _
                }) {
                    Some(pos) => samplers[pos].value += value,
                    None => samplers.push(PercentageProfileCounter {
                        description: sampler.tag.label,
                        value,
                    }),
                }
            }
            samplers.push(PercentageProfileCounter {
                description: "Total",
                value: total,
            });
            let samplers: Vec<&dyn ProfileCounter> = samplers.iter().map(|sampler| {
                sampler as &dyn ProfileCounter
            }).collect();
            Profiler::draw_counters(
                &samplers,
                debug_renderer,
                false,
                &mut self.draw_state,
            );
        }

        let rect =
            self.backend_graph
                .draw_graph(self.draw_state.x_right, self.draw_state.y_right, "CPU (backend)", debug_renderer);
        self.draw_state.y_right += rect.size.height + PROFILE_PADDING;
        let rect = self.renderer_graph.draw_graph(
            self.draw_state.x_right,
            self.draw_state.y_right,
            "CPU (renderer)",
            debug_renderer,
        );
        self.draw_state.y_right += rect.size.height + PROFILE_PADDING;
        let rect =
            self.ipc_graph
                .draw_graph(self.draw_state.x_right, self.draw_state.y_right, "DisplayList IPC", debug_renderer);
        self.draw_state.y_right += rect.size.height + PROFILE_PADDING;
        let rect = self.gpu_graph
            .draw_graph(self.draw_state.x_right, self.draw_state.y_right, "GPU", debug_renderer);
        self.draw_state.y_right += rect.size.height + PROFILE_PADDING;
        let rect = self.gpu_frames
            .draw(self.draw_state.x_left, f32::max(self.draw_state.y_left, self.draw_state.y_right), debug_renderer);
        self.draw_state.y_right += rect.size.height + PROFILE_PADDING;
    }

    pub fn draw_profile(
        &mut self,
        frame_profiles: &[FrameProfileCounters],
        backend_profile: &BackendProfileCounters,
        renderer_profile: &RendererProfileCounters,
        renderer_timers: &mut RendererProfileTimers,
        gpu_samplers: &[GpuSampler<GpuProfileTag>],
        screen_fraction: f32,
        debug_renderer: &mut DebugRenderer,
        compact: bool,
    ) {
        self.draw_state.x_left = 20.0;
        self.draw_state.y_left = 40.0;
        self.draw_state.x_right = 450.0;
        self.draw_state.y_right = 40.0;

        let mut gpu_graph = 0;
        let gpu_graphrs = mem::replace(&mut renderer_timers.gpu_samples, Vec::new());
        for sample in &gpu_graphrs {
            gpu_graph += sample.time_ns;
        }
        renderer_timers.gpu_graph.set(gpu_graph);

        self.backend_graph
            .push(backend_profile.total_time.nanoseconds);
        self.backend_time.set(backend_profile.total_time.nanoseconds);
        self.renderer_graph
            .push(renderer_timers.cpu_time.nanoseconds);
        self.renderer_time.set(renderer_timers.cpu_time.nanoseconds);
        self.ipc_graph
            .push(backend_profile.ipc.total_time.nanoseconds);
        self.ipc_time.set(backend_profile.ipc.total_time.nanoseconds);
        self.gpu_graph.push(gpu_graph);
        self.gpu_time.set(gpu_graph);
        self.gpu_frames.push(gpu_graph, gpu_graphrs);

        if compact {
            self.draw_compact_profile(
                renderer_profile,
                debug_renderer,
            );
        } else {
            self.draw_full_profile(
                frame_profiles,
                backend_profile,
                renderer_profile,
                renderer_timers,
                gpu_samplers,
                screen_fraction,
                debug_renderer,
            );
        }
    }
}

pub struct ChangeIndicator {
    counter: u32,
}

impl ChangeIndicator {
    pub fn new() -> Self {
        ChangeIndicator {
            counter: 0
        }
    }

    pub fn changed(&mut self) {
        self.counter = (self.counter + 1) % 15;
    }

    const WIDTH : f32 = 20.0;
    const HEIGHT: f32 = 10.0;

    pub fn width() -> f32 {
      ChangeIndicator::WIDTH * 16.0
    }

    pub fn draw(
        &self,
        x: f32, y: f32,
        color: ColorU,
        debug_renderer: &mut DebugRenderer
    ) {
        let margin = 0.0;
        let tx = self.counter as f32 * ChangeIndicator::WIDTH;
        debug_renderer.add_quad(
            x - margin,
            y - margin,
            x + 15.0 * ChangeIndicator::WIDTH + margin,
            y + ChangeIndicator::HEIGHT + margin,
            ColorU::new(0, 0, 0, 150),
            ColorU::new(0, 0, 0, 150),
        );

        debug_renderer.add_quad(
            x + tx,
            y,
            x + tx + ChangeIndicator::WIDTH,
            y + ChangeIndicator::HEIGHT,
            color,
            ColorU::new(25, 25, 25, 255),
        );
    }
}
