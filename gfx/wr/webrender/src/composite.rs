/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::ColorF;
use api::units::{DeviceRect, DeviceIntSize, DeviceIntRect, DeviceIntPoint};
use crate::gpu_types::{ZBufferId, ZBufferIdGenerator};
use crate::picture::{ResolvedSurfaceTexture, SurfaceTextureDescriptor};

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
        size: DeviceIntSize,
        is_opaque: bool,
    },
    DestroySurface,
}

/// Describes an operation to apply to a native surface
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeSurfaceOperation {
    pub id: NativeSurfaceId,
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

impl Default for CompositorKind {
    /// Default compositor config is full present without partial present.
    fn default() -> Self {
        CompositorKind::Draw {
            max_partial_present_rects: 0,
        }
    }
}

/// The list of tiles to be drawn this frame
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CompositeState {
    pub opaque_tiles: Vec<CompositeTile>,
    pub alpha_tiles: Vec<CompositeTile>,
    pub clear_tiles: Vec<CompositeTile>,
    pub z_generator: ZBufferIdGenerator,
    // If false, we can't rely on the dirty rects in the CompositeTile
    // instances. This currently occurs during a scroll event, as a
    // signal to refresh the whole screen. This is only a temporary
    // measure until we integrate with OS compositors. In the meantime
    // it gives us the ability to partial present for any non-scroll
    // case as a simple win (e.g. video, animation etc).
    pub dirty_rects_are_valid: bool,
    /// List of OS native surface create / destroy operations to
    /// apply when render occurs.
    pub native_surface_updates: Vec<NativeSurfaceOperation>,
    /// The kind of compositor for picture cache tiles (e.g. drawn by WR, or OS compositor)
    pub compositor_kind: CompositorKind,
    /// Picture caching may be disabled dynamically, based on debug flags, pinch zoom etc.
    pub picture_caching_is_enabled: bool,
}

impl CompositeState {
    /// Construct a new state for compositing picture tiles. This is created
    /// during each frame construction and passed to the renderer.
    pub fn new(
        compositor_kind: CompositorKind,
        mut picture_caching_is_enabled: bool,
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
            native_surface_updates: Vec::new(),
            compositor_kind,
            picture_caching_is_enabled,
        }
    }

    /// Queue up allocation of a new OS native compositor surface with the
    /// specified id and dimensions.
    pub fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        size: DeviceIntSize,
        is_opaque: bool,
    ) -> SurfaceTextureDescriptor {
        self.native_surface_updates.push(
            NativeSurfaceOperation {
                id,
                details: NativeSurfaceOperationDetails::CreateSurface {
                    size,
                    is_opaque,
                }
            }
        );

        SurfaceTextureDescriptor::NativeSurface {
            id,
            size,
        }
    }

    /// Queue up destruction of an existing native OS surface. This is used when
    /// a picture cache tile is dropped or resized.
    pub fn destroy_surface(
        &mut self,
        id: NativeSurfaceId,
    ) {
        self.native_surface_updates.push(
            NativeSurfaceOperation {
                id,
                details: NativeSurfaceOperationDetails::DestroySurface,
            }
        );
    }
}

/// An arbitrary identifier for a native (OS compositor) surface
#[repr(C)]
#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeSurfaceId(pub u64);

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
    /// The surface is allocated but not placed in the visual tree.
    fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        size: DeviceIntSize,
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
        id: NativeSurfaceId,
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
