/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use bindings::{GeckoProfilerThreadListener, WrCompositor};
use gleam::{gl, gl::Gl, gl::GLenum};
use std::cell::Cell;
use std::collections::hash_map::HashMap;
use std::os::raw::c_void;
use std::ptr;
use std::rc::Rc;
use std::sync::{mpsc, Arc, Condvar, Mutex};
use std::thread;
use webrender::{
    api::units::*, Compositor, CompositorCapabilities, NativeSurfaceId, NativeSurfaceInfo, NativeTileId,
    ThreadListener, CompositorSurfaceTransform, api::ExternalImageId, api::ImageRendering
};

#[no_mangle]
pub extern "C" fn wr_swgl_create_context() -> *mut c_void {
    swgl::Context::create().into()
}

#[no_mangle]
pub extern "C" fn wr_swgl_reference_context(ctx: *mut c_void) {
    swgl::Context::from(ctx).reference();
}

#[no_mangle]
pub extern "C" fn wr_swgl_destroy_context(ctx: *mut c_void) {
    swgl::Context::from(ctx).destroy();
}

#[no_mangle]
pub extern "C" fn wr_swgl_make_current(ctx: *mut c_void) {
    swgl::Context::from(ctx).make_current();
}

#[no_mangle]
pub extern "C" fn wr_swgl_init_default_framebuffer(
    ctx: *mut c_void,
    width: i32,
    height: i32,
    stride: i32,
    buf: *mut c_void,
) {
    swgl::Context::from(ctx).init_default_framebuffer(width, height, stride, buf);
}

#[no_mangle]
pub extern "C" fn wr_swgl_gen_texture(ctx: *mut c_void) -> u32 {
    swgl::Context::from(ctx).gen_textures(1)[0]
}

#[no_mangle]
pub extern "C" fn wr_swgl_delete_texture(ctx: *mut c_void, tex: u32) {
    swgl::Context::from(ctx).delete_textures(&[tex]);
}

#[no_mangle]
pub extern "C" fn wr_swgl_set_texture_parameter(
    ctx: *mut c_void,
    tex: u32,
    pname: u32,
    param: i32,
) {
    swgl::Context::from(ctx).set_texture_parameter(tex, pname, param);
}

#[no_mangle]
pub extern "C" fn wr_swgl_set_texture_buffer(
    ctx: *mut c_void,
    tex: u32,
    internal_format: u32,
    width: i32,
    height: i32,
    stride: i32,
    buf: *mut c_void,
    min_width: i32,
    min_height: i32,
) {
    swgl::Context::from(ctx).set_texture_buffer(
        tex,
        internal_format,
        width,
        height,
        stride,
        buf,
        min_width,
        min_height,
    );
}

pub struct SwTile {
    x: i32,
    y: i32,
    fbo_id: u32,
    color_id: u32,
    tex_id: u32,
    pbo_id: u32,
    dirty_rect: DeviceIntRect,
    valid_rect: DeviceIntRect,
    /// Composition of tiles must be ordered such that any tiles that may overlap
    /// an invalidated tile in an earlier surface only get drawn after that tile
    /// is actually updated. We store a count of the number of overlapping invalid
    /// here, that gets decremented when the invalid tiles are finally updated so
    /// that we know when it is finally safe to draw. Must use a Cell as we might
    /// be analyzing multiple tiles and surfaces
    overlaps: Cell<u32>,
    /// Whether the tile's contents has been invalidated
    invalid: Cell<bool>,
}

impl SwTile {
    fn origin(&self, surface: &SwSurface) -> DeviceIntPoint {
        DeviceIntPoint::new(self.x * surface.tile_size.width, self.y * surface.tile_size.height)
    }

    /// Bounds used for determining overlap dependencies. This may either be the
    /// full tile bounds or the actual valid rect, depending on whether the tile
    /// is invalidated this frame. These bounds are more conservative as such and
    /// may differ from the precise bounds used to actually composite the tile.
    fn overlap_rect(
        &self,
        surface: &SwSurface,
        transform: &CompositorSurfaceTransform,
        clip_rect: &DeviceIntRect,
    ) -> Option<DeviceIntRect> {
        let origin = self.origin(surface);
        // If the tile was invalidated this frame, then we don't have precise
        // bounds. Instead, just use the default surface tile size.
        let bounds = if self.invalid.get() {
            DeviceIntRect::new(origin, surface.tile_size)
        } else {
            self.valid_rect.translate(origin.to_vector())
        };
        let device_rect = transform.transform_rect(&bounds.to_f32().cast_unit()).unwrap().round_out().to_i32();
        device_rect.cast_unit().intersection(clip_rect)
    }

    /// Determine if the tile's bounds may overlap the dependency rect if it were
    /// to be composited at the given position.
    fn may_overlap(
        &self,
        surface: &SwSurface,
        transform: &CompositorSurfaceTransform,
        clip_rect: &DeviceIntRect,
        dep_rect: &DeviceIntRect,
    ) -> bool {
        self.overlap_rect(surface, transform, clip_rect)
            .map_or(false, |r| r.intersects(dep_rect))
    }

