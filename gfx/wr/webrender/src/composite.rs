/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::ColorF;
use api::units::{DeviceRect, DeviceIntSize, DeviceIntRect, DeviceIntPoint, WorldRect, DevicePixelScale, DevicePoint};
use crate::gpu_types::{ZBufferId, ZBufferIdGenerator};
use crate::picture::{ResolvedSurfaceTexture, TileId, TileCacheInstance, TileSurface};
use crate::resource_cache::ResourceCache;
use std::{ops, u64};

/*
 Types and definitions related to compositing picture cache tiles
 and/or OS compositor integration.
 */

/// Describes details of an operation to apply to a native surface
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum NativeSurfaceOperationDetails {
    CreateSurface {
        id: NativeSurfaceId,
        tile_size: DeviceIntSize,
        is_opaque: bool,
    },
    DestroySurface {
        id: NativeSurfaceId,
    },
    CreateTile {
        id: NativeTileId,
    },
    DestroyTile {
        id: NativeTileId,
    }
}

/// Describes an operation to apply to a native surface
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeSurfaceOperation {
    pub details: NativeSurfaceOperationDetails,
}

/// Describes the source surface information for a tile to be composited. This
/// is the analog of the TileSurface type, with target surface information
/// resolved such that it can be used by the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum CompositeTileSurface {
    Texture {
        surface: ResolvedSurfaceTexture,
    },
    Color {
        color: ColorF,
    },
    Clear,
}

/// Describes the geometry and surface of a tile to be composited
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CompositeTile {
    pub surface: CompositeTileSurface,
    pub rect: DeviceRect,
    pub clip_rect: DeviceRect,
    pub dirty_rect: DeviceRect,
    pub z_id: ZBufferId,
    pub tile_id: TileId,
}

/// Public interface specified in `RendererOptions` that configures
/// how WR compositing will operate.
pub enum CompositorConfig {
    /// Let WR draw tiles via normal batching. This requires no special OS support.
    Draw {
        /// If this is zero, a full screen present occurs at the end of the
        /// frame. This is the simplest and default mode. If this is non-zero,
        /// then the operating system supports a form of 'partial present' where
        /// only dirty regions of the framebuffer need to be updated.
        max_partial_present_rects: usize,
    },
    /// Use a native OS compositor to draw tiles. This requires clients to implement
    /// the Compositor trait, but can be significantly more power efficient on operating
    /// systems that support it.
    Native {
        /// The maximum number of dirty rects that can be provided per compositor
        /// surface update. If this is zero, the entire compositor surface for
        /// a given tile will be drawn if it's dirty.
        max_update_rects: usize,
        /// A client provided interface to a native / OS compositor.
        compositor: Box<dyn Compositor>,
    }
}

impl Default for CompositorConfig {
    /// Default compositor config is full present without partial present.
    fn default() -> Self {
        CompositorConfig::Draw {
            max_partial_present_rects: 0,
        }
    }
}

/// This is a representation of `CompositorConfig` without the `Compositor` trait
/// present. This allows it to be freely copied to other threads, such as the render
/// backend where the frame builder can access it.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CompositorKind {
    /// WR handles compositing via drawing.
    Draw {
        /// Partial present support.
        max_partial_present_rects: usize,
    },
    /// Native OS compositor.
    Native {
        /// Maximum dirty rects per compositor surface.
        max_update_rects: usize,
    },
}

impl CompositorKind {
    /// Returns true if this compositor is native (uses OS compositor)
    pub fn is_native(&self) -> bool {
        match self {
            CompositorKind::Draw { .. } => false,
            CompositorKind::Native { .. } => true,
        }
    }
}

impl Default for CompositorKind {
    /// Default compositor config is full present without partial present.
    fn default() -> Self {
        CompositorKind::Draw {
            max_partial_present_rects: 0,
        }
    }
}

/// Information about an opaque surface used to occlude tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct Occluder {
    slice: usize,
    device_rect: DeviceIntRect,
}

