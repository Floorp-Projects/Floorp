/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use gleam::gl;
#[cfg(feature = "debug_renderer")]
use std::mem;
use std::rc::Rc;

use device::FrameId;


pub trait NamedTag {
    fn get_label(&self) -> &str;
}

#[derive(Debug, Clone)]
pub struct GpuTimer<T> {
    pub tag: T,
    pub time_ns: u64,
}

#[derive(Debug, Clone)]
pub struct GpuSampler<T> {
    pub tag: T,
    pub count: u64,
}

pub struct QuerySet<T> {
    set: Vec<gl::GLuint>,
    data: Vec<T>,
    pending: gl::GLuint,
}

impl<T> QuerySet<T> {
    fn new() -> Self {
        QuerySet {
            set: Vec::new(),
            data: Vec::new(),
            pending: 0,
        }
    }

    fn reset(&mut self) {
        self.data.clear();
        self.pending = 0;
    }

    fn add(&mut self, value: T) -> Option<gl::GLuint> {
        assert_eq!(self.pending, 0);
        self.set.get(self.data.len()).cloned().map(|query_id| {
            self.data.push(value);
            self.pending = query_id;
            query_id
        })
    }

    #[cfg(feature = "debug_renderer")]
    fn take<F: Fn(&mut T, gl::GLuint)>(&mut self, fun: F) -> Vec<T> {
        let mut data = mem::replace(&mut self.data, Vec::new());
        for (value, &query) in data.iter_mut().zip(self.set.iter()) {
            fun(value, query)
        }
        data
    }
}

pub struct GpuFrameProfile<T> {
    gl: Rc<gl::Gl>,
    timers: QuerySet<GpuTimer<T>>,
    samplers: QuerySet<GpuSampler<T>>,
    frame_id: FrameId,
    inside_frame: bool,
}

impl<T> GpuFrameProfile<T> {
    fn new(gl: Rc<gl::Gl>) -> Self {
        GpuFrameProfile {
            gl,
            timers: QuerySet::new(),
            samplers: QuerySet::new(),
            frame_id: FrameId::new(0),
            inside_frame: false,
        }
    }

    fn enable_timers(&mut self, count: i32) {
        self.timers.set = self.gl.gen_queries(count);
    }

    fn disable_timers(&mut self) {
        if !self.timers.set.is_empty() {
            self.gl.delete_queries(&self.timers.set);
        }
        self.timers.set = Vec::new();
    }

    fn enable_samplers(&mut self, count: i32) {
        self.samplers.set = self.gl.gen_queries(count);
    }

    fn disable_samplers(&mut self) {
        if !self.samplers.set.is_empty() {
            self.gl.delete_queries(&self.samplers.set);
        }
        self.samplers.set = Vec::new();
    }

    fn begin_frame(&mut self, frame_id: FrameId) {
        self.frame_id = frame_id;
        self.timers.reset();
        self.samplers.reset();
        self.inside_frame = true;
    }

    fn end_frame(&mut self) {
        self.finish_timer();
        self.finish_sampler();
        self.inside_frame = false;
    }

    fn finish_timer(&mut self) {
        debug_assert!(self.inside_frame);
        if self.timers.pending != 0 {
            self.gl.end_query(gl::TIME_ELAPSED);
            self.timers.pending = 0;
        }
    }

    fn finish_sampler(&mut self) {
        debug_assert!(self.inside_frame);
        if self.samplers.pending != 0 {
            self.gl.end_query(gl::SAMPLES_PASSED);
            self.samplers.pending = 0;
        }
    }
}

impl<T: NamedTag> GpuFrameProfile<T> {
    fn start_timer(&mut self, tag: T) -> GpuTimeQuery {
        self.finish_timer();

        let marker = GpuMarker::new(&self.gl, tag.get_label());

        if let Some(query) = self.timers.add(GpuTimer { tag, time_ns: 0 }) {
            self.gl.begin_query(gl::TIME_ELAPSED, query);
        }

        GpuTimeQuery(marker)
    }

    fn start_sampler(&mut self, tag: T) -> GpuSampleQuery {
        self.finish_sampler();

        if let Some(query) = self.samplers.add(GpuSampler { tag, count: 0 }) {
            self.gl.begin_query(gl::SAMPLES_PASSED, query);
        }

        GpuSampleQuery
    }