    /// Get valid source and destination rectangles for composition of the tile
    /// within a surface, bounded by the clipping rectangle. May return None if
    /// it falls outside of the clip rect.
    fn composite_rects(
        &self,
        surface: &SwSurface,
        transform: &CompositorSurfaceTransform,
        clip_rect: &DeviceIntRect,
    ) -> Option<(DeviceIntRect, DeviceIntRect, bool)> {
        let valid = self.valid_rect.translate(self.origin(surface).to_vector());
        let valid = transform.transform_rect(&valid.to_f32().cast_unit()).unwrap().round_out().to_i32();
        valid.cast_unit().intersection(clip_rect).map(|r| (r.translate(-valid.origin.to_vector().cast_unit()), r, transform.m22 < 0.0))
    }
}

pub struct SwSurface {
    tile_size: DeviceIntSize,
    is_opaque: bool,
    tiles: Vec<SwTile>,
}

struct DrawTileHelper {
    gl: Rc<dyn gl::Gl>,
    prog: u32,
    quad_vbo: u32,
    quad_vao: u32,
    dest_matrix_loc: i32,
    tex_matrix_loc: i32,
}

impl DrawTileHelper {
    fn new(gl: Rc<dyn gl::Gl>) -> Self {
        let quad_vbo = gl.gen_buffers(1)[0];
        gl.bind_buffer(gl::ARRAY_BUFFER, quad_vbo);
        let quad_data: [f32; 8] = [0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0];
        gl::buffer_data(&*gl, gl::ARRAY_BUFFER, &quad_data, gl::STATIC_DRAW);

        let quad_vao = gl.gen_vertex_arrays(1)[0];
        gl.bind_vertex_array(quad_vao);
        gl.enable_vertex_attrib_array(0);
        gl.vertex_attrib_pointer(0, 2, gl::FLOAT, false, 0, 0);
        gl.bind_vertex_array(0);

        let version = match gl.get_type() {
            gl::GlType::Gl => "#version 150",
            gl::GlType::Gles => "#version 300 es",
        };
        let vert_source = "
            in vec2 aVert;
            uniform mat3 uDestMatrix;
            uniform mat3 uTexMatrix;
            out vec2 vTexCoord;
            void main(void) {
                gl_Position = vec4((uDestMatrix * vec3(aVert, 1.0)).xy, 0.0, 1.0);
                vTexCoord = (uTexMatrix * vec3(aVert, 1.0)).xy;
            }
        ";
        let vs = gl.create_shader(gl::VERTEX_SHADER);
        gl.shader_source(vs, &[version.as_bytes(), vert_source.as_bytes()]);
        gl.compile_shader(vs);
        let frag_source = "
            #ifdef GL_ES
                #ifdef GL_FRAGMENT_PRECISION_HIGH
                    precision highp float;
                #else
                    precision mediump float;
                #endif
            #endif
            in vec2 vTexCoord;
            out vec4 oFragColor;
            uniform sampler2D uTex;
            void main(void) {
                oFragColor = texture(uTex, vTexCoord);
            }
        ";
        let fs = gl.create_shader(gl::FRAGMENT_SHADER);
        gl.shader_source(fs, &[version.as_bytes(), frag_source.as_bytes()]);
        gl.compile_shader(fs);

        let prog = gl.create_program();
        gl.attach_shader(prog, vs);
        gl.attach_shader(prog, fs);
        gl.bind_attrib_location(prog, 0, "aVert");
        gl.link_program(prog);

        let mut status = [0];
        unsafe {
            gl.get_program_iv(prog, gl::LINK_STATUS, &mut status);
        }
        assert!(status[0] != 0);

        //println!("vert: {}", gl.get_shader_info_log(vs));
        //println!("frag: {}", gl.get_shader_info_log(fs));
        //println!("status: {}, {}", status[0], gl.get_program_info_log(prog));

        gl.use_program(prog);
        let dest_matrix_loc = gl.get_uniform_location(prog, "uDestMatrix");
        assert!(dest_matrix_loc != -1);
        let tex_matrix_loc = gl.get_uniform_location(prog, "uTexMatrix");
        assert!(tex_matrix_loc != -1);
        let tex_loc = gl.get_uniform_location(prog, "uTex");
        assert!(tex_loc != -1);
        gl.uniform_1i(tex_loc, 0);
        gl.use_program(0);

        gl.delete_shader(vs);
        gl.delete_shader(fs);

        DrawTileHelper {
            gl,
            prog,
            quad_vao,
            quad_vbo,
            dest_matrix_loc,
            tex_matrix_loc,
        }
    }

    fn deinit(&self) {
        self.gl.delete_program(self.prog);
        self.gl.delete_vertex_arrays(&[self.quad_vao]);
        self.gl.delete_buffers(&[self.quad_vbo]);
    }