/// Describes the properties that identify a tile composition uniquely.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(PartialEq, Clone)]
pub struct CompositeSurfaceDescriptor {
    pub slice: usize,
    pub surface_id: Option<NativeSurfaceId>,
    pub offset: DevicePoint,
    pub clip_rect: DeviceRect,
}

/// Describes surface properties used to composite a frame. This
/// is used to compare compositions between frames.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(PartialEq, Clone)]
pub struct CompositeDescriptor {
    pub surfaces: Vec<CompositeSurfaceDescriptor>,
}

impl CompositeDescriptor {
    /// Construct an empty descriptor.
    pub fn empty() -> Self {
        CompositeDescriptor {
            surfaces: Vec::new(),
        }
    }
}

/// The list of tiles to be drawn this frame
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CompositeState {
    // TODO(gw): Consider splitting up CompositeState into separate struct types depending
    //           on the selected compositing mode. Many of the fields in this state struct
    //           are only applicable to either Native or Draw compositing mode.
    /// List of opaque tiles to be drawn by the Draw compositor.
    pub opaque_tiles: Vec<CompositeTile>,
    /// List of alpha tiles to be drawn by the Draw compositor.
    pub alpha_tiles: Vec<CompositeTile>,
    /// List of clear tiles to be drawn by the Draw compositor.
    pub clear_tiles: Vec<CompositeTile>,
    /// Used to generate z-id values for tiles in the Draw compositor mode.
    pub z_generator: ZBufferIdGenerator,
    // If false, we can't rely on the dirty rects in the CompositeTile
    // instances. This currently occurs during a scroll event, as a
    // signal to refresh the whole screen. This is only a temporary
    // measure until we integrate with OS compositors. In the meantime
    // it gives us the ability to partial present for any non-scroll
    // case as a simple win (e.g. video, animation etc).
    pub dirty_rects_are_valid: bool,
    /// The kind of compositor for picture cache tiles (e.g. drawn by WR, or OS compositor)
    pub compositor_kind: CompositorKind,
    /// Picture caching may be disabled dynamically, based on debug flags, pinch zoom etc.
    pub picture_caching_is_enabled: bool,
    /// The overall device pixel scale, used for tile occlusion conversions.
    global_device_pixel_scale: DevicePixelScale,
    /// List of registered occluders
    occluders: Vec<Occluder>,
    /// Description of the surfaces and properties that are being composited.
    pub descriptor: CompositeDescriptor,
}

impl CompositeState {
    /// Construct a new state for compositing picture tiles. This is created
    /// during each frame construction and passed to the renderer.
    pub fn new(
        compositor_kind: CompositorKind,
        mut picture_caching_is_enabled: bool,
        global_device_pixel_scale: DevicePixelScale,
    ) -> Self {
        // The native compositor interface requires picture caching to work, so
        // force it here and warn if it was disabled.
        if let CompositorKind::Native { .. } = compositor_kind {
            if !picture_caching_is_enabled {
                warn!("Picture caching cannot be disabled in native compositor config");
            }
            picture_caching_is_enabled = true;
        }

        CompositeState {
            opaque_tiles: Vec::new(),
            alpha_tiles: Vec::new(),
            clear_tiles: Vec::new(),
            z_generator: ZBufferIdGenerator::new(0),
            dirty_rects_are_valid: true,
            compositor_kind,
            picture_caching_is_enabled,
            global_device_pixel_scale,
            occluders: Vec::new(),
            descriptor: CompositeDescriptor::empty(),
        }
    }

    /// Register an occluder during picture cache updates that can be
    /// used during frame building to occlude tiles.
    pub fn register_occluder(
        &mut self,
        slice: usize,
        rect: WorldRect,
    ) {
        let device_rect = (rect * self.global_device_pixel_scale).round().to_i32();

        self.occluders.push(Occluder {
            device_rect,
            slice,
        });
    }