    #[cfg(feature = "debug_renderer")]
    fn build_samples(&mut self) -> (FrameId, Vec<GpuTimer<T>>, Vec<GpuSampler<T>>) {
        debug_assert!(!self.inside_frame);
        let gl = &self.gl;

        (
            self.frame_id,
            self.timers.take(|timer, query| {
                timer.time_ns = gl.get_query_object_ui64v(query, gl::QUERY_RESULT)
            }),
            self.samplers.take(|sampler, query| {
                sampler.count = gl.get_query_object_ui64v(query, gl::QUERY_RESULT)
            }),
        )
    }
}

impl<T> Drop for GpuFrameProfile<T> {
    fn drop(&mut self) {
        self.disable_timers();
        self.disable_samplers();
    }
}

pub struct GpuProfiler<T> {
    gl: Rc<gl::Gl>,
    frames: Vec<GpuFrameProfile<T>>,
    next_frame: usize,
}

impl<T> GpuProfiler<T> {
    pub fn new(gl: Rc<gl::Gl>) -> Self {
        const MAX_PROFILE_FRAMES: usize = 4;
        let frames = (0 .. MAX_PROFILE_FRAMES)
            .map(|_| GpuFrameProfile::new(Rc::clone(&gl)))
            .collect();

        GpuProfiler {
            gl,
            next_frame: 0,
            frames,
        }
    }

    pub fn enable_timers(&mut self) {
        const MAX_TIMERS_PER_FRAME: i32 = 256;

        for frame in &mut self.frames {
            frame.enable_timers(MAX_TIMERS_PER_FRAME);
        }
    }

    pub fn disable_timers(&mut self) {
        for frame in &mut self.frames {
            frame.disable_timers();
        }
    }

    pub fn enable_samplers(&mut self) {
        const MAX_SAMPLERS_PER_FRAME: i32 = 16;
        if cfg!(target_os = "macos") {
            warn!("Expect OSX driver bugs related to sample queries")
        }

        for frame in &mut self.frames {
            frame.enable_samplers(MAX_SAMPLERS_PER_FRAME);
        }
    }

    pub fn disable_samplers(&mut self) {
        for frame in &mut self.frames {
            frame.disable_samplers();
        }
    }
}

impl<T: NamedTag> GpuProfiler<T> {
    #[cfg(feature = "debug_renderer")]
    pub fn build_samples(&mut self) -> (FrameId, Vec<GpuTimer<T>>, Vec<GpuSampler<T>>) {
        self.frames[self.next_frame].build_samples()
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        self.frames[self.next_frame].begin_frame(frame_id);
    }

    pub fn end_frame(&mut self) {
        self.frames[self.next_frame].end_frame();
        self.next_frame = (self.next_frame + 1) % self.frames.len();
    }

    pub fn start_timer(&mut self, tag: T) -> GpuTimeQuery {
        self.frames[self.next_frame].start_timer(tag)
    }

    pub fn start_sampler(&mut self, tag: T) -> GpuSampleQuery {
        self.frames[self.next_frame].start_sampler(tag)
    }

    pub fn finish_sampler(&mut self, _sampler: GpuSampleQuery) {
        self.frames[self.next_frame].finish_sampler()
    }

    pub fn start_marker(&mut self, label: &str) -> GpuMarker {
        GpuMarker::new(&self.gl, label)
    }

    pub fn place_marker(&mut self, label: &str) {
        GpuMarker::fire(&self.gl, label)
    }
}

#[must_use]
pub struct GpuMarker {
    gl: Rc<gl::Gl>,
}

impl GpuMarker {
    fn new(gl: &Rc<gl::Gl>, message: &str) -> Self {
        gl.push_group_marker_ext(message);
        GpuMarker { gl: Rc::clone(gl) }
    }

    fn fire(gl: &Rc<gl::Gl>, message: &str) {
        gl.insert_event_marker_ext(message);
    }
}

impl Drop for GpuMarker {
    fn drop(&mut self) {
        self.gl.pop_group_marker_ext();
    }
}

#[must_use]
pub struct GpuTimeQuery(GpuMarker);
#[must_use]
pub struct GpuSampleQuery;