    fn enable(&self, viewport: &DeviceIntRect) {
        self.gl.viewport(
            viewport.origin.x,
            viewport.origin.y,
            viewport.size.width,
            viewport.size.height,
        );
        self.gl.bind_vertex_array(self.quad_vao);
        self.gl.use_program(self.prog);
        self.gl.active_texture(gl::TEXTURE0);
    }

    fn draw(
        &self,
        viewport: &DeviceIntRect,
        dest: &DeviceIntRect,
        src: &DeviceIntRect,
        surface: &SwSurface,
        tile: &SwTile,
        flip_y: bool,
        filter: GLenum,
    ) {
        let dx = dest.origin.x as f32 / viewport.size.width as f32;
        let dy = dest.origin.y as f32 / viewport.size.height as f32;
        let dw = dest.size.width as f32 / viewport.size.width as f32;
        let dh = dest.size.height as f32 / viewport.size.height as f32;
        self.gl.uniform_matrix_3fv(
            self.dest_matrix_loc,
            false,
            &[
                2.0 * dw,
                0.0,
                0.0,
                0.0,
                if flip_y { 2.0 * dh } else { -2.0 * dh },
                0.0,
                -1.0 + 2.0 * dx,
                if flip_y { -1.0 + 2.0 * dy } else { 1.0 - 2.0 * dy },
                1.0,
             ],
        );
        let sx = src.origin.x as f32 / surface.tile_size.width as f32;
        let sy = src.origin.y as f32 / surface.tile_size.height as f32;
        let sw = src.size.width as f32 / surface.tile_size.width as f32;
        let sh = src.size.height as f32 / surface.tile_size.height as f32;
        self.gl
            .uniform_matrix_3fv(self.tex_matrix_loc, false, &[sw, 0.0, 0.0, 0.0, sh, 0.0, sx, sy, 1.0]);

        self.gl.bind_texture(gl::TEXTURE_2D, tile.tex_id);
        self.gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, filter as gl::GLint);
        self.gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, filter as gl::GLint);
        self.gl.draw_arrays(gl::TRIANGLE_STRIP, 0, 4);
    }

    fn disable(&self) {
        self.gl.use_program(0);
        self.gl.bind_vertex_array(0);
    }
}

/// A tile composition job to be processed by the SwComposite thread.
/// Stores relevant details about the tile and where to composite it.
struct SwCompositeJob {
    /// Locked texture that will be unlocked immediately following the job
    locked_src: swgl::LockedResource,
    /// Locked framebuffer that may be shared among many jobs
    locked_dst: swgl::LockedResource,
    src_rect: DeviceIntRect,
    dst_rect: DeviceIntRect,
    opaque: bool,
    flip_y: bool,
    filter: GLenum,
}

/// The SwComposite thread processes a queue of composite jobs, also signaling
/// via a condition when all available jobs have been processed, as tracked by
/// the job count.
struct SwCompositeThread {
    /// Queue of available composite jobs
    job_queue: mpsc::Sender<SwCompositeJob>,
    /// Count of unprocessed jobs still in the queue
    job_count: Mutex<usize>,
    /// Condition signaled when there are no more jobs left to process
    done_cond: Condvar,
}

/// The SwCompositeThread struct is shared between the SwComposite thread
/// and the rendering thread so that both ends can access the job queue.
unsafe impl Sync for SwCompositeThread {}

impl SwCompositeThread {
    /// Create the SwComposite thread. Requires a SWGL context in which
    /// to do the composition.
    fn new() -> Arc<SwCompositeThread> {
        let (job_queue, job_rx) = mpsc::channel();
        let info = Arc::new(SwCompositeThread {
            job_queue,
            job_count: Mutex::new(0),
            done_cond: Condvar::new(),
        });
        let result = info.clone();
        let thread_name = "SwComposite";
        thread::Builder::new()
            .name(thread_name.into())
            .spawn(move || {
                let thread_listener = GeckoProfilerThreadListener::new();
                thread_listener.thread_started(thread_name);
                // Process any available jobs. This will return a non-Ok
                // result when the job queue is dropped, causing the thread
                // to eventually exit.
                while let Ok(job) = job_rx.recv() {
                    job.locked_dst.composite(
                        &job.locked_src,
                        job.src_rect.origin.x,
                        job.src_rect.origin.y,
                        job.src_rect.size.width,
                        job.src_rect.size.height,
                        job.dst_rect.origin.x,
                        job.dst_rect.origin.y,
                        job.dst_rect.size.width,
                        job.dst_rect.size.height,
                        job.opaque,
                        job.flip_y,
                        job.filter,
                    );
                    // Release locked resources before modifying job count
                    drop(job);
                    // Decrement the job count. If applicable, signal that all jobs
                    // have been completed.
                    let mut count = info.job_count.lock().unwrap();
                    *count -= 1;
                    if *count <= 0 {
                        info.done_cond.notify_all();
                    }
                }
                thread_listener.thread_stopped(thread_name);
            })
            .expect("Failed creating SwComposite thread");
        result
    }