    /// Returns true if a tile with the specified rectangle and slice
    /// is occluded by an opaque surface in front of it.
    pub fn is_tile_occluded(
        &self,
        slice: usize,
        rect: WorldRect,
    ) -> bool {
        // It's often the case that a tile is only occluded by considering multiple
        // picture caches in front of it (for example, the background tiles are
        // often occluded by a combination of the content slice + the scrollbar slices).

        // The basic algorithm is:
        //    For every occluder:
        //      If this occluder is in front of the tile we are querying:
        //         Clip the occluder rectangle to the query rectangle.
        //    Calculate the total non-overlapping area of those clipped occluders.
        //    If the cumulative area of those occluders is the same as the area of the query tile,
        //       Then the entire tile must be occluded and can be skipped during rasterization and compositing.

        // Get the reference area we will compare against.
        let device_rect = (rect * self.global_device_pixel_scale).round().to_i32();
        let ref_area = device_rect.size.width * device_rect.size.height;

        // Calculate the non-overlapping area of the valid occluders.
        let cover_area = area_of_occluders(&self.occluders, slice, &device_rect);
        debug_assert!(cover_area <= ref_area);

        // Check if the tile area is completely covered
        ref_area == cover_area
    }

    /// Add a picture cache to be composited
    pub fn push_surface(
        &mut self,
        tile_cache: &TileCacheInstance,
        device_clip_rect: DeviceRect,
        z_id: ZBufferId,
        global_device_pixel_scale: DevicePixelScale,
        resource_cache: &ResourceCache,
    ) {
        let mut visible_tile_count = 0;

        for key in &tile_cache.tiles_to_draw {
            let tile = &tile_cache.tiles[key];
            if !tile.is_visible {
                // This can occur when a tile is found to be occluded during frame building.
                continue;
            }

            visible_tile_count += 1;

            let device_rect = (tile.world_rect * global_device_pixel_scale).round();
            let dirty_rect = (tile.world_dirty_rect * global_device_pixel_scale).round();
            let valid_rect = (tile.world_valid_rect * global_device_pixel_scale).round_out();
            let surface = tile.surface.as_ref().expect("no tile surface set!");

            // When compositing in simple (draw) mode, each tile only needs to write pixels
            // where (a) the valid region of the tile is and (b) the overall clip rect of
            // the picture cache surface.
            let clip_rect = match valid_rect.intersection(&device_clip_rect) {
                Some(clip_rect) => clip_rect,
                None => continue,
            };

            let (surface, is_opaque) = match surface {
                TileSurface::Color { color } => {
                    (CompositeTileSurface::Color { color: *color }, true)
                }
                TileSurface::Clear => {
                    (CompositeTileSurface::Clear, false)
                }
                TileSurface::Texture { descriptor, .. } => {
                    let surface = descriptor.resolve(resource_cache, tile_cache.current_tile_size);
                    (
                        CompositeTileSurface::Texture { surface },
                        tile.is_opaque || tile_cache.is_opaque(),
                    )
                }
            };

            let tile = CompositeTile {
                surface,
                rect: device_rect,
                dirty_rect,
                clip_rect,
                z_id,
                tile_id: tile.id,
            };

            self.push_tile(tile, is_opaque);
        }

        if visible_tile_count > 0 {
            self.descriptor.surfaces.push(
                CompositeSurfaceDescriptor {
                    slice: tile_cache.slice,
                    surface_id: tile_cache.native_surface_id,
                    offset: tile_cache.device_position,
                    clip_rect: device_clip_rect,
                }
            );
        }
    }

    /// Add a tile to the appropriate array, depending on tile properties and compositor mode.
    fn push_tile(
        &mut self,
        tile: CompositeTile,
        is_opaque: bool,
    ) {
        match tile.surface {
            CompositeTileSurface::Color { .. } => {
                // Color tiles are, by definition, opaque. We might support non-opaque color
                // tiles if we ever find pages that have a lot of these.
                self.opaque_tiles.push(tile);
            }
            CompositeTileSurface::Clear => {
                // Clear tiles have a special bucket
                self.clear_tiles.push(tile);
            }
            CompositeTileSurface::Texture { .. } => {
                // Texture surfaces get bucketed by opaque/alpha, for z-rejection
                // on the Draw compositor mode.
                if is_opaque {
                    self.opaque_tiles.push(tile);
                } else {
                    self.alpha_tiles.push(tile);
                }
            }
        }
    }
}

