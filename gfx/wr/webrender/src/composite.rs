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

/// Defines what code path WR is using to composite picture cache tiles.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CompositeMode {
    /// Let WR draw tiles via normal batching. This requires no special OS support.
    Draw,
    /// Use a native OS compositor to draw tiles. This requires clients to implement
    /// the Compositor trait, but can be significantly more power efficient on operating
    /// systems that support it.
    Native,
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
    /// The current mode for compositing picture cache tiles (e.g. drawn by WR, or OS compositor)
    pub composite_mode: CompositeMode,
}

impl CompositeState {
    /// Construct a new state for compositing picture tiles. This is created
    /// during each frame construction and passed to the renderer.
    pub fn new(composite_mode: CompositeMode) -> Self {
        CompositeState {
            opaque_tiles: Vec::new(),
            alpha_tiles: Vec::new(),
            clear_tiles: Vec::new(),
            z_generator: ZBufferIdGenerator::new(0),
            dirty_rects_are_valid: true,
            native_surface_updates: Vec::new(),
            composite_mode,
        }
    }

    /// Queue up allocation of a new OS native compositor surface with the
    /// specified id and dimensions.
    pub fn create_surface(
        &mut self,
        id: NativeSurfaceId,
        size: DeviceIntSize,
    ) -> SurfaceTextureDescriptor {
        debug_assert_eq!(self.composite_mode, CompositeMode::Native);

        self.native_surface_updates.push(
            NativeSurfaceOperation {
                id,
                details: NativeSurfaceOperationDetails::CreateSurface {
                    size,
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
        debug_assert_eq!(self.composite_mode, CompositeMode::Native);

        self.native_surface_updates.push(
            NativeSurfaceOperation {
                id,
                details: NativeSurfaceOperationDetails::DestroySurface,
            }
        );
    }
}

/// An arbitrary identifier for a native (OS compositor) surface
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NativeSurfaceId(pub u64);

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
    );

    /// Destroy the surface with the specified id.
    fn destroy_surface(
        &mut self,
        id: NativeSurfaceId,
    );

    /// Bind this surface such that WR can issue OpenGL commands
    /// that will target the surface. Returns an (x, y) offset
    /// where WR should draw into the surface. This can be set
    /// to (0, 0) if the OS doesn't use texture atlases.
    fn bind(
        &mut self,
        id: NativeSurfaceId,
    ) -> DeviceIntPoint;

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