    /// Queue a tile for composition by adding to the queue and increasing the job count.
    fn queue_composite(
        &self,
        locked_src: swgl::LockedResource,
        locked_dst: swgl::LockedResource,
        src_rect: DeviceIntRect,
        dst_rect: DeviceIntRect,
        opaque: bool,
        flip_y: bool,
        filter: GLenum,
    ) {
        // There are still tile updates happening, so send the job to the SwComposite thread.
        *self.job_count.lock().unwrap() += 1;
        self.job_queue
            .send(SwCompositeJob {
                locked_src,
                locked_dst,
                src_rect,
                dst_rect,
                opaque,
                flip_y,
                filter,
            })
            .expect("Failing queuing SwComposite job");
    }

    /// Wait for all queued composition jobs to be processed by checking the done condition.
    fn wait_for_composites(&self) {
        let mut jobs = self.job_count.lock().unwrap();
        while *jobs > 0 {
            jobs = self.done_cond.wait(jobs).unwrap();
        }
    }
}

pub struct SwCompositor {
    gl: swgl::Context,
    native_gl: Option<Rc<dyn gl::Gl>>,
    compositor: Option<WrCompositor>,
    surfaces: HashMap<NativeSurfaceId, SwSurface>,
    frame_surfaces: Vec<(NativeSurfaceId, CompositorSurfaceTransform, DeviceIntRect, GLenum)>,
    cur_tile: NativeTileId,
    draw_tile: Option<DrawTileHelper>,
    /// The maximum tile size required for any of the allocated surfaces.
    max_tile_size: DeviceIntSize,
    /// Reuse the same depth texture amongst all tiles in all surfaces.
    /// This depth texture must be big enough to accommodate the largest used
    /// tile size for any surface. The maximum requested tile size is tracked
    /// to ensure that this depth texture is at least that big.
    depth_id: u32,
    /// Instance of the SwComposite thread, only created if we are not relying
    /// on OpenGL compositing or a native RenderCompositor.
    composite_thread: Option<Arc<SwCompositeThread>>,
    /// SWGL locked resource for sharing framebuffer with SwComposite thread
    locked_framebuffer: Option<swgl::LockedResource>,
}

impl SwCompositor {
    pub fn new(gl: swgl::Context, native_gl: Option<Rc<dyn gl::Gl>>, compositor: Option<WrCompositor>) -> Self {
        let depth_id = gl.gen_textures(1)[0];
        // Only create the SwComposite thread if we're neither using OpenGL composition nor a native
        // render compositor. Thus, we are compositing into the main software framebuffer, which in
        // that case benefits from compositing asynchronously while we are updating tiles.
        let composite_thread = if native_gl.is_none() && compositor.is_none() {
            Some(SwCompositeThread::new())
        } else {
            None
        };
        SwCompositor {
            gl,
            compositor,
            surfaces: HashMap::new(),
            frame_surfaces: Vec::new(),
            cur_tile: NativeTileId {
                surface_id: NativeSurfaceId(0),
                x: 0,
                y: 0,
            },
            draw_tile: native_gl.as_ref().map(|gl| DrawTileHelper::new(gl.clone())),
            native_gl,
            max_tile_size: DeviceIntSize::zero(),
            depth_id,
            composite_thread,
            locked_framebuffer: None,
        }
    }

    fn deinit_shader(&mut self) {
        if let Some(draw_tile) = &self.draw_tile {
            draw_tile.deinit();
        }
        self.draw_tile = None;
    }

    fn deinit_tile(&self, tile: &SwTile) {
        self.gl.delete_framebuffers(&[tile.fbo_id]);
        self.gl.delete_textures(&[tile.color_id]);
        if let Some(native_gl) = &self.native_gl {
            native_gl.delete_textures(&[tile.tex_id]);
            native_gl.delete_buffers(&[tile.pbo_id]);
        }
    }

    fn deinit_surface(&self, surface: &SwSurface) {
        for tile in &surface.tiles {
            self.deinit_tile(tile);
        }
    }

    /// Reset tile dependency state for a new frame.
    fn reset_overlaps(&mut self) {
        for surface in self.surfaces.values_mut() {
            for tile in &mut surface.tiles {
                tile.overlaps.set(0);
                tile.invalid.set(false);
            }
        }
    }