/// An arbitrary identifier for a native (OS compositor) surface
#[repr(C)]
#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeSurfaceId(pub u64);

impl NativeSurfaceId {
    /// A special id for the native surface that is used for debug / profiler overlays.
    pub const DEBUG_OVERLAY: NativeSurfaceId = NativeSurfaceId(u64::MAX);
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeTileId {
    pub surface_id: NativeSurfaceId,
    pub x: i32,
    pub y: i32,
}

impl NativeTileId {
    /// A special id for the native surface that is used for debug / profiler overlays.
    pub const DEBUG_OVERLAY: NativeTileId = NativeTileId {
        surface_id: NativeSurfaceId::DEBUG_OVERLAY,
        x: 0,
        y: 0,
    };
}

/// Information about a bound surface that the native compositor
/// returns to WR.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct NativeSurfaceInfo {
    /// An offset into the surface that WR should draw. Some compositing
    /// implementations (notably, DirectComposition) use texture atlases
    /// when the surface sizes are small. In this case, an offset can
    /// be returned into the larger texture where WR should draw. This
    /// can be (0, 0) if texture atlases are not used.
    pub origin: DeviceIntPoint,
    /// The ID of the FBO that WR should bind to, in order to draw to
    /// the bound surface. On Windows (ANGLE) this will always be 0,
    /// since creating a p-buffer sets the default framebuffer to
    /// be the DirectComposition surface. On Mac, this will be non-zero,
    /// since it identifies the IOSurface that has been bound to draw to.
    // TODO(gw): This may need to be a larger / different type for WR
    //           backends that are not GL.
    pub fbo_id: u32,
}

/// Defines an interface to a native (OS level) compositor. If supplied
/// by the client application, then picture cache slices will be
/// composited by the OS compositor, rather than drawn via WR batches.
pub trait Compositor {
    /// Create a new OS compositor surface with the given properties.
    fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        tile_size: DeviceIntSize,
        is_opaque: bool,
    );

    /// Destroy the surface with the specified id. WR may call this
    /// at any time the surface is no longer required (including during
    /// renderer deinit). It's the responsibility of the embedder
    /// to ensure that the surface is only freed once the GPU is
    /// no longer using the surface (if this isn't already handled
    /// by the operating system).
    fn destroy_surface(
        &mut self,
        id: NativeSurfaceId,
    );

    /// Create a new OS compositor tile with the given properties.
    fn create_tile(
        &mut self,
        id: NativeTileId,
    );

    /// Destroy an existing compositor tile.
    fn destroy_tile(
        &mut self,
        id: NativeTileId,
    );

    /// Bind this surface such that WR can issue OpenGL commands
    /// that will target the surface. Returns an (x, y) offset
    /// where WR should draw into the surface. This can be set
    /// to (0, 0) if the OS doesn't use texture atlases. The dirty
    /// rect is a local surface rect that specifies which part
    /// of the surface needs to be updated. If max_update_rects
    /// in CompositeConfig is 0, this will always be the size
    /// of the entire surface. The returned offset is only
    /// relevant to compositors that store surfaces in a texture
    /// atlas (that is, WR expects that the dirty rect doesn't
    /// affect the coordinates of the returned origin).
    fn bind(
        &mut self,
        id: NativeTileId,
        dirty_rect: DeviceIntRect,
    ) -> NativeSurfaceInfo;

    /// Unbind the surface. This is called by WR when it has
    /// finished issuing OpenGL commands on the current surface.
    fn unbind(
        &mut self,
    );

    /// Begin the frame
    fn begin_frame(&mut self);

    /// Add a surface to the visual tree to be composited. Visuals must
    /// be added every frame, between the begin/end transaction call. The
    /// z-order of the surfaces is determined by the order they are added
    /// to the visual tree.
    // TODO(gw): Adding visuals every frame makes the interface simple,
    //           but may have performance implications on some compositors?
    //           We might need to change the interface to maintain a visual
    //           tree that can be mutated?
    // TODO(gw): We might need to add a concept of a hierachy in future.
    // TODO(gw): In future, expand to support a more complete transform matrix.
    fn add_surface(
        &mut self,
        id: NativeSurfaceId,
        position: DeviceIntPoint,
        clip_rect: DeviceIntRect,
    );

    /// Commit any changes in the compositor tree for this frame. WR calls
    /// this once when all surface and visual updates are complete, to signal
    /// that the OS composite transaction should be applied.
    fn end_frame(&mut self);
}

/// Return the total area covered by a set of occluders, accounting for
/// overlapping areas between those rectangles.
fn area_of_occluders(
    occluders: &[Occluder],
    slice: usize,
    clip_rect: &DeviceIntRect,
) -> i32 {
    // This implementation is based on the article https://leetcode.com/articles/rectangle-area-ii/.
    // This is not a particularly efficient implementation (it skips building segment trees), however
    // we typically use this where the length of the rectangles array is < 10, so simplicity is more important.

    let mut area = 0;

    // Whether this event is the start or end of a rectangle
    #[derive(Debug)]
    enum EventKind {
        Begin,
        End,
    }

    // A list of events on the y-axis, with the rectangle range that it affects on the x-axis
    #[derive(Debug)]
    struct Event {
        y: i32,
        x_range: ops::Range<i32>,
        kind: EventKind,
    }

    impl Event {
        fn new(y: i32, kind: EventKind, x0: i32, x1: i32) -> Self {
            Event {
                y,
                x_range: ops::Range {
                    start: x0,
                    end: x1,
                },
                kind,
            }
        }
    }

    // Step through each rectangle and build the y-axis event list
    let mut events = Vec::with_capacity(occluders.len() * 2);
    for occluder in occluders {
        // Only consider occluders in front of this rect
        if occluder.slice > slice {
            // Clip the source rect to the rectangle we care about, since we only
            // want to record area for the tile we are comparing to.
            if let Some(rect) = occluder.device_rect.intersection(clip_rect) {
                let x0 = rect.origin.x;
                let x1 = x0 + rect.size.width;
                events.push(Event::new(rect.origin.y, EventKind::Begin, x0, x1));
                events.push(Event::new(rect.origin.y + rect.size.height, EventKind::End, x0, x1));
            }
        }
    }

    // If we didn't end up with any valid events, the area must be 0
    if events.is_empty() {
        return 0;
    }

    // Sort the events by y-value
    events.sort_by_key(|e| e.y);
    let mut active: Vec<ops::Range<i32>> = Vec::new();
    let mut cur_y = events[0].y;

    // Step through each y interval
    for event in &events {
        // This is the dimension of the y-axis we are accumulating areas for
        let dy = event.y - cur_y;

        // If we have active events covering x-ranges in this y-interval, process them
        if dy != 0 && !active.is_empty() {
            assert!(dy > 0);

            // Step through the x-ranges, ordered by x0 of each event
            active.sort_by_key(|i| i.start);
            let mut query = 0;
            let mut cur = active[0].start;

            // Accumulate the non-overlapping x-interval that contributes to area for this y-interval.
            for interval in &active {
                cur = interval.start.max(cur);
                query += (interval.end - cur).max(0);
                cur = cur.max(interval.end);
            }

            // Accumulate total area for this y-interval
            area += query * dy;
        }

        // Update the active events list
        match event.kind {
            EventKind::Begin => {
                active.push(event.x_range.clone());
            }
            EventKind::End => {
                let index = active.iter().position(|i| *i == event.x_range).unwrap();
                active.remove(index);
            }
        }

        cur_y = event.y;
    }

    area
}