    /// Computes an overlap count for a tile that falls within the given composite
    /// destination rectangle. This requires checking all surfaces currently queued for
    /// composition so far in this frame and seeing if they have any invalidated tiles
    /// whose destination rectangles would also overlap the supplied tile. If so, then the
    /// increment the overlap count to account for all such dependencies on invalid tiles.
    /// Tiles with the same overlap count will still be drawn with a stable ordering in
    /// the order the surfaces were queued, so it is safe to ignore other possible sources
    /// of composition ordering dependencies, as the later queued tile will still be drawn
    /// later than the blocking tiles within that stable order. We assume that the tile's
    /// surface hasn't yet been added to the current frame list of surfaces to composite
    /// so that we only process potential blockers from surfaces that would come earlier
    /// in composition.
    fn get_overlaps(&self, overlap_rect: &DeviceIntRect) -> u32 {
        let mut overlaps = 0;
        for &(ref id, ref transform, ref clip_rect, _) in &self.frame_surfaces {
            // If the surface's clip rect doesn't overlap the tile's rect,
            // then there is no need to check any tiles within the surface.
            if !overlap_rect.intersects(clip_rect) {
                continue;
            }
            if let Some(surface) = self.surfaces.get(id) {
                for tile in &surface.tiles {
                    // If there is a deferred tile that might overlap the destination rectangle,
                    // record the overlap.
                    if tile.overlaps.get() > 0 &&
                       tile.may_overlap(surface, transform, clip_rect, overlap_rect) {
                        overlaps += 1;
                    }
                }
            }
        }
        overlaps
    }

    /// Helper function that queues a composite job to the current locked framebuffer
    fn queue_composite(
        &self,
        surface: &SwSurface,
        transform: &CompositorSurfaceTransform,
        clip_rect: &DeviceIntRect,
        filter: GLenum,
        tile: &SwTile,
    ) {
        if let Some(ref composite_thread) = self.composite_thread {
            if let Some((src_rect, dst_rect, flip_y)) = tile.composite_rects(surface, transform, clip_rect) {
                if let Some(texture) = self.gl.lock_texture(tile.color_id) {
                    let framebuffer = self.locked_framebuffer.clone().unwrap();
                    composite_thread.queue_composite(texture, framebuffer, src_rect, dst_rect, surface.is_opaque, flip_y, filter);
                }
            }
        }
    }

    /// If using the SwComposite thread, we need to compute an overlap count for all tiles
    /// within the surface being queued for composition this frame. If the tile is immediately
    /// ready to composite, then queue that now. Otherwise, set its draw order index for later
    /// composition.
    fn init_composites(
        &mut self,
        id: &NativeSurfaceId,
        transform: &CompositorSurfaceTransform,
        clip_rect: &DeviceIntRect,
        filter: GLenum,
    ) {
        if self.composite_thread.is_none() {
            return;
        }

        if let Some(surface) = self.surfaces.get(&id) {
            for tile in &surface.tiles {
                if let Some(overlap_rect) = tile.overlap_rect(surface, transform, clip_rect) {
                    let mut overlaps = self.get_overlaps(&overlap_rect);
                    // Record an extra overlap for an invalid tile to track the tile's dependency
                    // on its own future update.
                    if tile.invalid.get() {
                        overlaps += 1;
                    }
                    if overlaps == 0 {
                        // Not dependent on any tiles, so go ahead and composite now.
                        self.queue_composite(surface, transform, clip_rect, filter, tile);
                    } else {
                        // Has a dependency on some invalid tiles, so need to defer composition.
                        tile.overlaps.set(overlaps);
                    }
                }
            }
        }
    }

    /// Issue composites for any tiles that are no longer blocked following a tile update.
    /// We process all surfaces and tiles in the order they were queued.
    fn flush_composites(&self, tile_id: &NativeTileId, surface: &SwSurface, tile: &SwTile) {
        if self.composite_thread.is_none() {
            return;
        }

        // Look for the tile in the frame list and composite it if it has no dependencies.
        let mut frame_surfaces = self
            .frame_surfaces
            .iter()
            .skip_while(|&(ref id, _, _, _)| *id != tile_id.surface_id);
        let overlap_rect = match frame_surfaces.next() {
            Some(&(_, ref transform, ref clip_rect, filter)) => {
                // Remove invalid tile's update dependency.
                if tile.invalid.get() {
                    tile.overlaps.set(tile.overlaps.get() - 1);
                }
                // If the tile still has overlaps, keep deferring it till later.
                if tile.overlaps.get() > 0 {
                    return;
                }
                // Otherwise, the tile's dependencies are all resolved, so composite it.
                self.queue_composite(surface, transform, clip_rect, filter, tile);
                // Finally, get the tile's overlap rect used for tracking dependencies
                match tile.overlap_rect(surface, transform, clip_rect) {
                    Some(overlap_rect) => overlap_rect,
                    None => return,
                }
            }
            None => return,
        };

        // Accumulate rects whose dependencies have been satisfied from this update.
        // Store the union of all these bounds to quickly reject unaffected tiles.
        let mut flushed_bounds = overlap_rect;
        let mut flushed_rects = vec![overlap_rect];

        // Check surfaces following the update in the frame list and see if they would overlap it.
        for &(ref id, ref transform, ref clip_rect, filter) in frame_surfaces {
            // If the clip rect doesn't overlap the conservative bounds, we can skip the whole surface.
            if !flushed_bounds.intersects(clip_rect) {
                continue;
            }
            if let Some(surface) = self.surfaces.get(&id) {
                // Search through the surface's tiles for any blocked on this update and queue jobs for them.
                for tile in &surface.tiles {
                    let mut overlaps = tile.overlaps.get();
                    // Only check tiles that have existing unresolved dependencies
                    if overlaps == 0 {
                        continue;
                    }
                    // Get this tile's overlap rect for tracking dependencies
                    let overlap_rect = match tile.overlap_rect(surface, transform, clip_rect) {
                        Some(overlap_rect) => overlap_rect,
                        None => continue,
                    };
                    // Do a quick check to see if the tile overlaps the conservative bounds.
                    if !overlap_rect.intersects(&flushed_bounds) {
                        continue;
                    }
                    // Decrement the overlap count if this tile is dependent on any flushed rects.
                    for flushed_rect in &flushed_rects {
                        if overlap_rect.intersects(flushed_rect) {
                            overlaps -= 1;
                        }
                    }
                    if overlaps != tile.overlaps.get() {
                        // If the overlap count changed, this tile had a dependency on some flush rects.
                        // If the count hit zero, it is ready to composite.
                        tile.overlaps.set(overlaps);
                        if overlaps == 0 {
                            self.queue_composite(surface, transform, clip_rect, filter, tile);
                            // Record that the tile got flushed to update any downwind dependencies.
                            flushed_bounds = flushed_bounds.union(&overlap_rect);
                            flushed_rects.push(overlap_rect);
                        }
                    }
                }
            }
        }
    }
}

impl Compositor for SwCompositor {
    fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        virtual_offset: DeviceIntPoint,
        tile_size: DeviceIntSize,
        is_opaque: bool,
    ) {
        if let Some(compositor) = &mut self.compositor {
            compositor.create_surface(id, virtual_offset, tile_size, is_opaque);
        }
        self.max_tile_size = DeviceIntSize::new(
            self.max_tile_size.width.max(tile_size.width),
            self.max_tile_size.height.max(tile_size.height),
        );
        self.surfaces.insert(
            id,
            SwSurface {
                tile_size,
                is_opaque,
                tiles: Vec::new(),
            },
        );
    }

    fn create_external_surface(
        &mut self,
        _id: NativeSurfaceId,
        _is_opaque: bool,
    ) {
    }

    fn destroy_surface(&mut self, id: NativeSurfaceId) {
        if let Some(surface) = self.surfaces.remove(&id) {
            self.deinit_surface(&surface);
        }
        if let Some(compositor) = &mut self.compositor {
            compositor.destroy_surface(id);
        }
    }

    fn deinit(&mut self) {
        for surface in self.surfaces.values() {
            self.deinit_surface(surface);
        }

        self.gl.delete_textures(&[self.depth_id]);

        self.deinit_shader();

        if let Some(compositor) = &mut self.compositor {
            compositor.deinit();
        }
    }

    fn create_tile(&mut self, id: NativeTileId) {
        if let Some(compositor) = &mut self.compositor {
            compositor.create_tile(id);
        }
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            let color_id = self.gl.gen_textures(1)[0];
            let fbo_id = self.gl.gen_framebuffers(1)[0];
            self.gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, fbo_id);
            self.gl
                .framebuffer_texture_2d(gl::DRAW_FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, color_id, 0);
            self.gl.framebuffer_texture_2d(
                gl::DRAW_FRAMEBUFFER,
                gl::DEPTH_ATTACHMENT,
                gl::TEXTURE_2D,
                self.depth_id,
                0,
            );
            self.gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, 0);

            let mut tex_id = 0;
            let mut pbo_id = 0;
            if let Some(native_gl) = &self.native_gl {
                tex_id = native_gl.gen_textures(1)[0];
                native_gl.bind_texture(gl::TEXTURE_2D, tex_id);
                native_gl.tex_image_2d(
                    gl::TEXTURE_2D,
                    0,
                    gl::RGBA8 as gl::GLint,
                    surface.tile_size.width,
                    surface.tile_size.height,
                    0,
                    gl::RGBA,
                    gl::UNSIGNED_BYTE,
                    None,
                );
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
                native_gl.tex_parameter_i(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
                native_gl.bind_texture(gl::TEXTURE_2D, 0);

                pbo_id = native_gl.gen_buffers(1)[0];
                native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, pbo_id);
                native_gl.buffer_data_untyped(
                    gl::PIXEL_UNPACK_BUFFER,
                    surface.tile_size.area() as isize * 4 + 16,
                    ptr::null(),
                    gl::DYNAMIC_DRAW,
                );
                native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
            }

            surface.tiles.push(SwTile {
                x: id.x,
                y: id.y,
                fbo_id,
                color_id,
                tex_id,
                pbo_id,
                dirty_rect: DeviceIntRect::zero(),
                valid_rect: DeviceIntRect::zero(),
                overlaps: Cell::new(0),
                invalid: Cell::new(false),
            });
        }
    }

    fn destroy_tile(&mut self, id: NativeTileId) {
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(idx) = surface.tiles.iter().position(|t| t.x == id.x && t.y == id.y) {
                let tile = surface.tiles.remove(idx);
                self.deinit_tile(&tile);
            }
        }
        if let Some(compositor) = &mut self.compositor {
            compositor.destroy_tile(id);
        }
    }

    fn attach_external_image(
        &mut self,
        _id: NativeSurfaceId,
        _external_image: ExternalImageId,
    ) {
    }

    fn invalidate_tile(&mut self, id: NativeTileId) {
        if let Some(compositor) = &mut self.compositor {
            compositor.invalidate_tile(id);
        }
        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(tile) = surface.tiles.iter_mut().find(|t| t.x == id.x && t.y == id.y) {
                tile.invalid.set(true);
            }
        }
    }

    fn bind(&mut self, id: NativeTileId, dirty_rect: DeviceIntRect, valid_rect: DeviceIntRect) -> NativeSurfaceInfo {
        let mut surface_info = NativeSurfaceInfo {
            origin: DeviceIntPoint::zero(),
            fbo_id: 0,
        };

        self.cur_tile = id;

        if let Some(surface) = self.surfaces.get_mut(&id.surface_id) {
            if let Some(tile) = surface.tiles.iter_mut().find(|t| t.x == id.x && t.y == id.y) {
                tile.dirty_rect = dirty_rect;
                tile.valid_rect = valid_rect;
                if valid_rect.is_empty() {
                    return surface_info;
                }

                let mut stride = 0;
                let mut buf = ptr::null_mut();
                if let Some(compositor) = &mut self.compositor {
                    if let Some(tile_info) = compositor.map_tile(id, dirty_rect, valid_rect) {
                        stride = tile_info.stride;
                        buf = tile_info.data;
                    }
                } else if let Some(native_gl) = &self.native_gl {
                    if tile.pbo_id != 0 {
                        native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, tile.pbo_id);
                        buf = native_gl.map_buffer_range(
                            gl::PIXEL_UNPACK_BUFFER,
                            0,
                            valid_rect.size.area() as isize * 4 + 16,
                            gl::MAP_WRITE_BIT | gl::MAP_INVALIDATE_BUFFER_BIT,
                        ); // | gl::MAP_UNSYNCHRONIZED_BIT);
                        if buf != ptr::null_mut() {
                            stride = valid_rect.size.width * 4;
                        } else {
                            native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
                            native_gl.delete_buffers(&[tile.pbo_id]);
                            tile.pbo_id = 0;
                        }
                    }
                }
                self.gl.set_texture_buffer(
                    tile.color_id,
                    gl::RGBA8,
                    valid_rect.size.width,
                    valid_rect.size.height,
                    stride,
                    buf,
                    surface.tile_size.width,
                    surface.tile_size.height,
                );
                // Reallocate the shared depth buffer to fit the valid rect, but within
                // a buffer sized to actually fit at least the maximum possible tile size.
                // The maximum tile size is supplied to avoid reallocation by ensuring the
                // allocated buffer is actually big enough to accommodate the largest tile
                // size requested by any used surface, even though supplied valid rect may
                // actually be much smaller than this. This will only force a texture
                // reallocation inside SWGL if the maximum tile size has grown since the
                // last time it was supplied, instead simply reusing the buffer if the max
                // tile size is not bigger than what was previously allocated.
                self.gl.set_texture_buffer(
                    self.depth_id,
                    gl::DEPTH_COMPONENT16,
                    valid_rect.size.width,
                    valid_rect.size.height,
                    0,
                    ptr::null_mut(),
                    self.max_tile_size.width,
                    self.max_tile_size.height,
                );
                surface_info.fbo_id = tile.fbo_id;
                surface_info.origin -= valid_rect.origin.to_vector();
            }
        }

        surface_info
    }

    fn unbind(&mut self) {
        let id = self.cur_tile;
        if let Some(surface) = self.surfaces.get(&id.surface_id) {
            if let Some(tile) = surface.tiles.iter().find(|t| t.x == id.x && t.y == id.y) {
                if tile.valid_rect.is_empty() {
                    self.flush_composites(&id, surface, tile);
                    return;
                }

                // get the color buffer even if we have a self.compositor, to make
                // sure that any delayed clears are resolved
                let (swbuf, _, _, stride) = self.gl.get_color_buffer(tile.fbo_id, true);

                if let Some(compositor) = &mut self.compositor {
                    compositor.unmap_tile();
                    return;
                }

                let native_gl = match &self.native_gl {
                    Some(native_gl) => native_gl,
                    None => {
                        // If we're not relying on a native compositor or OpenGL compositing,
                        // then composite any tiles that are dependent on this tile being
                        // updated but are otherwise ready to composite.
                        self.flush_composites(&id, surface, tile);
                        return;
                    }
                };

                assert!(stride % 4 == 0);
                let buf = if tile.pbo_id != 0 {
                    native_gl.unmap_buffer(gl::PIXEL_UNPACK_BUFFER);
                    0 as *mut c_void
                } else {
                    swbuf
                };
                let dirty = tile.dirty_rect;
                let src = unsafe {
                    (buf as *mut u32).offset(
                        (dirty.origin.y - tile.valid_rect.origin.y) as isize * (stride / 4) as isize
                            + (dirty.origin.x - tile.valid_rect.origin.x) as isize,
                    )
                };
                native_gl.active_texture(gl::TEXTURE0);
                native_gl.bind_texture(gl::TEXTURE_2D, tile.tex_id);
                native_gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, stride / 4);
                native_gl.tex_sub_image_2d_pbo(
                    gl::TEXTURE_2D,
                    0,
                    dirty.origin.x,
                    dirty.origin.y,
                    dirty.size.width,
                    dirty.size.height,
                    gl::BGRA,
                    gl::UNSIGNED_BYTE,
                    src as _,
                );
                native_gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0);
                if tile.pbo_id != 0 {
                    native_gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
                }

                if let Some(compositor) = &mut self.compositor {
                    let info = compositor.bind(id, tile.dirty_rect, tile.valid_rect);
                    native_gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, info.fbo_id);

                    let viewport = dirty.translate(info.origin.to_vector());
                    let draw_tile = self.draw_tile.as_ref().unwrap();
                    draw_tile.enable(&viewport);
                    draw_tile.draw(&viewport, &viewport, &dirty, &surface, &tile, false, gl::LINEAR);
                    draw_tile.disable();

                    native_gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, 0);

                    compositor.unbind();
                }

                native_gl.bind_texture(gl::TEXTURE_2D, 0);
            }
        }
    }

    fn begin_frame(&mut self) {
        if let Some(compositor) = &mut self.compositor {
            compositor.begin_frame();
        }
        self.frame_surfaces.clear();

        self.reset_overlaps();
        if self.composite_thread.is_some() {
            self.locked_framebuffer = self.gl.lock_framebuffer(0);
        }
    }

    fn add_surface(
        &mut self,
        id: NativeSurfaceId,
        transform: CompositorSurfaceTransform,
        clip_rect: DeviceIntRect,
        image_rendering: ImageRendering
    ) {
        if let Some(compositor) = &mut self.compositor {
            compositor.add_surface(id, transform, clip_rect, image_rendering);
        }

        let filter = match image_rendering {
            ImageRendering::Pixelated => {
                gl::NEAREST
            }
            ImageRendering::Auto | ImageRendering::CrispEdges => {
                gl::LINEAR
            }
        };

        // Compute overlap dependencies and issue any initial composite jobs for the SwComposite thread.
        self.init_composites(&id, &transform, &clip_rect, filter);

        self.frame_surfaces.push((id, transform, clip_rect, filter));
    }

    fn end_frame(&mut self) {
        if let Some(compositor) = &mut self.compositor {
            compositor.end_frame();
        } else if let Some(native_gl) = &self.native_gl {
            let (_, fw, fh, _) = self.gl.get_color_buffer(0, false);
            let viewport = DeviceIntRect::from_size(DeviceIntSize::new(fw, fh));
            let draw_tile = self.draw_tile.as_ref().unwrap();
            draw_tile.enable(&viewport);
            let mut blend = false;
            native_gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
            for &(ref id, ref transform, ref clip_rect, filter) in &self.frame_surfaces {
                if let Some(surface) = self.surfaces.get(id) {
                    if surface.is_opaque {
                        if blend {
                            native_gl.disable(gl::BLEND);
                            blend = false;
                        }
                    } else if !blend {
                        native_gl.enable(gl::BLEND);
                        blend = true;
                    }
                    for tile in &surface.tiles {
                        if let Some((src_rect, dst_rect, flip_y)) = tile.composite_rects(surface, transform, clip_rect) {
                            draw_tile.draw(&viewport, &dst_rect, &src_rect, surface, tile, flip_y, filter);
                        }
                    }
                }
            }
            if blend {
                native_gl.disable(gl::BLEND);
            }
            draw_tile.disable();
        } else if let Some(ref composite_thread) = self.composite_thread {
            // Need to wait for the SwComposite thread to finish any queued jobs.
            composite_thread.wait_for_composites();
            self.locked_framebuffer = None;
        }
    }

    fn enable_native_compositor(&mut self, enable: bool) {
        if let Some(compositor) = &mut self.compositor {
            compositor.enable_native_compositor(enable);
        }
    }

    fn get_capabilities(&self) -> CompositorCapabilities {
        if let Some(compositor) = &self.compositor {
            compositor.get_capabilities()
        } else {
            CompositorCapabilities {
                virtual_surface_size: 0,
            }
        }
    }
}
